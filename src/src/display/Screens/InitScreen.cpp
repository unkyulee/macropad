#include "InitScreen.h"
#include "display/display.h"
#include "app/app.h"

// Shown while the network task works through the saved networks. Once a
// connection is up (or the access point is running) display.cpp moves on
// to the first configured screen - except in AP mode, where this screen
// stays put because it is holding the instructions the user needs.
void InitScreen_setup(int slot)
{
    (void)slot;

    TFT_eSPI &tft = display_tft();

    display_header("STARTING UP");
    display_clear_body();

    tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
    tft.drawString("Macro Pad " VERSION, 20, BODY_TOP + 8, 2);
}

void InitScreen_render(int slot)
{
    (void)slot;

    TFT_eSPI &tft = display_tft();
    AppStatus &app = status();

    static bool wasAp = false;
    static char lastMessage[sizeof(app.wifiMessage)] = "";

    // repaint the whole body when switching into AP mode, the layouts
    // have nothing in common
    if (app.apMode != wasAp)
    {
        wasAp = app.apMode;
        lastMessage[0] = 0;
        display_clear_body();
    }

    if (app.apMode)
    {
        tft.setTextColor(TFT_ORANGE, TFT_BLACK);
        tft.drawString("No known network", 20, BODY_TOP + 6, 4);

        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.drawString("Join this WiFi:", 20, BODY_TOP + 44, 2);
        tft.drawString(app.apName, 20, BODY_TOP + 62, 4);

        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.drawString("Then open:", 20, BODY_TOP + 104, 2);
        tft.setTextColor(TFT_CYAN, TFT_BLACK);
        tft.drawString(String("http://") + app.ip, 20, BODY_TOP + 122, 4);

        tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
        tft.drawString("Restarts hourly until connected", 20, BODY_TOP + 164, 2);
        return;
    }

    // connecting: only the status line changes
    if (strcmp(lastMessage, app.wifiMessage) != 0)
    {
        strlcpy(lastMessage, app.wifiMessage, sizeof(lastMessage));

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
