#include "BLEKeypad.h"
#include "app/app.h"
#include "app/Config/Config.h"

#include <BleKeyboard.h>

// BleKeyboard.h has no Num Lock constant. Its non-printing keys are
// encoded as the HID usage plus 136 (see BleKeyboard::press), and Num Lock
// is usage 0x53, so 0xDB - which lands immediately before the library's
// own KEY_NUM_SLASH (0xDC, usage 0x54), matching the HID table order.
#ifndef KEY_NUM_LOCK
#define KEY_NUM_LOCK 0xDB
#endif

static BleKeyboard bleKeyboard;
static bool _enabled = false;

// A resolved key binding. Either a normal HID key with optional modifiers,
// or a media key - the two take different report paths in BleKeyboard.
struct KeyAction
{
    uint8_t modifiers[4] = {0, 0, 0, 0};
    uint8_t modifierCount = 0;
    uint8_t key = 0;             // 0 when unbound or when media is set
    const uint8_t *media = NULL; // MediaKeyReport, 2 bytes
};

static KeyAction _keys[KEY_COUNT];
static KeyAction _knobCW;
static KeyAction _knobCCW;

// name -> HID key, for everything that is not a single character
struct NamedKey
{
    const char *name;
    uint8_t key;
};

// prettier-ignore
static const NamedKey namedKeys[] = {
    {"F1", KEY_F1},   {"F2", KEY_F2},   {"F3", KEY_F3},   {"F4", KEY_F4},
    {"F5", KEY_F5},   {"F6", KEY_F6},   {"F7", KEY_F7},   {"F8", KEY_F8},
    {"F9", KEY_F9},   {"F10", KEY_F10}, {"F11", KEY_F11}, {"F12", KEY_F12},
    {"F13", KEY_F13}, {"F14", KEY_F14}, {"F15", KEY_F15}, {"F16", KEY_F16},
    {"F17", KEY_F17}, {"F18", KEY_F18}, {"F19", KEY_F19}, {"F20", KEY_F20},
    {"F21", KEY_F21}, {"F22", KEY_F22}, {"F23", KEY_F23}, {"F24", KEY_F24},
    {"ESC", KEY_ESC},             {"TAB", KEY_TAB},
    {"ENTER", KEY_RETURN},        {"RETURN", KEY_RETURN},
    {"SPACE", ' '},               {"BACKSPACE", KEY_BACKSPACE},
    {"DELETE", KEY_DELETE},       {"INSERT", KEY_INSERT},
    {"HOME", KEY_HOME},           {"END", KEY_END},
    {"PGUP", KEY_PAGE_UP},        {"PGDN", KEY_PAGE_DOWN},
    {"UP", KEY_UP_ARROW},         {"DOWN", KEY_DOWN_ARROW},
    {"LEFT", KEY_LEFT_ARROW},     {"RIGHT", KEY_RIGHT_ARROW},
    {"CAPSLOCK", KEY_CAPS_LOCK},  {"PRTSC", KEY_PRTSC},

    // The numeric keypad sends its own HID codes, distinct from the number
    // row: NUM_1 is not '1'. The difference matters - Alt plus a sequence
    // of numpad digits is how Windows enters characters by code point, and
    // that only works with these, never with the digits above the letters.
    {"NUM_LOCK", KEY_NUM_LOCK},
    {"NUM_0", KEY_NUM_0}, {"NUM_1", KEY_NUM_1}, {"NUM_2", KEY_NUM_2},
    {"NUM_3", KEY_NUM_3}, {"NUM_4", KEY_NUM_4}, {"NUM_5", KEY_NUM_5},
    {"NUM_6", KEY_NUM_6}, {"NUM_7", KEY_NUM_7}, {"NUM_8", KEY_NUM_8},
    {"NUM_9", KEY_NUM_9},
    {"NUM_SLASH", KEY_NUM_SLASH},       {"NUM_ASTERISK", KEY_NUM_ASTERISK},
    {"NUM_MINUS", KEY_NUM_MINUS},       {"NUM_PLUS", KEY_NUM_PLUS},
    {"NUM_ENTER", KEY_NUM_ENTER},       {"NUM_PERIOD", KEY_NUM_PERIOD},
};

// name -> media key report
struct NamedMedia
{
    const char *name;
    const uint8_t *report;
};

// prettier-ignore
static const NamedMedia namedMedia[] = {
    {"VOL_UP", KEY_MEDIA_VOLUME_UP},
    {"VOL_DOWN", KEY_MEDIA_VOLUME_DOWN},
    {"MUTE", KEY_MEDIA_MUTE},
    {"PLAY", KEY_MEDIA_PLAY_PAUSE},
    {"STOP", KEY_MEDIA_STOP},
    {"NEXT", KEY_MEDIA_NEXT_TRACK},
    {"PREV", KEY_MEDIA_PREVIOUS_TRACK},
    {"CALC", KEY_MEDIA_CALCULATOR},
    {"HOMEPAGE", KEY_MEDIA_WWW_HOME},
};

static uint8_t parse_modifier(const String &token)
{
    if (token == "CTRL" || token == "LEFT_CTRL")
        return KEY_LEFT_CTRL;
    if (token == "SHIFT" || token == "LEFT_SHIFT")
        return KEY_LEFT_SHIFT;
    if (token == "ALT" || token == "LEFT_ALT")
        return KEY_LEFT_ALT;
    if (token == "GUI" || token == "WIN" || token == "CMD")
        return KEY_LEFT_GUI;
    if (token == "RIGHT_CTRL")
        return KEY_RIGHT_CTRL;
    if (token == "RIGHT_SHIFT")
        return KEY_RIGHT_SHIFT;
    if (token == "RIGHT_ALT" || token == "ALTGR")
        return KEY_RIGHT_ALT;
    if (token == "RIGHT_GUI")
        return KEY_RIGHT_GUI;
    return 0;
}

// "CTRL+SHIFT+P", "F13", "VOL_UP", "a" - an empty string leaves the key
// unbound, which is how the web UI disables a key.
static KeyAction parse_action(String action)
{
    KeyAction result;

    action.trim();
    if (action.length() == 0)
        return result;

    while (action.length() > 0)
    {
        int plus = action.indexOf('+');

        // a trailing lone '+' is the plus character itself, not a separator
        String token = (plus > 0) ? action.substring(0, plus) : action;
        action = (plus > 0) ? action.substring(plus + 1) : String("");
        token.trim();
        if (token.length() == 0)
            continue;

        // modifiers accumulate, anything else terminates the binding
        String upper = token;
        upper.toUpperCase();

        uint8_t modifier = parse_modifier(upper);
        if (modifier != 0 && action.length() > 0)
        {
            if (result.modifierCount < 4)
                result.modifiers[result.modifierCount++] = modifier;
            continue;
        }

        // a bare modifier with nothing after it is still a usable binding
        if (modifier != 0)
        {
            result.key = modifier;
            return result;
        }

        if (token.length() == 1)
        {
            result.key = (uint8_t)token.charAt(0);
            return result;
        }

        for (const NamedKey &named : namedKeys)
        {
            if (upper == named.name)
            {
                result.key = named.key;
                return result;
            }
        }

        for (const NamedMedia &named : namedMedia)
        {
            if (upper == named.name)
            {
                result.media = named.report;
                return result;
            }
        }

        _log("Unknown key action '%s'\n", token.c_str());
        return result;
    }

    return result;
}

// Each screen carries its own keymap, so this has to run again on every
// screen change as well as after a config edit.
void ble_reload()
{
    int slot = status().screen;

    config_lock();

    JsonDocument &cfg = config();

    // The init screen and the info screen have no keymap of their own, and
    // the init screen stays up for as long as the pad is in access point
    // mode. Borrowing the first enabled screen's map keeps the keys working
    // while either of them is showing.
    if (slot < 0 || slot >= SCREEN_COUNT)
    {
        for (int i = 0; i < SCREEN_COUNT; i++)
        {
            if (cfg["screens"][i]["enabled"].as<bool>())
            {
                slot = i;
                break;
            }
        }
    }

    for (int i = 0; i < KEY_COUNT; i++)
    {
        if (slot < 0)
            _keys[i] = KeyAction();
        else
            _keys[i] = parse_action(cfg["screens"][slot]["keys"][i].as<String>());
    }

    _knobCW = parse_action(cfg["knob"]["cw"].as<String>());
    _knobCCW = parse_action(cfg["knob"]["ccw"].as<String>());

    config_unlock();

    _debug("[ble] keymap reloaded for screen %d\n", slot);
}

void ble_setup()
{
    config_lock();
    _enabled = config()["ble"]["enabled"].as<bool>();
    String name = config()["ble"]["name"].as<String>();
    config_unlock();

    ble_reload();

    if (!_enabled)
    {
        _log("BLE keyboard disabled by config\n");
        return;
    }

    if (name.length() == 0)
        name = "Macro Pad";

    bleKeyboard.setName(name.c_str());
    bleKeyboard.begin();

    _log("BLE keyboard advertising as '%s'\n", name.c_str());
}

bool ble_connected()
{
    return _enabled && bleKeyboard.isConnected();
}

void ble_loop()
{
    if (!_enabled)
        return;

    // surface link state on the display without polling it every frame
    static unsigned long last = 0;
    if (millis() - last < 500)
        return;
    last = millis();

    AppStatus &app = status();
    bool connected = bleKeyboard.isConnected();
    if (connected != app.bleConnected)
    {
        app.bleConnected = connected;
        app.dirty = true;
        _log("BLE %s\n", connected ? "connected" : "disconnected");
    }
}

static void send_action(const KeyAction &action, bool pressed)
{
    if (action.key == 0 && action.media == NULL)
        return; // unbound

    if (pressed)
    {
        for (uint8_t i = 0; i < action.modifierCount; i++)
            bleKeyboard.press(action.modifiers[i]);

        if (action.media != NULL)
            bleKeyboard.press(action.media);
        else
            bleKeyboard.press(action.key);
    }
    else
    {
        if (action.media != NULL)
            bleKeyboard.release(action.media);
        else
            bleKeyboard.release(action.key);

        // release modifiers last so the host sees them held for the key
        for (uint8_t i = 0; i < action.modifierCount; i++)
            bleKeyboard.release(action.modifiers[i]);
    }
}

void ble_key(int index, bool pressed)
{
    if (!ble_connected() || index < 0 || index >= KEY_COUNT)
        return;

    send_action(_keys[index], pressed);
}

void ble_knob(int direction)
{
    if (!ble_connected())
        return;

    // a detent is a discrete event, so press and release back to back
    const KeyAction &action = (direction > 0) ? _knobCW : _knobCCW;
    send_action(action, true);
    delay(5);
    send_action(action, false);
}
