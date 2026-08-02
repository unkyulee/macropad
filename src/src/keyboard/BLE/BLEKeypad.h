#pragma once

#include <Arduino.h>

void ble_setup();
void ble_loop();

// Rebuild the cached keymap after the web UI changes the configuration.
void ble_reload();

// index is the physical key index, matching the "keys" array in config.json
void ble_key(int index, bool pressed);

// direction is +1 for clockwise, -1 for counter clockwise
void ble_knob(int direction);

bool ble_connected();
