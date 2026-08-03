#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>

// number of physical keys, must match the keypad matrix
#define KEY_COUNT 20

// configurable screen slots
#define SCREEN_COUNT 5

// saved wifi networks the init screen will try in turn
#define WIFI_COUNT 10

// screen types, kept as strings in config.json
#define SCREEN_CLOCK "clock"
#define SCREEN_KEYMAP "keymap"
#define SCREEN_GIF "gif"
#define SCREEN_CALCULATOR "calculator"

// Mounts the FAT partition and loads /config.json, writing a default
// config when the file is missing or unreadable.
void config_setup();

bool config_load();
bool config_save();

// Replace everything with the factory defaults and write them out. The
// stored file wins over the built-in defaults on every boot, so this is
// the only way to pick up a changed default keymap.
void config_reset();

// The live configuration document. It is read from the input core and
// written from the network core, so every access must be wrapped in
// config_lock() / config_unlock().
JsonDocument &config();

void config_lock();
void config_unlock();
