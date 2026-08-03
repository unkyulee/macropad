#include "app.h"
#include "Config/Config.h"
#include "FileSystem/FileSystemFAT.h"

// app state storage
AppStatus _status;
AppStatus &status()
{
    return _status;
}

// One filesystem for everything: the web UI uploaded from data/,
// config.json, and GIF uploads. The partition label has to match
// partitions_16mb.csv.
static FileSystemFAT _storage("storage");
static bool _fsReady = false;

FileSystem *gfs()
{
    return &_storage;
}

FileSystemFAT *fatfs()
{
    return &_storage;
}

bool fs_ready()
{
    return _fsReady;
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

    _fsReady = _storage.begin();

    // listing the root at boot makes it obvious whether the data/ upload
    // landed and whether anything persisted across the reset
    if (_fsReady)
    {
        File root = _storage.openDir("/");
        if (root && root.isDirectory())
        {
            File entry = root.openNextFile();
            while (entry)
            {
                _log("  %s%s  %u bytes\n", entry.isDirectory() ? "[dir] " : "",
                     entry.name(), entry.size());
                entry = root.openNextFile();
            }
            root.close();
        }
    }

    // loads /config.json, falling back to built-in defaults if it is
    // missing or unreadable
    config_setup();
}
