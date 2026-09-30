#include "InitScreen.h"
#include "display/display.h"
#include "app/app.h"

// Shown while the network task works through the saved networks. Once a
// connection is up, or the pad falls back to its access point, display.cpp
// moves on to the first configured screen. The AP details are on the info
// screen, so a fresh pad goes straight to the clock instead of waiting here.
// See ClockScreen: cleared by setup() so a revisit repaints.
static char _lastMessage[48] = "";

void InitScreen_setup(int slot)
{
    (void)slot;

    _lastMessage[0] = 0;

    TFT_eSPI &tft = display_tft();

    display_clear_body();

    tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
    tft.drawString("Macro Pad " VERSION, 20, BODY_TOP + 8, 2);
}

void InitScreen_render(int slot)
{
    (void)slot;

    TFT_eSPI &tft = display_tft();
    AppStatus &app = status();

    // connecting: only the status line changes
    if (strcmp(_lastMessage, app.wifiMessage) != 0)
    {
        strlcpy(_lastMessage, app.wifiMessage, sizeof(_lastMessage));

        tft.setTextPadding(tft.width() - 40);
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.drawString(app.wifiMessage, 20, BODY_TOP + 40, 4);
        tft.setTextPadding(0);
    }
}

bool InitScreen_key(int index, bool pressed)
{
    (void)index;
    (void)pressed;

    // nothing is bound while booting, and nothing should reach the host
    return true;
}
