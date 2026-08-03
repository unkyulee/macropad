#pragma once

#include <Arduino.h>

// Every screen implements this. setup() is called once when the screen
// becomes active, render() on every display pass, key() only while the
// screen is active. key() returns true when it handled the press itself.
struct Screen
{
    void (*setup)(int slot);
    void (*render)(int slot);
    bool (*key)(int index, bool pressed);
};
