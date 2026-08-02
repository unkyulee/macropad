#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>

// number of physical keys the config describes, must match the keypad matrix
#define KEY_COUNT 20

// Mounts the FAT partition and loads /config.json, writing a default
// config when the file is missing or unreadable.
void config_setup();

bool config_load();
bool config_save();

// True when the FAT partition mounted, so the web server can serve files.
bool config_fs_ready();

// The live configuration document. It is read from the input core and
// written from the network core, so every access must be wrapped in
// config_lock() / config_unlock().
JsonDocument &config();

void config_lock();
void config_unlock();
