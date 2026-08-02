#pragma once

#include <Arduino.h>
#include "Log/Log.h"

// app version
#define VERSION "0.1.0"

// Shared state between the input layer (keypad / knob) and the display.
// Inputs only write, the display only reads and clears the dirty flag.
struct AppStatus
{
    // last key event
    int key = -1;         // key index in the matrix
    char label = 0;       // character assigned to that key
    bool pressed = false; // press or release

    // rotary encoder
    long knob = 0;      // accumulated position
    int knobDelta = 0;  // last movement, -1 or +1

    // set by inputs, cleared by the display once rendered
    bool dirty = true;
};

AppStatus &status();

//
void app_setup();
