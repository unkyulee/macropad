#include "Wifi.h"
#include "app/app.h"
#include "app/Config/Config.h"

#include <WiFi.h>

// how long to wait for one network before moving to the next
#define STA_TIMEOUT_MS 10000

// how often to retry the saved networks while running as an AP
#define RETRY_INTERVAL_MS 60000

// restart the device after this long with no connection, in case the
// radio or the router got itself into a state a reboot would clear
#define OFFLINE_RESTART_MS (60UL * 60UL * 1000UL)

static unsigned long _offlineSince = 0;

static void say(const char *message)
{
    AppStatus &app = status();
    strlcpy(app.wifiMessage, message, sizeof(app.wifiMessage));
    app.dirty = true;
    _log("WiFi: %s\n", message);
}

static void publish_state(bool connected, bool apMode, const IPAddress &ip, const char *ssid)
{
    AppStatus &app = status();

    app.wifiConnected = connected;
    app.apMode = apMode;
    strlcpy(app.ip, ip.toString().c_str(), sizeof(app.ip));
    if (ssid)
        strlcpy(app.ssid, ssid, sizeof(app.ssid));
    app.dirty = true;

    if (connected)
        _offlineSince = 0;
    else if (_offlineSince == 0)
        _offlineSince = millis();
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
    // open network on purpose: the pad has no keyboard to type a password
    // with, and the AP only exists to hand over credentials
    WiFi.softAP(name.c_str());

    AppStatus &app = status();
    strlcpy(app.apName, name.c_str(), sizeof(app.apName));

    _log("Access point '%s' at %s\n", name.c_str(), WiFi.softAPIP().toString().c_str());
    publish_state(false, true, WiFi.softAPIP(), "");
    say("access point");
}

static bool connect_to(const String &ssid, const String &password)
{
    char message[sizeof(status().wifiMessage)];
    snprintf(message, sizeof(message), "trying %s", ssid.c_str());
    say(message);

    WiFi.begin(ssid.c_str(), password.c_str());

    unsigned long started = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - started < STA_TIMEOUT_MS)
        delay(100);

    if (WiFi.status() != WL_CONNECTED)
    {
        WiFi.disconnect();
        return false;
    }

    _log("Connected to '%s', IP %s\n", ssid.c_str(), WiFi.localIP().toString().c_str());
    publish_state(true, false, WiFi.localIP(), ssid.c_str());
    say("connected");
    return true;
}

// Scan first, then try only the saved networks that are actually in range,
// in the order they appear in the config. Without the scan a list of ten
// networks would take almost two minutes of timeouts to walk through.
static bool start_sta()
{
    config_lock();
    JsonArray networks = config()["wifi"]["networks"].as<JsonArray>();
    int count = networks.size();
    if (count > WIFI_COUNT)
        count = WIFI_COUNT;

    String ssids[WIFI_COUNT];
    String passwords[WIFI_COUNT];
    for (int i = 0; i < count; i++)
    {
        ssids[i] = networks[i]["ssid"].as<String>();
        passwords[i] = networks[i]["password"].as<String>();
    }
    config_unlock();

    if (count == 0)
    {
        say("no networks saved");
        return false;
    }

    WiFi.mode(WIFI_STA);
    WiFi.setSleep(true); // the pad is idle most of the time

    say("scanning");
    int found = WiFi.scanNetworks();

    for (int i = 0; i < count; i++)
    {
        if (ssids[i].length() == 0)
            continue;

        bool inRange = false;
        for (int j = 0; j < found; j++)
        {
            if (WiFi.SSID(j) == ssids[i])
            {
                inRange = true;
                break;
            }
        }

        if (!inRange)
            continue;

        if (connect_to(ssids[i], passwords[i]))
        {
            WiFi.scanDelete();
            return true;
        }
    }

    WiFi.scanDelete();
    say("no saved network in range");
    return false;
}

void wifi_setup()
{
    WiFi.persistent(false);
    _offlineSince = millis();

    if (!start_sta())
        start_ap();
}

void wifi_reconnect()
{
    WiFi.softAPdisconnect(true);
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

    // an hour with no link and nothing to show for it: restart. Skipped
    // while a browser is likely to be sitting on the config page.
    if (_offlineSince != 0 && millis() - _offlineSince > OFFLINE_RESTART_MS)
    {
        if (WiFi.softAPgetStationNum() == 0)
        {
            _log("Offline for an hour, restarting\n");
            delay(100);
            ESP.restart();
        }
    }

    if (app.apMode)
    {
        // periodically give the saved networks another chance, so the pad
        // rejoins on its own once the router is back
        static unsigned long lastRetry = 0;
        if (millis() - lastRetry < RETRY_INTERVAL_MS)
            return;
        lastRetry = millis();

        // don't yank the radio out from under someone using the config page
        if (WiFi.softAPgetStationNum() > 0)
            return;

        if (start_sta())
            WiFi.softAPdisconnect(true);
        else
            start_ap();

        return;
    }

    // station mode: drop back to the access point if the link is lost, so
    // the config page never becomes unreachable
    bool connected = (WiFi.status() == WL_CONNECTED);
    if (connected != app.wifiConnected)
    {
        if (connected)
            publish_state(true, false, WiFi.localIP(), NULL);
        else
        {
            _log("WiFi lost\n");
            publish_state(false, false, IPAddress(0, 0, 0, 0), NULL);
            if (!start_sta())
                start_ap();
        }
    }
}
