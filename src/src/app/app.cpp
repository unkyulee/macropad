#include "app.h"
#include "Config/Config.h"

// app state storage
AppStatus _status;
AppStatus &status()
{
    return _status;
}

// Bring up the serial console, the filesystem and the configuration.
void app_setup()
{
    Serial.begin(115200);
#ifdef _DEBUG
    // give the USB CDC console time to attach before the first log line
    delay(2000);
#endif

    _log("Macro Pad %s\n", VERSION);

#if defined(BOARD_HAS_PSRAM)
    if (psramFound())
        _log("PSRAM: %d bytes\n", ESP.getPsramSize());
    else
        _log("PSRAM not found or not initialized\n");
#endif

    _log("Flash: %u bytes at %u Hz\n", ESP.getFlashChipSize(), ESP.getFlashChipSpeed());

    // mounts the FAT partition and loads /config.json, falling back to
    // built-in defaults when either is missing
    config_setup();
}
