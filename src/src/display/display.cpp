#include "display.h"
#include "app/app.h"
#include "app/Config/Config.h"

#include "Screens/Screen.h"
#include "Screens/InitScreen.h"
#include "Screens/ClockScreen.h"
#include "Screens/KeymapScreen.h"
#include "Screens/GifScreen.h"
#include "Screens/CalculatorScreen.h"
#include "Screens/InfoScreen.h"
#include "keyboard/BLE/BLEKeypad.h"

#include <SPI.h>

// pins are defined in platformio.ini
static TFT_eSPI tft = TFT_eSPI();

static const Screen INIT_SCREEN = {InitScreen_setup, InitScreen_render, InitScreen_key};
static const Screen CLOCK_SCREEN = {ClockScreen_setup, ClockScreen_render, ClockScreen_key};
static const Screen KEYMAP_SCREEN = {KeymapScreen_setup, KeymapScreen_render, KeymapScreen_key};
static const Screen GIF_SCREEN = {GifScreen_setup, GifScreen_render, GifScreen_key};
static const Screen CALC_SCREEN = {CalculatorScreen_setup, CalculatorScreen_render, CalculatorScreen_key};
static const Screen INFO_SCREEN = {InfoScreen_setup, InfoScreen_render, InfoScreen_key};

// Virtual slot after the configured screens: the knob button lands here
// once it has been through every enabled screen. Always available, so
// the network details stay reachable even with no screen enabled.
#define INFO_SLOT SCREEN_COUNT
#define CYCLE_LENGTH (SCREEN_COUNT + 1)

static const Screen *_active = &INIT_SCREEN;
static int _slot = -1;

TFT_eSPI &display_tft()
{
    return tft;
}

// Name of the active screen, from config, so you can tell at a glance
// which of the five you are looking at.
static char _title[24] = "";

// rule colour, a very dark grey
#define FOOTER_RULE 0x2124

int display_body_bottom()
{
    return tft.height() - FOOTER_HEIGHT;
}

void display_clear_body()
{
    tft.fillRect(0, 0, tft.width(), display_body_bottom(), TFT_BLACK);
    tft.setTextDatum(TL_DATUM);
    tft.setTextPadding(0);
}

// Screen name on the left, link state on the right, a single rule above.
// Drawn here rather than by each screen so they all match.
static void display_footer()
{
    AppStatus &app = status();

    int top = display_body_bottom();

    tft.fillRect(0, top, tft.width(), FOOTER_HEIGHT, TFT_BLACK);
    tft.drawFastHLine(0, top, tft.width(), FOOTER_RULE);

    int textY = top + 6;

    tft.setTextPadding(0);
    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(TFT_SILVER, TFT_BLACK);
    tft.drawString(_title, 8, textY, 1);

    // Right hand side: lock state and the two links, each in its own colour,
    // so they cannot go in one drawString. Widths are measured and the block
    // is right aligned, which keeps it tidy whatever the labels say.
    struct Segment
    {
        const char *text;
        uint16_t colour;
    };

    Segment segments[] = {
        {"NUM", app.numLock ? TFT_GREEN : TFT_DARKGREY},
        {"CAPS", app.capsLock ? TFT_GREEN : TFT_DARKGREY},
        {app.apMode ? "AP" : "WIFI",
         app.apMode ? TFT_ORANGE : (app.wifiConnected ? TFT_GREEN : TFT_DARKGREY)},
        {"BLE", app.bleConnected ? TFT_GREEN : TFT_DARKGREY},
    };

    const int gap = 8;

    int total = -gap;
    for (const Segment &segment : segments)
        total += tft.textWidth(segment.text, 1) + gap;

    int x = tft.width() - 8 - total;
    for (const Segment &segment : segments)
    {
        tft.setTextColor(segment.colour, TFT_BLACK);
        tft.drawString(segment.text, x, textY, 1);
        x += tft.textWidth(segment.text, 1) + gap;
    }
}

static const Screen *screen_for(const String &type)
{
    if (type == SCREEN_CLOCK)
        return &CLOCK_SCREEN;
    if (type == SCREEN_GIF)
        return &GIF_SCREEN;
    if (type == SCREEN_CALCULATOR)
        return &CALC_SCREEN;
    return &KEYMAP_SCREEN;
}

static bool slot_enabled(int slot)
{
    if (slot == INFO_SLOT)
        return true; // not configurable, never skipped

    if (slot < 0 || slot >= SCREEN_COUNT)
        return false;

    config_lock();
    bool enabled = config()["screens"][slot]["enabled"].as<bool>();
    config_unlock();
    return enabled;
}

static void activate(int slot)
{
    // GIF holds a file handle, so it has to be told to let go before the
    // next screen takes over the panel
    GifScreen_close();

    _slot = slot;
    status().screen = slot;

    if (slot < 0)
    {
        _active = &INIT_SCREEN;
        strlcpy(_title, "STARTING UP", sizeof(_title));
    }
    else if (slot == INFO_SLOT)
    {
        _active = &INFO_SCREEN;
        strlcpy(_title, "info", sizeof(_title));
    }
    else
    {
        config_lock();
        String type = config()["screens"][slot]["type"].as<String>();
        String name = config()["screens"][slot]["name"].as<String>();
        config_unlock();

        _active = screen_for(type);

        // the name is what identifies the screen, the type is only a
        // fallback for a slot that was never named
        name.trim();
        strlcpy(_title, name.length() ? name.c_str() : type.c_str(), sizeof(_title));

        _log("Screen %d: %s (%s)\n", slot, _title, type.c_str());
    }

    _active->setup(slot);
    display_footer();

    // the keymap belongs to the screen, so the BLE bindings change with it
    ble_reload();

    // make sure the footer picks up the current link state
    status().dirty = true;
}

void display_next_screen()
{
    // walk forward to the next enabled slot, wrapping around through the
    // info screen that sits at the end of the cycle
    for (int step = 1; step <= CYCLE_LENGTH; step++)
    {
        int candidate = (_slot + step) % CYCLE_LENGTH;
        if (candidate < 0)
            candidate += CYCLE_LENGTH;

        if (slot_enabled(candidate))
        {
            activate(candidate);
            return;
        }
    }

    // unreachable: the info slot is always enabled, so the loop above
    // always finds somewhere to go even with every screen turned off
}

void display_reload()
{
    // re-enter the current slot so it picks up its new settings, or find
    // the first enabled one if the current slot was just turned off
    if (_slot >= 0 && slot_enabled(_slot))
        activate(_slot);
    else
    {
        _slot = -1;
        display_next_screen();
    }
}

void display_setup()
{
    _log("Display setup\n");

    tft.begin();
    tft.setRotation(1);
    tft.fillScreen(TFT_BLACK);

    activate(-1);
}

void display_loop()
{
    AppStatus &app = status();

    // leave the init screen once the network task has settled. In AP mode
    // it stays: the pairing instructions are the useful thing to show.
    if (app.booting && !app.apMode && app.wifiConnected)
    {
        app.booting = false;
        display_next_screen();
    }

    if (app.dirty)
    {
        app.dirty = false;
        display_footer();
    }

    _active->render(_slot);
}

bool display_key(int index, bool pressed)
{
    return _active->key(index, pressed);
}
