#include "keyboard.h"

#include "Keypad/Keypad.h"
#include "Knob/Knob.h"

//
void keyboard_setup()
{
    keypad_setup();
    knob_setup();
}

//
void keyboard_loop()
{
    keypad_loop();
    knob_loop();
}
