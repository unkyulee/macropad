#include "app.h"

// app state storage
AppStatus _status;
AppStatus &status()
{
    return _status;
}

// Bring up the serial console and report the board resources.
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
}
