#include "ClockScreen.h"
#include "display/display.h"
#include "app/app.h"
#include "app/Config/Config.h"

#include "service/Time/Ntp.h"

#include <time.h>

// The POSIX TZ string carries the offset and the daylight saving rules,
// so the pad needs no timezone database and survives a DST change on its
// own. The web UI writes it as "tz", with "tzName" only for display.
//
// SNTP itself is started by the network layer the moment a connection
// comes up, so the clock is right whether this screen was opened before
// or after the pad got online.
void ClockScreen_setup(int slot)
{
    TFT_eSPI &tft = display_tft();

    config_lock();
    String tz = config()["screens"][slot]["tz"].as<String>();
    String tzName = config()["screens"][slot]["tzName"].as<String>();
    config_unlock();

    ntp_set_zone(tz.c_str());

    // covers the case where this screen is reached while already online
    if (status().wifiConnected)
        ntp_begin();

    // the footer carries the screen name, drawn by display.cpp
    (void)tzName;
    display_clear_body();

    tft.setTextDatum(TL_DATUM);
}

void ClockScreen_render(int slot)
{
    (void)slot;

    TFT_eSPI &tft = display_tft();

    static char lastTime[6] = "";

    time_t now = time(nullptr);
    struct tm local;
    localtime_r(&now, &local);

    // before the first NTP reply the clock sits in 1970, so show placeholder
    // dashes rather than a confidently wrong time
    bool synced = ntp_synced();

    // the digits are dimmed until the time is real, so the moment the first
    // reply lands it has to be repainted even if the text happens to match
    static int lastSynced = -1;
    bool justSynced = (lastSynced != (int)synced);
    lastSynced = (int)synced;

    char timeText[6];
    if (synced)
        strftime(timeText, sizeof(timeText), "%H:%M", &local);
    else
        strlcpy(timeText, "--:--", sizeof(timeText));

    // nothing changes for a whole minute at a time
    if (!justSynced && strcmp(timeText, lastTime) == 0)
        return;

    strlcpy(lastTime, timeText, sizeof(lastTime));

    // font 7 is the seven segment face, doubled up it fills the panel.
    // Measured rather than assumed, so a narrower display drops to single
    // size instead of running off the edge.
    tft.setTextSize(2);
    if (tft.textWidth(timeText, 7) > tft.width() - 8)
        tft.setTextSize(1);

    tft.setTextDatum(MC_DATUM);
    tft.setTextPadding(tft.width());
    tft.setTextColor(synced ? TFT_WHITE : TFT_DARKGREY, TFT_BLACK);
    tft.drawString(timeText, tft.width() / 2, display_body_bottom() / 2, 7);

    tft.setTextSize(1);
    tft.setTextPadding(0);
    tft.setTextDatum(TL_DATUM);
}

bool ClockScreen_key(int index, bool pressed)
{
    (void)index;
    (void)pressed;

    // keys stay bound to the host on the clock screen
    return false;
}
