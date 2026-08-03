#pragma once

#include <Arduino.h>

void keypad_setup();
void keypad_loop();

// Re-read the keypad related settings after a config change.
void keypad_reload();

// Physical key index that cycles screens instead of sending a keystroke.
int keypad_screen_key();
