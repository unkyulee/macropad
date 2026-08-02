#include "Wifi.h"
#include "app/app.h"
#include "app/Config/Config.h"

#include <WiFi.h>

// how long to wait for the configured network before falling back to AP
#define STA_TIMEOUT_MS 12000

// how often to retry the configured network while running as an AP
#define RETRY_INTERVAL_MS 60000

static void publish_state(bool connected, bool apMode, const IPAddress &ip, const char *ssid)
{
    AppStatus &app = status();

    app.wifiConnected = connected;
    app.apMode = apMode;
    strlcpy(app.ip, ip.toString().c_str(), sizeof(app.ip));
    strlcpy(app.ssid, ssid ? ssid : "", sizeof(app.ssid));
    app.dirty = true;
}

// Access point name is stable per device so a bookmarked address keeps
// working: MacroPad-<last two bytes of the MAC>.
static String ap_name()
{
    uint8_t mac[6];
    WiFi.macAddress(mac);

    char name[24];
    snprintf(name, sizeof(name), "MacroPad-%02X%02X", mac[4], mac[5]);
    return String(name);
}

static void start_ap()
{
    String name = ap_name();

    WiFi.mode(WIFI_AP);
    // open network on purpose: the pad has no screen keyboard to type a
    // password with, and the AP only exists to hand over credentials
    WiFi.softAP(name.c_str());

    _log("Access point '%s' at %s\n", name.c_str(), WiFi.softAPIP().toString().c_str());
    publish_state(false, true, WiFi.softAPIP(), name.c_str());
}

static bool start_sta()
{
    config_lock();
    String ssid = config()["wifi"]["ssid"].as<String>();
    String password = config()["wifi"]["password"].as<String>();
    config_unlock();

    if (ssid.length() == 0)
    {
        _log("No WiFi credentials configured\n");
        return false;
    }

    _log("Connecting to '%s'\n", ssid.c_str());

    WiFi.mode(WIFI_STA);
    WiFi.setSleep(true); // the pad is idle most of the time
    WiFi.begin(ssid.c_str(), password.c_str());

    unsigned long started = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - started < STA_TIMEOUT_MS)
        delay(100);

    if (WiFi.status() != WL_CONNECTED)
    {
        _log("Connection to '%s' failed\n", ssid.c_str());
        return false;
    }

    _log("Connected, IP %s\n", WiFi.localIP().toString().c_str());
    publish_state(true, false, WiFi.localIP(), ssid.c_str());
    return true;
}

void wifi_setup()
{
    WiFi.persistent(false);

    if (!start_sta())
        start_ap();
}

void wifi_reconnect()
{
    WiFi.disconnect(true);
    delay(100);

    if (!start_sta())
        start_ap();
}

void wifi_loop()
{
    static unsigned long lastCheck = 0;
    if (millis() - lastCheck < 2000)
        return;
    lastCheck = millis();

    AppStatus &app = status();

    if (app.apMode)
    {
        // periodically give the configured network another chance, so the
        // pad rejoins on its own once the router is back
        static unsigned long lastRetry = 0;
        if (millis() - lastRetry < RETRY_INTERVAL_MS)
            return;
        lastRetry = millis();

        config_lock();
        bool hasCredentials = config()["wifi"]["ssid"].as<String>().length() > 0;
        config_unlock();

        if (hasCredentials && start_sta())
            WiFi.softAPdisconnect(true);

        return;
    }

    // station mode: drop back to the access point if the link is lost, so
    // the config page never becomes unreachable
    bool connected = (WiFi.status() == WL_CONNECTED);
    if (connected != app.wifiConnected)
    {
        if (connected)
        {
            // copy first: publish_state writes into app.ssid
            char ssid[sizeof(app.ssid)];
            strlcpy(ssid, app.ssid, sizeof(ssid));
            publish_state(true, false, WiFi.localIP(), ssid);
        }
        else
        {
            _log("WiFi lost\n");
            start_ap();
        }
    }
}
