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

// Clears the drawing area, leaving the footer alone. Screens call this
// from their setup().
void display_clear_body();

// The screen name and link state live in a footer that display.cpp draws
// and keeps up to date, separated from the body by a single rule rather
// than sitting in a filled bar.
#define FOOTER_HEIGHT 20

// small top margin for the body area
#define BODY_TOP 4

// first y coordinate the body must not draw on
int display_body_bottom();
