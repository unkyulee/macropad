#include "display.h"
#include "app/app.h"

#include <SPI.h>
#include <TFT_eSPI.h>

// pins are defined in platformio.ini
static TFT_eSPI tft = TFT_eSPI();

// layout, landscape 320x240
#define HEADER_HEIGHT 32
#define LABEL_X 20
#define VALUE_X 130
#define ROW_KEY 52
#define ROW_STATE 88
#define ROW_KNOB 124
#define ROW_WIFI 160
#define ROW_BLE 196

static void display_row(const char *label, int y)
{
    tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
    tft.drawString(label, LABEL_X, y, 4);
}

//
void display_setup()
{
    _log("Display setup\n");

    tft.begin();
    tft.setRotation(3);
    tft.fillScreen(TFT_BLACK);

    // header
    tft.fillRect(0, 0, tft.width(), HEADER_HEIGHT, TFT_NAVY);
    tft.setTextColor(TFT_WHITE, TFT_NAVY);
    tft.drawString("MACRO PAD " VERSION, 8, 6, 2);

    // static labels, only the values are repainted afterwards
    display_row("KEY", ROW_KEY);
    display_row("STATE", ROW_STATE);
    display_row("KNOB", ROW_KNOB);
    display_row("WIFI", ROW_WIFI);
    display_row("BLE", ROW_BLE);

    // values are drawn over a fixed width block so the previous
    // text is erased without clearing the whole screen
    tft.setTextPadding(tft.width() - VALUE_X - LABEL_X);
}

//
void display_loop()
{
    AppStatus &app = status();

    // repaint only when an input or a service reported a change
    if (!app.dirty)
        return;
    app.dirty = false;

    char buffer[48];

    // KEY: matrix index and the character it maps to
    if (app.key < 0)
        snprintf(buffer, sizeof(buffer), "-");
    else if (isPrintable(app.label))
        snprintf(buffer, sizeof(buffer), "%d  '%c'", app.key, app.label);
    else
        snprintf(buffer, sizeof(buffer), "%d  0x%02X", app.key, app.label);

    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString(buffer, VALUE_X, ROW_KEY, 4);

    // STATE
    tft.setTextColor(app.pressed ? TFT_GREEN : TFT_DARKGREY, TFT_BLACK);
    tft.drawString(app.key < 0 ? "-" : (app.pressed ? "PRESSED" : "RELEASED"), VALUE_X, ROW_STATE, 4);

    // KNOB position and last direction
    snprintf(buffer, sizeof(buffer), "%ld  %s", app.knob,
             app.knobDelta > 0 ? "CW" : (app.knobDelta < 0 ? "CCW" : ""));
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.drawString(buffer, VALUE_X, ROW_KNOB, 4);

    // WIFI: the address to reach the config page on
    if (app.wifiConnected || app.apMode)
        snprintf(buffer, sizeof(buffer), "%s", app.ip);
    else
        snprintf(buffer, sizeof(buffer), "connecting");

    tft.setTextColor(app.apMode ? TFT_ORANGE : (app.wifiConnected ? TFT_GREEN : TFT_DARKGREY), TFT_BLACK);
    tft.drawString(buffer, VALUE_X, ROW_WIFI, 4);

    // BLE link state
    tft.setTextColor(app.bleConnected ? TFT_GREEN : TFT_DARKGREY, TFT_BLACK);
    tft.drawString(app.bleConnected ? "connected" : "advertising", VALUE_X, ROW_BLE, 4);
}
