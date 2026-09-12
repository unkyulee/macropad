#include "Config.h"
#include "app/app.h"
#include "app/FileSystem/FileSystem.h"

#define CONFIG_FILE "/config.json"

static JsonDocument _config;
static SemaphoreHandle_t _lock = nullptr;

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

bool config_load()
{
    config_lock();

    bool loaded = false;

    if (fs_ready() && gfs()->exists(CONFIG_FILE))
    {
        File file = gfs()->open(CONFIG_FILE, "r");
        if (file)
        {
            DeserializationError error = deserializeJson(_config, file);
            file.close();

            if (error)
                _log("config.json is not valid JSON: %s\n", error.c_str());
            else
            {
                _log("Config loaded\n");
                loaded = true;
            }
        }
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

bool config_save()
{
    if (!fs_ready())
    {
        _log("Cannot save config, filesystem not mounted\n");
        return false;
    }

    config_lock();

    bool ok = false;
    File file = gfs()->open(CONFIG_FILE, "w");
    if (file)
    {
        ok = serializeJsonPretty(_config, file) > 0;
        file.close();
    }

    config_unlock();

    if (ok)
        _log("Config saved\n");
    else
        _log("Failed to write %s\n", CONFIG_FILE);

    return ok;
}

void config_setup()
{
    _lock = xSemaphoreCreateMutex();
    config_load();
}
