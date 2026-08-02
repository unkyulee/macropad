#include "WebUI.h"
#include "app/app.h"
#include "app/Config/Config.h"
#include "service/Wifi/Wifi.h"

#include <WebServer.h>
#include <WiFi.h>
#include <FFat.h>

static WebServer server(80);

// Shown when /index.html is missing from the filesystem, which happens
// before the first `pio run -t uploadfs`. Deliberately tiny: its only job
// is to keep WiFi setup reachable so the pad is never locked out.
static const char FALLBACK_PAGE[] PROGMEM =
    "<!doctype html><meta name=viewport content='width=device-width,initial-scale=1'>"
    "<title>Macro Pad</title>"
    "<style>body{font:16px system-ui;margin:2rem;max-width:30rem}"
    "input{width:100%;padding:.5rem;margin:.25rem 0 1rem;box-sizing:border-box}"
    "button{padding:.6rem 1.2rem}</style>"
    "<h1>Macro Pad</h1>"
    "<p>The web UI has not been uploaded yet. Run <code>pio run -t uploadfs</code>. "
    "You can still configure WiFi below.</p>"
    "<label>SSID<input id=s></label><label>Password<input id=p type=password></label>"
    "<button onclick=\"save()\">Save and connect</button><pre id=o></pre>"
    "<script>async function save(){"
    "const c=await(await fetch('/api/config')).json();"
    "c.wifi={ssid:s.value,password:p.value};"
    "await fetch('/api/config',{method:'POST',body:JSON.stringify(c)});"
    "o.textContent='Saved, reconnecting...';"
    "await fetch('/api/wifi/connect',{method:'POST'});}</script>";

static void handle_root()
{
    if (config_fs_ready() && FFat.exists("/index.html"))
    {
        File file = FFat.open("/index.html", "r");
        if (file)
        {
            server.streamFile(file, "text/html");
            file.close();
            return;
        }
    }

    server.send_P(200, "text/html", FALLBACK_PAGE);
}

static void handle_status()
{
    AppStatus &app = status();

    JsonDocument doc;
    doc["version"] = VERSION;
    doc["uptime"] = millis() / 1000;
    doc["heap"] = ESP.getFreeHeap();

    doc["wifi"]["connected"] = app.wifiConnected;
    doc["wifi"]["ap"] = app.apMode;
    doc["wifi"]["ip"] = app.ip;
    doc["wifi"]["ssid"] = app.ssid;
    doc["wifi"]["rssi"] = app.wifiConnected ? WiFi.RSSI() : 0;

    doc["ble"]["connected"] = app.bleConnected;

    doc["input"]["key"] = app.key;
    doc["input"]["pressed"] = app.pressed;
    doc["input"]["knob"] = app.knob;

    doc["storage"]["mounted"] = config_fs_ready();
    if (config_fs_ready())
    {
        doc["storage"]["total"] = FFat.totalBytes();
        doc["storage"]["free"] = FFat.freeBytes();
    }

    String out;
    serializeJson(doc, out);
    server.send(200, "application/json", out);
}

static void handle_config_get()
{
    config_lock();
    String out;
    serializeJson(config(), out);
    config_unlock();

    server.send(200, "application/json", out);
}

static void handle_config_post()
{
    if (!server.hasArg("plain"))
    {
        server.send(400, "application/json", "{\"error\":\"empty body\"}");
        return;
    }

    JsonDocument incoming;
    DeserializationError error = deserializeJson(incoming, server.arg("plain"));
    if (error)
    {
        String message = String("{\"error\":\"") + error.c_str() + "\"}";
        server.send(400, "application/json", message);
        return;
    }

    if (!incoming.is<JsonObject>())
    {
        server.send(400, "application/json", "{\"error\":\"expected an object\"}");
        return;
    }

    config_lock();
    config().clear();
    config().set(incoming.as<JsonObjectConst>());
    config_unlock();

    if (!config_save())
    {
        server.send(500, "application/json", "{\"error\":\"save failed\"}");
        return;
    }

    // the input core owns the parsed keymap, so ask it to rebuild rather
    // than touching its cache from this task
    status().configReload = true;

    server.send(200, "application/json", "{\"ok\":true}");
}

static void handle_wifi_scan()
{
    int found = WiFi.scanNetworks();

    JsonDocument doc;
    JsonArray networks = doc.to<JsonArray>();
    for (int i = 0; i < found; i++)
    {
        JsonObject network = networks.add<JsonObject>();
        network["ssid"] = WiFi.SSID(i);
        network["rssi"] = WiFi.RSSI(i);
        network["open"] = (WiFi.encryptionType(i) == WIFI_AUTH_OPEN);
    }
    WiFi.scanDelete();

    String out;
    serializeJson(doc, out);
    server.send(200, "application/json", out);
}

static void handle_wifi_connect()
{
    server.send(200, "application/json", "{\"ok\":true}");

    // the response has to go out before the radio drops
    delay(200);
    wifi_reconnect();
}

static void handle_reboot()
{
    server.send(200, "application/json", "{\"ok\":true}");
    delay(200);
    ESP.restart();
}

void webui_setup()
{
    server.on("/", HTTP_GET, handle_root);
    server.on("/api/status", HTTP_GET, handle_status);
    server.on("/api/config", HTTP_GET, handle_config_get);
    server.on("/api/config", HTTP_POST, handle_config_post);
    server.on("/api/wifi/scan", HTTP_GET, handle_wifi_scan);
    server.on("/api/wifi/connect", HTTP_POST, handle_wifi_connect);
    server.on("/api/reboot", HTTP_POST, handle_reboot);

    // anything else (css, js, images) comes straight off the filesystem
    if (config_fs_ready())
        server.serveStatic("/", FFat, "/");

    server.onNotFound([]()
                      { server.send(404, "application/json", "{\"error\":\"not found\"}"); });

    server.begin();
    _log("Web server listening on port 80\n");
}

void webui_loop()
{
    server.handleClient();
}
