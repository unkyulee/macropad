#pragma once

#include <Arduino.h>
#include <TFT_eSPI.h>

void display_setup();
void display_loop();

// Key routing. Returns true when the screen consumed the key, in which
// case it must not also be sent to the host over BLE.
bool display_key(int index, bool pressed);

// Advance to the next enabled screen, wrapping around. Called when the
// knob button is pressed and when the init screen finishes.
void display_next_screen();

// Rebuild screen state after the web UI changes the configuration.
void display_reload();

// Shared panel handle for the individual screens.
TFT_eSPI &display_tft();

// Helpers the screens use so they all look like part of the same device.
void display_header(const char *title);
void display_clear_body();

// y coordinate where the body area starts, below the header
#define BODY_TOP 34
