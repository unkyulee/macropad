#include <Arduino.h>

#include "app/app.h"
#include "display/display.h"
#include "keyboard/keyboard.h"

void setup()
{
    app_setup();
    display_setup();
    keyboard_setup();
}

void loop()
{
    // read the keypad and the knob, then paint whatever changed
    keyboard_loop();
    display_loop();

    yield();
}
