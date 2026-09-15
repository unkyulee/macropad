#include "HidKeyboard.h"

#include <NimBLEDevice.h>
#include <NimBLEHIDDevice.h>

static const uint8_t KEYBOARD_ID = 0x01;
static const uint8_t MEDIA_KEYS_ID = 0x02;

static const uint16_t HID_VID = 0x05ac;
static const uint16_t HID_PID = 0x820a;
static const uint16_t HID_VERSION = 0x0210;

// Reports sent back to back can be coalesced or dropped by the host, which
// shows up as a modifier missing from a shortcut.
static const uint32_t REPORT_GAP_MS = 7;

// Encoded by hand rather than with HIDTypes.h: the Arduino core's own BLE
// library ships a header of the same name, and pulling it in makes the
// library finder compile that whole stack alongside NimBLE.
// prettier-ignore
static const uint8_t REPORT_MAP[] = {
    0x05, 0x01,          // USAGE_PAGE (Generic Desktop)
    0x09, 0x06,          // USAGE (Keyboard)
    0xA1, 0x01,          // COLLECTION (Application)
    0x85, KEYBOARD_ID,   //   REPORT_ID
    0x05, 0x07,          //   USAGE_PAGE (Keyboard/Keypad)
    0x19, 0xE0,          //   USAGE_MINIMUM (Left Control)
    0x29, 0xE7,          //   USAGE_MAXIMUM (Right GUI)
    0x15, 0x00,          //   LOGICAL_MINIMUM (0)
    0x25, 0x01,          //   LOGICAL_MAXIMUM (1)
    0x75, 0x01,          //   REPORT_SIZE (1)
    0x95, 0x08,          //   REPORT_COUNT (8)
    0x81, 0x02,          //   INPUT (Data,Var,Abs) ; modifier bits
    0x95, 0x01,          //   REPORT_COUNT (1)
    0x75, 0x08,          //   REPORT_SIZE (8)
    0x81, 0x01,          //   INPUT (Const) ; reserved byte
    0x95, 0x05,          //   REPORT_COUNT (5)
    0x75, 0x01,          //   REPORT_SIZE (1)
    0x05, 0x08,          //   USAGE_PAGE (LEDs)
    0x19, 0x01,          //   USAGE_MINIMUM (Num Lock)
    0x29, 0x05,          //   USAGE_MAXIMUM (Kana)
    0x91, 0x02,          //   OUTPUT (Data,Var,Abs) ; LED bits
    0x95, 0x01,          //   REPORT_COUNT (1)
    0x75, 0x03,          //   REPORT_SIZE (3)
    0x91, 0x01,          //   OUTPUT (Const) ; LED padding
    0x95, 0x06,          //   REPORT_COUNT (6)
    0x75, 0x08,          //   REPORT_SIZE (8)
    0x15, 0x00,          //   LOGICAL_MINIMUM (0)
    0x25, 0x65,          //   LOGICAL_MAXIMUM (101)
    0x05, 0x07,          //   USAGE_PAGE (Keyboard/Keypad)
    0x19, 0x00,          //   USAGE_MINIMUM (0)
    0x29, 0x65,          //   USAGE_MAXIMUM (101)
    0x81, 0x00,          //   INPUT (Data,Array,Abs) ; 6 key slots
    0xC0,                // END_COLLECTION

    0x05, 0x0C,          // USAGE_PAGE (Consumer)
    0x09, 0x01,          // USAGE (Consumer Control)
    0xA1, 0x01,          // COLLECTION (Application)
    0x85, MEDIA_KEYS_ID, //   REPORT_ID
    0x05, 0x0C,          //   USAGE_PAGE (Consumer)
    0x15, 0x00,          //   LOGICAL_MINIMUM (0)
    0x25, 0x01,          //   LOGICAL_MAXIMUM (1)
    0x75, 0x01,          //   REPORT_SIZE (1)
    0x95, 0x10,          //   REPORT_COUNT (16)
    0x09, 0xB5,          //   USAGE (Scan Next Track)     ; byte 0 bit 0
    0x09, 0xB6,          //   USAGE (Scan Previous Track)
    0x09, 0xB7,          //   USAGE (Stop)
    0x09, 0xCD,          //   USAGE (Play/Pause)
    0x09, 0xE2,          //   USAGE (Mute)
    0x09, 0xE9,          //   USAGE (Volume Increment)
    0x09, 0xEA,          //   USAGE (Volume Decrement)
    0x0A, 0x23, 0x02,    //   USAGE (WWW Home)            ; byte 0 bit 7
    0x0A, 0x94, 0x01,    //   USAGE (My Computer)         ; byte 1 bit 0
    0x0A, 0x92, 0x01,    //   USAGE (Calculator)
    0x0A, 0x2A, 0x02,    //   USAGE (WWW Favorites)
    0x0A, 0x21, 0x02,    //   USAGE (WWW Search)
    0x0A, 0x26, 0x02,    //   USAGE (WWW Stop)
    0x0A, 0x24, 0x02,    //   USAGE (WWW Back)
    0x0A, 0x83, 0x01,    //   USAGE (Media Select)
    0x0A, 0x8A, 0x01,    //   USAGE (Mail)                ; byte 1 bit 7
    0x81, 0x02,          //   INPUT (Data,Var,Abs)
    0xC0,                // END_COLLECTION
};

// US layout: ASCII -> HID usage, with the high bit meaning "needs shift"
static const uint8_t SH = 0x80;

// prettier-ignore
static const uint8_t ASCII_MAP[128] = {
    // 0x00: only backspace, tab and line feed are typeable
    0, 0, 0, 0, 0, 0, 0, 0, 0x2a, 0x2b, 0x28, 0, 0, 0, 0, 0,
    // 0x10
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    // 0x20: space ! " # $ % & ' ( ) * + , - . /
    0x2c, SH|0x1e, SH|0x34, SH|0x20, SH|0x21, SH|0x22, SH|0x24, 0x34,
    SH|0x26, SH|0x27, SH|0x25, SH|0x2e, 0x36, 0x2d, 0x37, 0x38,
    // 0x30: 0-9 : ; < = > ?
    0x27, 0x1e, 0x1f, 0x20, 0x21, 0x22, 0x23, 0x24,
    0x25, 0x26, SH|0x33, 0x33, SH|0x36, 0x2e, SH|0x37, SH|0x38,
    // 0x40: @ A-O
    SH|0x1f, SH|0x04, SH|0x05, SH|0x06, SH|0x07, SH|0x08, SH|0x09, SH|0x0a,
    SH|0x0b, SH|0x0c, SH|0x0d, SH|0x0e, SH|0x0f, SH|0x10, SH|0x11, SH|0x12,
    // 0x50: P-Z [ \ ] ^ _
    SH|0x13, SH|0x14, SH|0x15, SH|0x16, SH|0x17, SH|0x18, SH|0x19, SH|0x1a,
    SH|0x1b, SH|0x1c, SH|0x1d, 0x2f, 0x31, 0x30, SH|0x23, SH|0x2d,
    // 0x60: ` a-o
    0x35, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a,
    0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10, 0x11, 0x12,
    // 0x70: p-z { | } ~ DEL
    0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1a,
    0x1b, 0x1c, 0x1d, SH|0x2f, SH|0x31, SH|0x30, SH|0x35, 0,
};

// The host writes the LED output report whenever a lock key toggles
class HidOutputCallbacks : public NimBLECharacteristicCallbacks
{
public:
    explicit HidOutputCallbacks(HidKeyboard &keyboard) : _keyboard(keyboard) {}

    void onWrite(NimBLECharacteristic *chr, NimBLEConnInfo &) override
    {
        NimBLEAttValue value = chr->getValue();
        if (value.size() > 0)
            _keyboard._leds = value.data()[0];
    }

private:
    HidKeyboard &_keyboard;
};

void HidKeyboard::begin(const std::string &name, const std::string &manufacturer)
{
    NimBLEDevice::init(name);
    NimBLEDevice::setSecurityAuth(true, true, true);

    _server = NimBLEDevice::createServer();
    // 1.x did this by default, 2.x leaves the pad invisible after a disconnect
    _server->advertiseOnDisconnect(true);

    NimBLEHIDDevice *hid = new NimBLEHIDDevice(_server);
    _inputKeys = hid->getInputReport(KEYBOARD_ID);
    _inputMedia = hid->getInputReport(MEDIA_KEYS_ID);
    hid->getOutputReport(KEYBOARD_ID)->setCallbacks(new HidOutputCallbacks(*this));

    hid->setManufacturer(manufacturer);
    hid->setPnp(0x02, HID_VID, HID_PID, HID_VERSION);
    hid->setHidInfo(0x00, 0x01);
    hid->setReportMap((uint8_t *)REPORT_MAP, sizeof(REPORT_MAP));
    hid->setBatteryLevel(100);

    NimBLEAdvertising *advertising = _server->getAdvertising();
    // 2.x no longer puts the device name in the advertisement on its own
    advertising->setName(name);
    advertising->setAppearance(HID_KEYBOARD);
    advertising->addServiceUUID(hid->getHidService()->getUUID());
    advertising->start(); // also starts the GATT server
}

bool HidKeyboard::isConnected() const
{
    return _server != nullptr && _server->getConnectedCount() > 0;
}

void HidKeyboard::sendKeys()
{
    if (!isConnected())
        return;

    _inputKeys->setValue((uint8_t *)&_keyReport, sizeof(_keyReport));
    _inputKeys->notify();
    delay(REPORT_GAP_MS);
}

void HidKeyboard::sendMedia()
{
    if (!isConnected())
        return;

    _inputMedia->setValue(_mediaReport, sizeof(_mediaReport));
    _inputMedia->notify();
    delay(REPORT_GAP_MS);
}

void HidKeyboard::press(uint8_t k)
{
    if (k >= 136)
    {
        k -= 136; // non-printing key, already a HID usage
    }
    else if (k >= 128)
    {
        _keyReport.modifiers |= (1 << (k - 128));
        k = 0;
    }
    else
    {
        k = ASCII_MAP[k];
        if (k == 0)
            return;

        if (k & SH)
        {
            _keyReport.modifiers |= 0x02; // left shift
            k &= 0x7F;
        }
    }

    if (k != 0)
    {
        int free = -1;
        for (int i = 0; i < 6; i++)
        {
            if (_keyReport.keys[i] == k)
            {
                free = -2; // already held
                break;
            }
            if (free == -1 && _keyReport.keys[i] == 0)
                free = i;
        }

        if (free == -1)
            return; // all six slots taken
        if (free >= 0)
            _keyReport.keys[free] = k;
    }

    sendKeys();
}

void HidKeyboard::release(uint8_t k)
{
    if (k >= 136)
    {
        k -= 136;
    }
    else if (k >= 128)
    {
        _keyReport.modifiers &= ~(1 << (k - 128));
        k = 0;
    }
    else
    {
        k = ASCII_MAP[k];
        if (k == 0)
            return;

        if (k & SH)
        {
            _keyReport.modifiers &= ~0x02;
            k &= 0x7F;
        }
    }

    if (k != 0)
    {
        for (int i = 0; i < 6; i++)
        {
            if (_keyReport.keys[i] == k)
                _keyReport.keys[i] = 0;
        }
    }

    sendKeys();
}

void HidKeyboard::press(const MediaKeyReport k)
{
    _mediaReport[0] |= k[0];
    _mediaReport[1] |= k[1];
    sendMedia();
}

void HidKeyboard::release(const MediaKeyReport k)
{
    _mediaReport[0] &= ~k[0];
    _mediaReport[1] &= ~k[1];
    sendMedia();
}

void HidKeyboard::releaseAll()
{
    _keyReport = {};
    _mediaReport[0] = 0;
    _mediaReport[1] = 0;
    sendKeys();
    sendMedia();
}
