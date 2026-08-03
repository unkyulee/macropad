#include "Keypad.h"
#include "app/app.h"
#include "keyboard/BLE/BLEKeypad.h"

#include <Adafruit_Keypad.h>

// 5 rows x 4 columns, matching the macro pad PCB layout
#define ROWS 5
#define COLS 4

// TODO: placeholder pins - replace with the real ESP32-S3 wiring.
// Must stay clear of the ILI9341 SPI pins (3, 10, 11, 12, 13, 46).
static byte rowPins[ROWS] = {1, 2, 42, 41, 40};
static byte colPins[COLS] = {39, 45, 48, 47};

// Adafruit_Keypad reports the value stored here as e.bit.KEY, so the
// matrix holds the flat key index and the label table below maps that
// index to the character shown on screen.
// prettier-ignore
static char keys[ROWS][COLS] = {
    { 0,  1,  2,  3},
    { 4,  5,  6,  7},
    { 8,  9, 10, 11},
    {12, 13, 14, 15},
    {16, 17, 18, 19},
};

// what each physical key means, indexed by the key index above
// prettier-ignore
static const char labels[ROWS * COLS] = {
    '1', '2', '3', '4',
    '5', '6', '7', '8',
    '9', 'A', 'B', 'C',
    'D', 'E', 'F', 'G',
    'H',   0, 'I',   0,
};

static Adafruit_Keypad keypad = Adafruit_Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

//
void keypad_setup()
{
    keypad.begin();
    _log("Keypad initialized (%dx%d)\n", ROWS, COLS);
}

//
void keypad_loop()
{
    // debounce interval
    static unsigned long last = 0;
    if (millis() - last < 10)
        return;
    last = millis();

    keypad.tick();

    while (keypad.available())
    {
        keypadEvent e = keypad.read();
        int index = e.bit.KEY;
        if (index < 0 || index >= ROWS * COLS)
            continue;

        AppStatus &app = status();
        app.key = index;
        app.label = labels[index];
        app.pressed = (e.bit.EVENT == KEY_JUST_PRESSED);
        app.dirty = true;

        _debug("[keypad] key %d '%c' %s\n", index, app.label,
               app.pressed ? "pressed" : "released");

        // forward to the host over BLE, no-op while unpaired
        ble_key(index, app.pressed);
    }
}
