#include "MassStorage.h"
#include "app/app.h"
#include "app/Config/Config.h"
#include "app/FileSystem/FileSystemFAT.h"

#include "USB.h"
#include "USBMSC.h"
#include "esp_partition.h"
#include "wear_levelling.h"

#if CONFIG_TINYUSB_MSC_ENABLED

static USBMSC msc;

// MSC hands the host raw sectors, so it needs its own view of the wear
// levelling layer: FFat keeps its wl_handle private and there is no way to
// borrow it. Both instances sit over the same partition, which is fine for
// reads. Writes from the host are followed by a remount so the device side
// does not keep a stale picture of the volume.
static wl_handle_t _wl = WL_INVALID_HANDLE;
static uint32_t _sectorSize = 0;
static uint32_t _sectorCount = 0;

static volatile bool _mounted = false;      // host has the volume
static volatile unsigned long _lastWrite = 0; // millis of the last host write
static volatile bool _hostWrote = false;

bool ms_active()
{
    return _mounted;
}

static int32_t ms_read(uint32_t lba, uint32_t offset, void *buffer, uint32_t bufsize)
{
    if (_wl == WL_INVALID_HANDLE)
        return -1;

    size_t address = (size_t)lba * _sectorSize + offset;
    if (wl_read(_wl, address, buffer, bufsize) != ESP_OK)
        return -1;

    return bufsize;
}

static int32_t ms_write(uint32_t lba, uint32_t offset, uint8_t *buffer, uint32_t bufsize)
{
    if (_wl == WL_INVALID_HANDLE)
        return -1;

    size_t address = (size_t)lba * _sectorSize + offset;

    // flash has to be erased before it can be rewritten
    if (wl_erase_range(_wl, address, bufsize) != ESP_OK)
        return -1;

    if (wl_write(_wl, address, buffer, bufsize) != ESP_OK)
        return -1;

    _lastWrite = millis();
    _hostWrote = true;
    return bufsize;
}

// The host calls this on mount and on eject.
static bool ms_start_stop(uint8_t power_condition, bool start, bool load_eject)
{
    (void)power_condition;

    if (load_eject)
    {
        _mounted = start;
        _log("USB drive %s\n", start ? "mounted by host" : "ejected");

        // an eject is the host telling us it is finished, so pick up
        // whatever it left behind right away
        if (!start)
            _hostWrote = true;
    }

    return true;
}

void ms_setup()
{
    const esp_partition_t *partition = esp_partition_find_first(
        ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_FAT, "storage");

    if (partition == NULL)
    {
        _log("Mass storage: no 'storage' partition found\n");
        return;
    }

    if (wl_mount(partition, &_wl) != ESP_OK)
    {
        _log("Mass storage: wear levelling mount failed\n");
        _wl = WL_INVALID_HANDLE;
        return;
    }

    _sectorSize = wl_sector_size(_wl);
    _sectorCount = wl_size(_wl) / _sectorSize;

    msc.vendorID("MacroPad");
    msc.productID("Storage");
    msc.productRevision("1.0");
    msc.onRead(ms_read);
    msc.onWrite(ms_write);
    msc.onStartStop(ms_start_stop);
    msc.mediaPresent(true);
    msc.begin(_sectorCount, _sectorSize);

    USB.begin();

    _log("Mass storage ready: %u sectors of %u bytes\n", _sectorCount, _sectorSize);
}

void ms_loop()
{
    if (!_hostWrote)
        return;

    // wait for the host to stop writing before touching the volume, a file
    // copy arrives as a burst of sector writes
    if (millis() - _lastWrite < 1500)
        return;

    _hostWrote = false;

    // remount so the FATFS layer drops its cached directory data, then
    // reload the configuration in case that is what the host replaced
    _log("Host finished writing, reloading filesystem\n");

    fatfs()->end();
    if (fatfs()->begin())
    {
        config_load();
        status().configReload = true;
    }
}

#else // CONFIG_TINYUSB_MSC_ENABLED

// TinyUSB is not compiled in: this happens when ARDUINO_USB_MODE=1, which
// selects the hardware CDC/JTAG peripheral and cannot do mass storage.
void ms_setup()
{
    _log("Mass storage unavailable, build with -D ARDUINO_USB_MODE=0\n");
}

void ms_loop() {}

bool ms_active()
{
    return false;
}

#endif
