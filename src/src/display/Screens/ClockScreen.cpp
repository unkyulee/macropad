#include "ClockScreen.h"
#include "display/display.h"
#include "app/app.h"
#include "app/Config/Config.h"

#include <time.h>

static bool _ntpStarted = false;

// The POSIX TZ string carries the offset and the daylight saving rules,
// so the pad needs no timezone database and survives a DST change on its
// own. The web UI writes it as "tz", with "tzName" only for display.
void ClockScreen_setup(int slot)
{
    TFT_eSPI &tft = display_tft();

    config_lock();
    String tz = config()["screens"][slot]["tz"].as<String>();
    String tzName = config()["screens"][slot]["tzName"].as<String>();
    config_unlock();

    if (tz.length() == 0)
        tz = "UTC0";

    // NTP only needs starting once, but the zone can change per screen
    if (!_ntpStarted && status().wifiConnected)
    {
        configTzTime(tz.c_str(), "pool.ntp.org", "time.nist.gov");
        _ntpStarted = true;
        _log("NTP started, TZ=%s\n", tz.c_str());
    }
    else
    {
        setenv("TZ", tz.c_str(), 1);
        tzset();
    }

    display_header(tzName.length() ? tzName.c_str() : "CLOCK");
    display_clear_body();

    tft.setTextDatum(TL_DATUM);
}

void ClockScreen_render(int slot)
{
    (void)slot;

    TFT_eSPI &tft = display_tft();

    static char lastTime[6] = "";
    static char lastDate[24] = "";

    time_t now = time(nullptr);
    struct tm local;
    localtime_r(&now, &local);

    // before the first NTP reply the clock sits in 1970
    bool synced = (now > 1700000000);

    char timeText[6];
    strftime(timeText, sizeof(timeText), "%H:%M", &local);

    char dateText[24];
    strftime(dateText, sizeof(dateText), "%a %d %b %Y", &local);

    if (strcmp(timeText, lastTime) != 0)
    {
        strlcpy(lastTime, timeText, sizeof(lastTime));

        tft.setTextColor(synced ? TFT_WHITE : TFT_DARKGREY, TFT_BLACK);
        tft.setTextPadding(tft.width());
        // font 7 is the seven segment face, large enough to read across a desk
        tft.drawString(timeText, 24, BODY_TOP + 34, 7);
        tft.setTextPadding(0);
    }

    if (strcmp(dateText, lastDate) != 0)
    {
        strlcpy(lastDate, dateText, sizeof(lastDate));

        tft.setTextColor(TFT_CYAN, TFT_BLACK);
        tft.setTextPadding(tft.width() - 48);
        tft.drawString(synced ? dateText : "waiting for time", 24, BODY_TOP + 122, 4);
        tft.setTextPadding(0);
    }

    // seconds tick quietly under the date
    static int lastSecond = -1;
    if (local.tm_sec != lastSecond)
    {
        lastSecond = local.tm_sec;

        char seconds[4];
        snprintf(seconds, sizeof(seconds), ":%02d", local.tm_sec);

        tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
        tft.setTextPadding(60);
        tft.drawString(seconds, 232, BODY_TOP + 70, 4);
        tft.setTextPadding(0);
    }
}

bool ClockScreen_key(int index, bool pressed)
{
    (void)index;
    (void)pressed;

    // keys stay bound to the host on the clock screen
    return false;
}
