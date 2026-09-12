# macropad 


# Bill of Material

* [ILI9341 2.8" TFT LCD (240x320)](https://ko.aliexpress.com/item/1005006323532762.html)
  – Includes a built-in SD card slot (used in this build). Variations exist, so double-check dimensions if using the provided STL files.
  - If you can find the same dimension with ST7789 chip, it will still work and actually has a better quality. Just that it is a bit difficult to find than ILI9341

* [ESP32-S3 N16R8](https://www.amazon.com/Development-AYWHP-ESP32-S3-DevKitC-WROOM-1-N16R8-Compatible/dp/B0DG8L7MQ9)

* [Keypad PCB](./PCB)
  - You will need a PCB for the keypad. It is possible to handwire instead of using the PCB. Refer to the schematic to figure out the wiring in case of hand wiring


# Wiring Diagram


## ESP32-S3 and DISPLAY

The panel is two separate chips on one board: the **ILI9341** display controller and the
**XPT2046** resistive touch digitiser. They share a single SPI bus (SPI3 / HSPI) and are
told apart by their own chip selects — `TFT_CS` for the panel, `TOUCH_CS` for touch.
Eleven wires total, one of which is optional.

```
   ESP32-S3                              ILI9341 module
   ========                              ==============

   3V3      ───────────────────────────  VCC
   GND      ───────────────────────────  GND

   GPIO 12  ──┬────────────────────────  SCK     ┐
              └────────────────────────  T_CLK   │
   GPIO 11  ──┬────────────────────────  SDI     │  shared
              └────────────────────────  T_DIN   │  SPI bus
   GPIO 13  ──┬────────────────────────  SDO     │
              └────────────────────────  T_DO    ┘

   GPIO 10  ───────────────────────────  CS      ─  display select
   GPIO  3  ───────────────────────────  RESET
   GPIO 46  ───────────────────────────  DC / RS
   GPIO 14  ───────────────────────────  LED     ─  backlight

   GPIO  9  ───────────────────────────  T_CS    ─  touch select
```

## Display and touch

| ESP32-S3 | Module pin    | Signal             | Notes |
| -------- | ------------- | ------------------ | ----- |
| `3V3`    | `VCC`         | 3.3 V supply       | Panel and both controllers are 3.3 V logic |
| `GND`    | `GND`         | Ground             | Keep the return short, next to the SPI lines |
| `GPIO 12`| `SCK` `T_CLK` | SPI clock — shared | One clock, both chips |
| `GPIO 11`| `SDI` `T_DIN` | MOSI — shared      | `SDI` on the panel, `T_DIN` on the digitiser |
| `GPIO 13`| `SDO` `T_DO`  | MISO — shared      | **Required.** Touch is read-back only; no MISO, no coordinates |
| `GPIO 10`| `CS`          | Display select     | `TFT_CS` |
| `GPIO  3`| `RESET`       | Panel reset        | Strapping pin (JTAG source); safe as an output |
| `GPIO 46`| `DC` / `RS`   | Data / command     | Strapping pin; floats during reset, output after boot |
| `GPIO 14`| `LED`         | Backlight          | Module carries the series resistor; PWM here for brightness |
| `GPIO  9`| `T_CS`        | Touch select       | `TOUCH_CS` |
| `GPIO  8`| `T_IRQ`       | Pen interrupt      | Optional — TFT_eSPI polls instead. Wire it only for wake-on-touch |

The module's **SD card header is left unconnected**. Firmware storage is the internal
`storage` FAT partition (`FFat`), exposed to the host over USB MSC.


## ESP32-S3 and KEYPAD

A scanned 5 × 4 matrix. Nine GPIO carry 18 keys — 17 Cherry MX switches (`U1`–`U17`) plus
the knob's push button — each with a 1N4148 (`D1`–`D18`) so held keys can't ghost. Rows are
driven, columns are read; the knob's A/B ride the same ribbon.

```
   ESP32-S3                            CN2          Keypad PCB
   ========                         (12-pin)        ==========

   GPIO  1  ──────────────────────────  1  ───────── ROW 1   ┐
   GPIO  2  ──────────────────────────  2  ───────── ROW 2   │
   GPIO 42  ──────────────────────────  3  ───────── ROW 3   │  driven
   GPIO 41  ──────────────────────────  4  ───────── ROW 4   │
   GPIO 40  ──────────────────────────  5  ───────── ROW 5   ┘

   GPIO 39  ──────────────────────────  6  ───────── COL 1   ┐
   GPIO 17  ──────────────────────────  7  ───────── COL 2   │  read
   GPIO 18  ──────────────────────────  8  ───────── COL 3   │  (pull-up)
   GPIO 47  ──────────────────────────  9  ───────── COL 4   ┘

   GPIO  4  ────────────────────────── 10  ───────── KNOB A
   GPIO  5  ────────────────────────── 11  ───────── KNOB B
   GND      ────────────────────────── 12  ───────── GND


   one node:   ROW n ──┬── switch ──▶|── COL m
                       │     Uxx      Dxx
```

`CN2` is a 12-pin Molex 22035125; the pin order above is the net count, not a verified
pinout — check it against the silkscreen before crimping.

### Physical layout

Keys sit on a 19.1 mm (0.75") grid. Three are oversized, which is why 17 switches fill 20
grid positions:

```
   ┌──────┬──────┬──────┬──────┐
   │  U1  │  U2  │  U3  │  U4  │  row 1
   ├──────┼──────┼──────┼──────┤
   │  U5  │  U6  │  U7  │      │  row 2
   ├──────┼──────┼──────┤  U8  │        U8, U15  2U vertical
   │  U9  │ U10  │ U11  │      │  row 3
   ├──────┼──────┼──────┼──────┤
   │ U12  │ U13  │ U14  │      │  row 4
   ├──────┴──────┼──────┤ U15  │        U16      2U horizontal
   │     U16     │ U17  │      │  row 5
   └─────────────┴──────┴──────┘
                                  ← EC11 knob mounts off the right edge
```

### Key index map

Each cell is **index** · designator · label, as reported by `keypad_loop()` and mapped
through `labels[]` in [Keypad.cpp](src/src/keyboard/Keypad/Keypad.cpp):

|           | `COL 1`        | `COL 2`      | `COL 3`       | `COL 4`        |
| --------- | -------------- | ------------ | ------------- | -------------- |
| **`ROW 1`** | **0** · U1 · `1`  | **1** · U2 · `2`  | **2** · U3 · `3`  | **3** · U4 · `4`   |
| **`ROW 2`** | **4** · U5 · `5`  | **5** · U6 · `6`  | **6** · U7 · `7`  | **7** · U8 · `8`   |
| **`ROW 3`** | **8** · U9 · `9`  | **9** · U10 · `A` | **10** · U11 · `B` | **11** · knob SW   |
| **`ROW 4`** | **12** · U12 · `D` | **13** · U13 · `E` | **14** · U14 · `F` | **15** · U15 · `G` |
| **`ROW 5`** | **16** · U16 · `H` | — unused     | **18** · U17 · `I` | — unused       |

Indices **17** and **19** have no switch — they are the second halves of the 2U `U16` and
`U15` footprints, and carry `0` in `labels[]`. Index **11** is the knob's push button, not a
key on the grid; it defaults to `screenKey`, so it cycles screens instead of sending a
keystroke.

### Pins

| Signal | GPIO | Notes |
| ------ | ---- | ----- |
| `ROW 1`–`ROW 5` | `1` `2` `42` `41` `40` | Scan outputs |
| `COL 1`–`COL 4` | `39` `17` `18` `47` | Inputs, pulled up |
| `KNOB A` / `KNOB B` | `4` / `5` | Encoder quadrature |
| `GND` | `GND` | Shared return |

These replace the `TODO: placeholder pins` still in the firmware. Three of the placeholders
cannot stay:

| Placeholder | Problem | Use instead |
| ----------- | ------- | ----------- |
| `GPIO 45` (col 2) | Sets `VDD_SPI` and must read low at reset — a pull-up or stuck key here can stop the board booting | `GPIO 17` |
| `GPIO 48` (col 3) | On-board RGB LED on DevKitC-1 v1.0 (v1.1 moves it to `GPIO 38`, so avoid both) | `GPIO 18` |
| `GPIO 20` (knob A) | Native USB D+ — `GPIO 19`/`20` are the USB pair | `GPIO 4` / `GPIO 5` |

Rows `1` `2` `40` `41` `42` and col `39` `47` are fine as they stand. Nothing here collides
with the display's `3` `8`–`14` `46`.

Diode polarity has to match the scan direction: with rows driven low and columns read,
current flows row → switch → diode → column, so every cathode band faces its column.
If no key registers but shorting a row to a column by hand does, the diodes run the other
way — swap `rowPins` and `colPins` in the `Adafruit_Keypad` constructor rather than
reworking 18 parts.


## KEYPAD and EC11 rotary encoder

The encoder plugs into `KNOBRIGHT1`, a 5-pin header on the right edge of the keypad PCB.
Quadrature goes to the ESP32, but the push button is wired into the key matrix — it costs
no extra GPIO.

```
        EC11                        Keypad PCB
        ====                        ==========

        A  (CLK) ───────────────── KNOBRIGHT1 1 ──── CN2 10 ──▶ GPIO 4
        C  (common) ────────────── KNOBRIGHT1 2 ──── GND
        B  (DT) ────────────────── KNOBRIGHT1 3 ──── CN2 11 ──▶ GPIO 5

        SW ──┬─────────────────── KNOBRIGHT1 4 ──── ROW 3
             └── ▶|── D18 ─────── KNOBRIGHT1 5 ──── COL 4
```

| EC11 pin | Goes to | Notes |
| -------- | ------- | ----- |
| `A` / `CLK` | `GPIO 4` | `INPUT_PULLUP`, interrupt on `CHANGE` |
| `B` / `DT`  | `GPIO 5` | Same; `RotaryEncoder` runs `LatchMode::FOUR3` for one step per detent |
| `C` | `GND` | Encoder common |
| `SW` | `ROW 3` × `COL 4` via `D18` | Key index **11** — no dedicated GPIO |

The push button is why the PCB carries 18 diodes for 17 switches: `D18` sits beside the
knob header rather than under a key. Detents that double-step or skip usually mean the
latch mode is wrong for the part, not a wiring fault — `FOUR3` suits a detented EC11, and
`RotaryEncoder` needs both pins on interrupts for `encoder.tick()` to keep up.