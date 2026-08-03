#include "WebUI.h"
#include "app/app.h"
#include "app/Config/Config.h"
#include "service/Wifi/Wifi.h"

#include <WebServer.h>
#include <WiFi.h>
#include <FFat.h>
#include "app/FileSystem/FileSystemFAT.h"

static WebServer server(80);

// The UI is uploaded from data/ with `pio run -t uploadfs` and served
// straight off the filesystem, alongside config.json and the GIF uploads.
#define INDEX_FILE "/index.html"

static void handle_root()
{
    server.sendHeader("Cache-Control", "no-cache");

    if (fs_ready() && gfs()->exists(INDEX_FILE))
    {
        File file = gfs()->open(INDEX_FILE, "r");
        if (file)
        {
            server.streamFile(file, "text/html");
            file.close();
            return;
        }
    }

    // nothing to serve: the API still works, so say what is missing rather
    // than leaving a blank page
    server.send(200, "text/plain",
                "index.html is not on the filesystem.\n"
                "Upload it with: pio run -t uploadfs\n");
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

    doc["screen"] = app.screen;

    doc["storage"]["mounted"] = fs_ready();
    if (fs_ready())
    {
        doc["storage"]["total"] = fatfs()->totalBytes();
        doc["storage"]["free"] = fatfs()->freeBytes();
        doc["storage"]["config"] = gfs()->exists("/config.json");
        doc["storage"]["ui"] = gfs()->exists(INDEX_FILE);
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

// ---- GIF storage ------------------------------------------------------
// Uploads land in /gif so the screen config can point at them by path and
// the mass storage view later has somewhere obvious to drop files.
#define GIF_DIR "/gif"

static File _upload;

static void handle_gif_list()
{
    JsonDocument doc;
    JsonArray files = doc.to<JsonArray>();

    if (fs_ready())
    {
        File dir = fatfs()->openDir(GIF_DIR);
        if (dir && dir.isDirectory())
        {
            File entry = dir.openNextFile();
            while (entry)
            {
                if (!entry.isDirectory())
                {
                    JsonObject item = files.add<JsonObject>();
                    item["path"] = String(GIF_DIR "/") + entry.name();
                    item["size"] = entry.size();
                }
                entry = dir.openNextFile();
            }
            dir.close();
        }
    }

    String out;
    serializeJson(doc, out);
    server.send(200, "application/json", out);
}

// WebServer calls this repeatedly as the body streams in, then once more
// at the end. The response is sent by the handler registered alongside it.
static void handle_gif_upload_data()
{
    HTTPUpload &upload = server.upload();

    if (upload.status == UPLOAD_FILE_START)
    {
        if (!fs_ready())
            return;

        fatfs()->mkdir(GIF_DIR);

        String name = upload.filename;
        int slash = name.lastIndexOf('/');
        if (slash >= 0)
            name = name.substring(slash + 1);
        if (!name.endsWith(".gif"))
            name += ".gif";

        String path = String(GIF_DIR "/") + name;
        _upload = gfs()->open(path.c_str(), "w");
        _log("Receiving %s\n", path.c_str());
    }
    else if (upload.status == UPLOAD_FILE_WRITE)
    {
        if (_upload)
            _upload.write(upload.buf, upload.currentSize);
    }
    else if (upload.status == UPLOAD_FILE_END)
    {
        if (_upload)
        {
            _upload.close();
            _log("Received %u bytes\n", upload.totalSize);
        }
    }
}

static void handle_gif_upload_done()
{
    server.send(200, "application/json", "{\"ok\":true}");
}

static void handle_gif_delete()
{
    if (!server.hasArg("path"))
    {
        server.send(400, "application/json", "{\"error\":\"missing path\"}");
        return;
    }

    String path = server.arg("path");
    if (!path.startsWith(GIF_DIR "/"))
    {
        server.send(400, "application/json", "{\"error\":\"outside " GIF_DIR "\"}");
        return;
    }

    bool ok = gfs()->remove(path.c_str());
    server.send(ok ? 200 : 404, "application/json", ok ? "{\"ok\":true}" : "{\"error\":\"not found\"}");
}

// The stored config wins over the built-in defaults on every boot, so a
// firmware update that changes the default keymap has no effect until this
// is called.
static void handle_config_reset()
{
    config_reset();
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
    server.on("/api/config/reset", HTTP_POST, handle_config_reset);
    server.on("/api/wifi/scan", HTTP_GET, handle_wifi_scan);
    server.on("/api/gif", HTTP_GET, handle_gif_list);
    server.on("/api/gif", HTTP_DELETE, handle_gif_delete);
    server.on("/api/gif/upload", HTTP_POST, handle_gif_upload_done, handle_gif_upload_data);
    server.on("/api/wifi/connect", HTTP_POST, handle_wifi_connect);
    server.on("/api/reboot", HTTP_POST, handle_reboot);

    // anything else (css, js, images) comes straight off the filesystem
    if (fs_ready())
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
