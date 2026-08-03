#include <Arduino.h>

#include "app/app.h"
#include "display/display.h"
#include "keyboard/keyboard.h"
#include "service/Wifi/Wifi.h"
#include "service/WebUI/WebUI.h"
#include "service/MassStorage/MassStorage.h"

// WiFi association and HTTP request handling both block for long enough to
// be felt as key latency, so they get their own core. The Arduino loop
// already runs on core 1, so this task takes core 0.
static void NetworkCore(void *pvParameters)
{
    wifi_setup();
    webui_setup();

    while (true)
    {
        wifi_loop();
        webui_loop();
        delay(2);
    }
}

void setup()
{
    app_setup();

    // expose the filesystem as a USB drive
    ms_setup();

    display_setup();
    keyboard_setup();

    xTaskCreatePinnedToCore(
        NetworkCore,   // Function to run
        "NetworkCore", // Name
        8192,          // Stack size
        NULL,          // Parameters
        1,             // Priority
        NULL,          // Task handle
        0              // Core 0
    );
}

void loop()
{
    // read the keypad and the knob, then paint whatever changed
    keyboard_loop();
    display_loop();
    ms_loop();

    yield();
}
