#include "Knob.h"
#include "app/app.h"
#include "keyboard/BLE/BLEKeypad.h"

#include <RotaryEncoder.h>

#define KNOB_PIN_A 21
#define KNOB_PIN_B 20

// FOUR3 is the latch mode for a detented EC11: one step per detent
static RotaryEncoder encoder(KNOB_PIN_A, KNOB_PIN_B, RotaryEncoder::LatchMode::FOUR3);

static void IRAM_ATTR knob_isr()
{
    encoder.tick();
}

//
void knob_setup()
{
    pinMode(KNOB_PIN_A, INPUT_PULLUP);
    pinMode(KNOB_PIN_B, INPUT_PULLUP);

    attachInterrupt(digitalPinToInterrupt(KNOB_PIN_A), knob_isr, CHANGE);
    attachInterrupt(digitalPinToInterrupt(KNOB_PIN_B), knob_isr, CHANGE);

    _log("Knob initialized (A=%d B=%d)\n", KNOB_PIN_A, KNOB_PIN_B);
}

//
void knob_loop()
{
    // the ISR keeps the state machine current, this only picks up the result
    static long lastPos = 0;

    long pos = encoder.getPosition();
    if (pos == lastPos)
        return;

    AppStatus &app = status();
    app.knobDelta = (pos > lastPos) ? 1 : -1;
    app.knob = pos;
    app.dirty = true;
    lastPos = pos;

    _log("[knob] position %ld direction %d\n", app.knob, app.knobDelta);

    ble_knob(app.knobDelta);
}
