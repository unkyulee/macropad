#include "display.h"
#include "app/app.h"
#include "app/Config/Config.h"

#include "Screens/Screen.h"
#include "Screens/InitScreen.h"
#include "Screens/ClockScreen.h"
#include "Screens/KeymapScreen.h"
#include "Screens/GifScreen.h"
#include "Screens/CalculatorScreen.h"
#include "keyboard/BLE/BLEKeypad.h"

#include <SPI.h>

// pins are defined in platformio.ini
static TFT_eSPI tft = TFT_eSPI();

static const Screen INIT_SCREEN = {InitScreen_setup, InitScreen_render, InitScreen_key};
static const Screen CLOCK_SCREEN = {ClockScreen_setup, ClockScreen_render, ClockScreen_key};
static const Screen KEYMAP_SCREEN = {KeymapScreen_setup, KeymapScreen_render, KeymapScreen_key};
static const Screen GIF_SCREEN = {GifScreen_setup, GifScreen_render, GifScreen_key};
static const Screen CALC_SCREEN = {CalculatorScreen_setup, CalculatorScreen_render, CalculatorScreen_key};

static const Screen *_active = &INIT_SCREEN;
static int _slot = -1;

TFT_eSPI &display_tft()
{
    return tft;
}

// Shared chrome so a screen change never leaves half the old layout on
// the panel: header strip on top, everything below it belongs to the screen.
void display_header(const char *title)
{
    tft.fillRect(0, 0, tft.width(), BODY_TOP - 2, TFT_NAVY);
    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(TFT_WHITE, TFT_NAVY);
    tft.drawString(title, 8, 7, 2);
}

void display_clear_body()
{
    tft.fillRect(0, BODY_TOP - 2, tft.width(), tft.height() - BODY_TOP + 2, TFT_BLACK);
    tft.setTextDatum(TL_DATUM);
    tft.setTextPadding(0);
}

// Status strip drawn into the right hand side of the header, so every
// screen shows the link state without having to draw it itself.
static void display_status_strip()
{
    AppStatus &app = status();

    static bool lastWifi = false;
    static bool lastAp = false;
    static bool lastBle = false;

    if (app.wifiConnected == lastWifi && app.apMode == lastAp && app.bleConnected == lastBle)
        return;

    lastWifi = app.wifiConnected;
    lastAp = app.apMode;
    lastBle = app.bleConnected;

    tft.setTextDatum(TR_DATUM);
    tft.setTextPadding(150);

    uint16_t wifiColour = app.apMode ? TFT_ORANGE : (app.wifiConnected ? TFT_GREEN : TFT_DARKGREY);
    String text = String(app.apMode ? "AP" : (app.wifiConnected ? "WIFI" : "----"));
    text += app.bleConnected ? "  BLE" : "  ---";

    tft.setTextColor(wifiColour, TFT_NAVY);
    tft.drawString(text, tft.width() - 8, 7, 2);

    tft.setTextPadding(0);
    tft.setTextDatum(TL_DATUM);
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
    }
    else
    {
        config_lock();
        String type = config()["screens"][slot]["type"].as<String>();
        config_unlock();

        _active = screen_for(type);
        _log("Screen %d: %s\n", slot, type.c_str());
    }

    _active->setup(slot);

    // the keymap belongs to the screen, so the BLE bindings change with it
    ble_reload();

    // force the header strip to repaint over the new chrome
    status().dirty = true;
}

void display_next_screen()
{
    // walk forward to the next enabled slot, wrapping around
    for (int step = 1; step <= SCREEN_COUNT; step++)
    {
        int candidate = (_slot + step) % SCREEN_COUNT;
        if (candidate < 0)
            candidate += SCREEN_COUNT;

        if (slot_enabled(candidate))
        {
            activate(candidate);
            return;
        }
    }

    // nothing is enabled: keep whatever is on screen, but if we are still
    // on the init screen there is nothing to fall back to
    if (_slot < 0)
    {
        display_header("NO SCREENS");
        display_clear_body();
        tft.setTextColor(TFT_ORANGE, TFT_BLACK);
        tft.drawString("No screen is enabled", 20, BODY_TOP + 40, 4);
        tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
        tft.drawString("Enable one from the web interface", 20, BODY_TOP + 80, 2);
    }
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
    tft.setRotation(3);
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
        display_status_strip();
    }

    _active->render(_slot);
}

bool display_key(int index, bool pressed)
{
    return _active->key(index, pressed);
}
