#include "InfoScreen.h"
#include "display/display.h"
#include "app/app.h"

// Last stop in the knob button cycle: which network the pad is on and how
// to reach it. Not a configurable slot, so it has no keymap of its own.
#define ROW_LABEL_1 (BODY_TOP + 6)
#define ROW_VALUE_1 (BODY_TOP + 24)
#define ROW_LABEL_2 (BODY_TOP + 62)
#define ROW_VALUE_2 (BODY_TOP + 80)
#define ROW_LABEL_3 (BODY_TOP + 118)
#define ROW_VALUE_3 (BODY_TOP + 136)
#define ROW_VERSION (BODY_TOP + 178)

// See ClockScreen: these have to survive render() but be cleared by
// setup(), or a screen revisit paints nothing onto the cleared panel.
static char _lastNetwork[40] = "";
static char _lastAddress[24] = "";
static int _lastBle = -1;

static void draw_label(const char *text, int y)
{
    TFT_eSPI &tft = display_tft();

    tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
    tft.drawString(text, 20, y, 2);
}

void InfoScreen_setup(int slot)
{
    (void)slot;

    TFT_eSPI &tft = display_tft();

    _lastNetwork[0] = 0;
    _lastAddress[0] = 0;
    _lastBle = -1;

    display_clear_body();

    draw_label("NETWORK", ROW_LABEL_1);
    draw_label("ADDRESS", ROW_LABEL_2);
    draw_label("BLUETOOTH", ROW_LABEL_3);

    tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
    tft.drawString("Macro Pad " VERSION, 20, ROW_VERSION, 2);
}

void InfoScreen_render(int slot)
{
    (void)slot;

    TFT_eSPI &tft = display_tft();
    AppStatus &app = status();

    char network[40];
    if (app.apMode)
        snprintf(network, sizeof(network), "%s", app.apName);
    else if (app.wifiConnected)
        snprintf(network, sizeof(network), "%s", app.ssid);
    else
        snprintf(network, sizeof(network), "not connected");

    char address[24];
    if (app.wifiConnected || app.apMode)
        snprintf(address, sizeof(address), "http://%s", app.ip);
    else
        snprintf(address, sizeof(address), "-");

    tft.setTextDatum(TL_DATUM);

    if (strcmp(network, _lastNetwork) != 0)
    {
        strlcpy(_lastNetwork, network, sizeof(_lastNetwork));

        // orange while the pad is hosting its own network, since that means
        // it never reached one of the saved ones
        tft.setTextColor(app.apMode ? TFT_ORANGE : (app.wifiConnected ? TFT_WHITE : TFT_DARKGREY), TFT_BLACK);
        tft.setTextPadding(tft.width() - 40);
        tft.drawString(network, 20, ROW_VALUE_1, 4);
        tft.setTextPadding(0);
    }

    if (strcmp(address, _lastAddress) != 0)
    {
        strlcpy(_lastAddress, address, sizeof(_lastAddress));

        tft.setTextColor(TFT_CYAN, TFT_BLACK);
        tft.setTextPadding(tft.width() - 40);
        tft.drawString(address, 20, ROW_VALUE_2, 4);
        tft.setTextPadding(0);
    }

    if ((int)app.bleConnected != _lastBle)
    {
        _lastBle = (int)app.bleConnected;

        tft.setTextColor(app.bleConnected ? TFT_GREEN : TFT_DARKGREY, TFT_BLACK);
        tft.setTextPadding(tft.width() - 40);
        tft.drawString(app.bleConnected ? "paired" : "advertising", 20, ROW_VALUE_3, 4);
        tft.setTextPadding(0);
    }

    // lock state lives in the footer, which every screen shares
}

bool InfoScreen_key(int index, bool pressed)
{
    (void)index;
    (void)pressed;

    // informational only, keys keep their host bindings
    return false;
}
