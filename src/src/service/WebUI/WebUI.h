#pragma once

// Local configuration web server. Serves the static UI from the FAT
// partition (upload it with: pio run -t uploadfs) and exposes a small
// JSON API on top of the shared configuration.
void webui_setup();
void webui_loop();
