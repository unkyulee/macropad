"""Generate the YouTube GIF with its default keymap baked into the pixels.

Uses only Python's stdlib. Edit KEYMAP here to make a custom guide; the
firmware simply plays the resulting GIF and does not render any key labels.
"""
from pathlib import Path
import struct


def encode(pixels):
    # Reset before the dictionary needs ten-bit codes. This keeps the
    # encoder small while still compressing the large solid-color regions.
    codes = [256]
    table = {bytes([i]): i for i in range(256)}
    word = b""
    for pixel in pixels:
        char = bytes([pixel])
        if word + char in table:
            word += char
            continue
        codes.append(table[word])
        table[word + char] = len(table) + 2
        word = char
        if len(table) >= 500:
            codes.append(256)
            table = {bytes([i]): i for i in range(256)}
    if word:
        codes.append(table[word])
    codes.append(257)
    packed = bytearray()
    bits = count = 0
    for code in codes:
        bits |= code << count
        count += 9
        while count >= 8:
            packed.append(bits & 255)
            bits >>= 8
            count -= 8
    if count:
        packed.append(bits)
    return b"\x08" + b"".join(
        bytes([len(packed[i:i + 255])]) + packed[i:i + 255]
        for i in range(0, len(packed), 255)
    ) + b"\x00"


# A small pixel font keeps the guide crisp at the display's native resolution.
FONT = dict(zip("ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789/+*- .", [
    "0E11111F111111", "1E11111E11111E", "0E11101010110E",
    "1E11111111111E", "1F10101E10101F", "1F10101E101010",
    "0E11101711110F", "1111111F111111", "0E04040404040E",
    "0702020212120C", "11121418141211", "1010101010101F",
    "111B1515111111", "11191513111111", "0E11111111110E",
    "1E11111E101010", "0E11111115120D", "1E11111E141211",
    "0F10100E01011E", "1F040404040404", "1111111111110E",
    "11111111110A04", "11111115151B11", "11110A040A1111",
    "11110A04040404", "1F01020408101F",
    "0E11131519110E", "040C040404040E", "0E11010204081F",
    "1E01010601011E", "02060A121F0202", "1F10101E01011E",
    "0E10101E11110E", "1F010204080808", "0E11110E11110E",
    "0E11110F01010E", "01010204081010", "0004041F040400",
    "00150E1F0E1500", "0000001F000000", "00000000000000",
    "00000000000C0C",
]))

KEYMAP = [
    ("NUM", "EXIT FULL"), ("/", "CAPTIONS"), ("*", "MUTE"), ("-", "VOLUME -"),
    ("7", "START"), ("8", "PREV VIDEO"), ("9", "FULLSCREEN"), ("+", "VOLUME +"),
    ("4", "BACK 10S"), ("5", "PLAY/PAUSE"), ("6", "FWD 10S"), ("KNOB", "NEXT SCREEN"),
    ("1", "SLOWER"), ("2", "NEXT VIDEO"), ("3", "FASTER"), ("ENTER", "FULLSCREEN"),
    ("0", "PLAY/PAUSE"), ("", ""), (".", "THEATER"), ("", ""),
]

width, height = 320, 216  # landscape body area, above the firmware footer


def rect(pixels, x, y, w, h, color):
    assert 0 <= x <= x + w <= width and 0 <= y <= y + h <= height
    for row in range(y, y + h):
        pixels[row * width + x:row * width + x + w] = [color] * w


def text(pixels, label, x, y, color=2, scale=1):
    for char in label:
        glyph = FONT[char]
        for row in range(7):
            bits = int(glyph[row * 2:row * 2 + 2], 16)
            for col in range(5):
                if bits & (1 << (4 - col)):
                    rect(pixels, x + col * scale, y + row * scale, scale, scale, color)
        x += 6 * scale


base = [0] * (width * height)
# Small play badge leaves most of the canvas available for the key guide.
rect(base, 8, 8, 44, 28, 1)
for x in range(24, 39):
    half = (38 - x) * 9 // 14
    rect(base, x, 22 - half, 1, 2 * half + 1, 2)
text(base, "YOUTUBE", 64, 8, scale=2)
text(base, "REMOTE / DEFAULT KEYS", 64, 28, color=4)
for index, (key, action) in enumerate(KEYMAP):
    if not key:
        continue
    x, y = (index % 4) * 80 + 4, 48 + (index // 4) * 30
    rect(base, x, y, 72, 1, 3)
    text(base, key, x + 3, y + 4, color=4)
    text(base, action, x + 3, y + 16)
text(base, "TURN KNOB / VOLUME", 8, 205, color=4)

gif = bytearray(b"GIF89a" + struct.pack("<HHBBB", width, height, 0xF7, 0, 0))
gif += bytes([0, 0, 0, 255, 0, 0, 255, 255, 255, 60, 60, 60, 80, 210, 230]) + bytes(251 * 3)
gif += b"\x21\xff\x0bNETSCAPE2.0\x03\x01\x00\x00\x00"
for frame in range(8):
    pixels = base.copy()
    rect(pixels, 8, 41, 304, 2, 3)
    rect(pixels, 8, 41, (frame + 1) * 38, 2, 1)
    gif += b"\x21\xf9\x04\x04\x0c\x00\x00\x00"
    gif += b"\x2c" + struct.pack("<HHHHB", 0, 0, width, height, 0)
    gif += encode(pixels)
gif += b"\x3b"

root = Path(__file__).resolve().parents[1]
asset = root / "data/gif/youtube.gif"
asset.parent.mkdir(parents=True, exist_ok=True)
asset.write_bytes(gif)
print(f"Generated {len(gif)} bytes, {width}x{height}, 8 frames")
