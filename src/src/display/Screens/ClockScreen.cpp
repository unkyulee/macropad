#include "ClockScreen.h"
#include "display/display.h"
#include "app/app.h"
#include "app/Config/Config.h"

#include "service/Time/Ntp.h"

#include <time.h>

// What was last painted. File scope, not render() locals, so setup() can
// clear them: re-entering the screen wipes the panel, and a cache that
// still matched the new value would skip the repaint and leave it blank.
static char _lastTime[6] = "";
static int _lastSynced = -1;
static int _lastPairing = -1;
static int _lastAp = -1;
static char _lastBleName[32] = "";

// Guide lines under the digits, shown until a host has paired. The clock
// is the first screen on a fresh pad, so this is where pairing is taught.
static void draw_pairing(bool needsPairing, const char *name)
{
    TFT_eSPI &tft = display_tft();

    int bottom = display_body_bottom();
    int top = bottom - 46;

    tft.fillRect(0, top, tft.width(), bottom - top, TFT_BLACK);
    if (!needsPairing)
        return;

    tft.setTextDatum(TC_DATUM);
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.drawString(String("Pair Bluetooth: \"") + name + "\"", tft.width() / 2, top + 4, 2);
    tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
    tft.drawString("Add it in your computer's Bluetooth menu", tft.width() / 2, top + 24, 2);
    tft.setTextDatum(TL_DATUM);
}

// One dim line above the digits while the pad has no network, so the
// dashes are explained without holding the clock back for WiFi setup.
static void draw_offline(bool apMode, const char *apName)
{
    TFT_eSPI &tft = display_tft();

    tft.fillRect(0, BODY_TOP, tft.width(), 20, TFT_BLACK);
    if (!apMode)
        return;

    tft.setTextDatum(TC_DATUM);
    tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
    tft.drawString(String("Set time: join WiFi ") + apName, tft.width() / 2, BODY_TOP + 2, 2);
    tft.setTextDatum(TL_DATUM);
}

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

    _lastTime[0] = 0;
    _lastSynced = -1;
    _lastPairing = -1;
    _lastAp = -1;
    _lastBleName[0] = 0;

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
    AppStatus &app = status();

    bool needsPairing = app.bleNeedsPairing;
    if ((int)needsPairing != _lastPairing || strcmp(_lastBleName, app.bleName) != 0)
    {
        _lastPairing = (int)needsPairing;
        strlcpy(_lastBleName, app.bleName, sizeof(_lastBleName));
        draw_pairing(needsPairing, app.bleName);
    }

    // the hint only matters until the time is real
    bool showAp = app.apMode && !ntp_synced();
    if ((int)showAp != _lastAp)
    {
        _lastAp = (int)showAp;
        draw_offline(showAp, app.apName);
    }

    time_t now = time(nullptr);
    struct tm local;
    localtime_r(&now, &local);

    // before the first NTP reply the clock sits in 1970, so show placeholder
    // dashes rather than a confidently wrong time
    bool synced = ntp_synced();

    // the digits are dimmed until the time is real, so the moment the first
    // reply lands it has to be repainted even if the text happens to match
    bool justSynced = (_lastSynced != (int)synced);
    _lastSynced = (int)synced;

    char timeText[6];
    if (synced)
        strftime(timeText, sizeof(timeText), "%H:%M", &local);
    else
        strlcpy(timeText, "--:--", sizeof(timeText));

    // nothing changes for a whole minute at a time
    if (!justSynced && strcmp(timeText, _lastTime) == 0)
        return;

    strlcpy(_lastTime, timeText, sizeof(_lastTime));

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
