#pragma once

#include <Arduino.h>
#include "Log/Log.h"

// app version
#define VERSION "0.2.0"

// Shared state between the input layer (keypad / knob), the network task
// and the display. Inputs and services write, the display reads and clears
// the dirty flag. Fixed char buffers instead of String: this struct is
// touched from both cores and a String would reallocate under the reader.
struct AppStatus
{
    // last key event
    int key = -1;         // key index in the matrix
    char label = 0;       // character assigned to that key
    bool pressed = false; // press or release

    // rotary encoder
    long knob = 0;     // accumulated position
    int knobDelta = 0; // last movement, -1 or +1

    // network
    bool wifiConnected = false;
    bool apMode = false;
    char ip[16] = "";
    char ssid[33] = "";
    char apName[24] = "";
    char wifiMessage[48] = "starting";

    // active screen slot, or -1 while the init screen owns the display
    int screen = -1;
    bool booting = true;

    // bluetooth
    bool bleConnected = false;

    // set by inputs and services, cleared by the display once rendered
    volatile bool dirty = true;

    // raised by the web server after a config change so the input layer
    // rebuilds its cached keymap on the core that owns it
    volatile bool configReload = false;
};

AppStatus &status();

//
void app_setup();

// Storage. Everything that touches files goes through this rather than
// naming a concrete filesystem, which stays in one place.
class FileSystem;
FileSystem *gfs();

// Same object, concrete type, for the few callers that need directory
// listings or free space.
class FileSystemFAT;
FileSystemFAT *fatfs();

// True once the storage partition is mounted.
bool fs_ready();
