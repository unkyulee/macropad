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
        screen["enabled"] = (i == 0); // one screen on, the rest waiting
        screen["type"] = (i == 0) ? SCREEN_CLOCK : SCREEN_KEYMAP;
        screen["tz"] = "UTC0";
        screen["tzName"] = "UTC";
        screen["file"] = "";

        // F13-F24 are unused by every OS, which makes them the safe default
        // for a macro pad: bind them to whatever you like on the host side.
        static const char *defaults[KEY_COUNT] = {
            "F13", "F14", "F15", "F16",
            "F17", "F18", "F19", "F20",
            "F21", "F22", "F23", "",
            "CTRL+C", "CTRL+V", "CTRL+Z", "CTRL+SHIFT+Z",
            "MUTE", "", "PLAY", ""};

        JsonArray keys = screen["keys"].to<JsonArray>();
        for (int k = 0; k < KEY_COUNT; k++)
            keys.add(defaults[k]);
    }
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
