#include "Config.h"
#include "app/app.h"
#include "app/FileSystem/FileSystem.h"
#include <stdarg.h>
#include <string.h>

#define CONFIG_FILE "/config.json"
#define CONFIG_TEMP "/config.tmp"
#define CONFIG_BACKUP "/config.bak"

static JsonDocument _config;
static SemaphoreHandle_t _lock = nullptr;
static char _saveError[160] = "";

const char *config_save_error()
{
    return _saveError;
}

static bool save_failed(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    vsnprintf(_saveError, sizeof(_saveError), format, args);
    va_end(args);
    _log("Config save failed: %s\n", _saveError);
    return false;
}

JsonDocument &config()
{
    return _config;
}

void config_lock()
{
    if (_lock)
        xSemaphoreTake(_lock, portMAX_DELAY);
}

void config_unlock()
{
    if (_lock)
        xSemaphoreGive(_lock);
}

// Factory configuration. Keys are named by action strings that
// keyboard/BLE/BLEKeypad.cpp resolves into HID reports, so the whole
// keymap is editable from the web UI without a firmware change.
static void config_defaults()
{
    _config.clear();

    _config["device"]["name"] = "MacroPad";

    // up to WIFI_COUNT networks, tried in order against whatever is in range
    _config["wifi"]["networks"].to<JsonArray>();

    _config["ble"]["enabled"] = true;
    _config["ble"]["name"] = "Macro Pad";

    // the knob push button cycles screens instead of sending a keystroke
    _config["general"]["screenKey"] = 11;

    _config["knob"]["cw"] = "VOL_UP";
    _config["knob"]["ccw"] = "VOL_DOWN";

    JsonArray screens = _config["screens"].to<JsonArray>();
    for (int i = 0; i < SCREEN_COUNT; i++)
    {
        JsonObject screen = screens.add<JsonObject>();
        screen["enabled"] = (i < 3);
        screen["type"] = (i == 0) ? SCREEN_CLOCK :
                         (i == 1) ? SCREEN_GIF :
                         (i == 2) ? SCREEN_CALCULATOR : SCREEN_KEYMAP;

        // shown in the footer, so you can tell which screen you are on
        char name[16];
        snprintf(name, sizeof(name), "Screen %d", i + 1);
        screen["name"] = (i == 0) ? "Numpad" :
                         (i == 1) ? "YouTube" :
                         (i == 2) ? "Calculator" : name;

        screen["tz"] = "UTC0";
        screen["tzName"] = "UTC";
        screen["file"] = (i == 1) ? "/gif/youtube.gif" : "";

        // Laid out like a real numeric keypad, which is what the 4x5 grid
        // physically resembles:
        //
        //   NumLk   /   *   -
        //     7     8   9   +
        //     4     5   6  (knob button)
        //     1     2   3  Enter
        //     0        (.)
        //
        // Key 11 stays empty because that is the knob button, which cycles
        // screens rather than sending anything. Keys 17 and 19 are empty
        // because the bottom row of the PCB is only populated at 16 and 18.
        static const char *defaults[KEY_COUNT] = {
            "NUM_LOCK", "NUM_SLASH", "NUM_ASTERISK", "NUM_MINUS",
            "NUM_7", "NUM_8", "NUM_9", "NUM_PLUS",
            "NUM_4", "NUM_5", "NUM_6", "",
            "NUM_1", "NUM_2", "NUM_3", "NUM_ENTER",
            "NUM_0", "", "NUM_PERIOD", ""};

        // 8/2 select the previous/next video; 4/6 seek by ten seconds.
        // Shift+P is supported by YouTube only within a playlist.
        static const char *youtube[KEY_COUNT] = {
            "ESC", "c", "m", "DOWN",
            "HOME", "SHIFT+p", "f", "UP",
            "j", "k", "l", "",
            "SHIFT+,", "SHIFT+n", "SHIFT+.", "f",
            "SPACE", "", "t", ""};

        JsonArray keys = screen["keys"].to<JsonArray>();
        for (int k = 0; k < KEY_COUNT; k++)
            keys.add((i == 1) ? youtube[k] : defaults[k]);
    }
}

void config_reset()
{
    config_lock();
    config_defaults();
    config_unlock();

    config_save();
    _log("Config reset to defaults\n");
}

static bool read_config(const char *path, JsonDocument &doc)
{
    File file = gfs()->open(path, "r");
    if (!file)
    {
        _log("Cannot open %s for reading\n", path);
        return false;
    }

    DeserializationError error = deserializeJson(doc, file);
    file.close();
    if (error || !doc.is<JsonObject>())
    {
        _log("Cannot load %s: %s\n", path, error ? error.c_str() : "expected an object");
        return false;
    }
    return true;
}

bool config_load()
{
    config_lock();

    bool loaded = false;

    if (fs_ready())
    {
        if (gfs()->exists(CONFIG_FILE))
            loaded = read_config(CONFIG_FILE, _config);
        if (!loaded && gfs()->exists(CONFIG_BACKUP))
        {
            loaded = read_config(CONFIG_BACKUP, _config);
            if (loaded)
                _log("Config recovered from backup\n");
        }
        if (loaded)
            _log("Config loaded (%u WiFi networks)\n", (unsigned)_config["wifi"]["networks"].size());
    }

    if (!loaded)
    {
        _log("Using default config\n");
        config_defaults();
    }

    config_unlock();

    // a missing or corrupt file is replaced so the next boot is clean
    if (!loaded)
        config_save();

    return loaded;
}

// Check the exact bytes sent to storage. Comparing parsed JSON instead can
// conflate storage failures with changes in numeric representation.
static bool verify_config_bytes(const String &payload)
{
    File file = gfs()->open(CONFIG_TEMP, "r");
    if (!file)
        return save_failed("Cannot reopen %s after writing", CONFIG_TEMP);

    size_t actualSize = file.size();
    if (actualSize != payload.length())
    {
        file.close();
        return save_failed("%s size: expected %u, read %u bytes",
                           CONFIG_TEMP, (unsigned)payload.length(), (unsigned)actualSize);
    }

    uint8_t buffer[256];
    size_t offset = 0;
    while (offset < payload.length())
    {
        size_t count = payload.length() - offset;
        if (count > sizeof(buffer))
            count = sizeof(buffer);
        size_t received = file.read(buffer, count);
        if (received != count)
        {
            file.close();
            return save_failed("%s read at %u: expected %u, got %u bytes",
                               CONFIG_TEMP, (unsigned)offset, (unsigned)count, (unsigned)received);
        }
        if (memcmp(buffer, payload.c_str() + offset, count) != 0)
        {
            size_t mismatch = 0;
            while (buffer[mismatch] == (uint8_t)payload[offset + mismatch])
                ++mismatch;
            file.close();
            // Report only the position, never credential contents.
            return save_failed("%s content mismatch at byte %u",
                               CONFIG_TEMP, (unsigned)(offset + mismatch));
        }
        offset += count;
    }
    file.close();
    return true;
}

// The caller holds the config lock. Keep the previous file until the new
// document has been written completely and checked by reading it back.
static bool write_config(const JsonDocument &doc)
{
    _saveError[0] = 0;
    if (!fs_ready())
        return save_failed("Filesystem not mounted");

    String payload;
    size_t expected = measureJsonPretty(doc);
    if (doc.overflowed() || !payload.reserve(expected) ||
        serializeJsonPretty(doc, payload) != expected)
        return save_failed("Could not serialize the complete configuration");

    _log("Saving config (%u bytes, %u WiFi networks)\n",
         (unsigned)expected, (unsigned)doc["wifi"]["networks"].size());
    File file = gfs()->open(CONFIG_TEMP, "w");
    if (!file)
        return save_failed("Cannot open %s for writing", CONFIG_TEMP);

    size_t written = file.write((const uint8_t *)payload.c_str(), payload.length());
    file.flush();
    file.close();

    if (written != expected)
    {
        gfs()->remove(CONFIG_TEMP);
        return save_failed("Short write: expected %u, wrote %u bytes",
                           (unsigned)expected, (unsigned)written);
    }
    if (!verify_config_bytes(payload))
    {
        gfs()->remove(CONFIG_TEMP);
        return false;
    }

    JsonDocument verified;
    if (gfs()->exists(CONFIG_FILE))
    {
        // Never replace a recovered backup with an unreadable primary file.
        if (read_config(CONFIG_FILE, verified))
        {
            if (gfs()->exists(CONFIG_BACKUP) && !gfs()->remove(CONFIG_BACKUP))
                return save_failed("Cannot remove previous backup");
            if (!gfs()->rename(CONFIG_FILE, CONFIG_BACKUP))
                return save_failed("Cannot move previous config to backup");
        }
        else if (!gfs()->remove(CONFIG_FILE))
            return save_failed("Cannot remove unreadable config");
    }

    if (!gfs()->rename(CONFIG_TEMP, CONFIG_FILE))
    {
        return save_failed("Cannot commit config; previous config kept in backup");
    }

    _log("Config saved and verified\n");
    return true;
}

bool config_save()
{
    config_lock();
    bool ok = write_config(_config);
    config_unlock();
    if (!ok)
        _log("Failed to save config\n");
    return ok;
}

bool config_apply(JsonDocument &candidate)
{
    config_lock();
    bool ok = write_config(candidate);
    if (ok)
        swap(_config, candidate);
    config_unlock();
    return ok;
}

void config_setup()
{
    _lock = xSemaphoreCreateMutex();
    config_load();
}
