#include "keyboard.h"
#include "app/app.h"

#include "Keypad/Keypad.h"
#include "Knob/Knob.h"
#include "BLE/BLEKeypad.h"

//
void keyboard_setup()
{
    keypad_setup();
    knob_setup();
    ble_setup();
}

//
void keyboard_loop()
{
    // the web server runs on the other core and cannot touch the parsed
    // keymap safely, so it raises a flag and the rebuild happens here
    AppStatus &app = status();
    if (app.configReload)
    {
        app.configReload = false;
        ble_reload();
    }

    keypad_loop();
    knob_loop();
    ble_loop();
}
