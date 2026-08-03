#pragma once

// USB mass storage. Exposes the FAT partition as a drive, so plugging the
// pad into a computer shows config.json, index.html and the GIF folder.
void ms_setup();
void ms_loop();

// True while the host has the volume mounted.
bool ms_active();
