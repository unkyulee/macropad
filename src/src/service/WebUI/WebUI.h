#pragma once

// Local configuration web server. Serves the UI compiled into the firmware
// from data/index.html and exposes a small JSON API on top of the shared
// configuration.
void webui_setup();
void webui_loop();
