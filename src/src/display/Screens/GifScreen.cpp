#include "GifScreen.h"
#include "display/display.h"
#include "app/app.h"
#include "app/Config/Config.h"

#include <AnimatedGIF.h>
#include "app/FileSystem/FileSystem.h"

static AnimatedGIF _gif;
static File _file;
static bool _open = false;
static int _offsetX = 0;
static int _offsetY = 0;
static unsigned long _nextFrame = 0;

// AnimatedGIF hands back one scan line at a time with a palette, so the
// line is expanded into 16 bit colour and pushed as a 1 pixel high image.
static void GIFDraw(GIFDRAW *pDraw)
{
    TFT_eSPI &tft = display_tft();

    uint16_t line[320];
    uint8_t *source = pDraw->pPixels;
    uint16_t *palette = pDraw->pPalette;

    int width = pDraw->iWidth;
    if (width > 320)
        width = 320;

    int y = _offsetY + pDraw->iY + pDraw->y;
    if (y < 0 || y >= tft.height())
        return;

    if (pDraw->ucDisposalMethod == 2) // restore to background
    {
        for (int x = 0; x < width; x++)
            if (source[x] == pDraw->ucTransparent)
                source[x] = pDraw->ucBackground;
        pDraw->ucHasTransparency = 0;
    }

    if (pDraw->ucHasTransparency)
    {
        // draw only the runs of opaque pixels so the previous frame shows
        // through the transparent ones
        int x = 0;
        while (x < width)
        {
            while (x < width && source[x] == pDraw->ucTransparent)
                x++;

            int start = x;
            int count = 0;
            while (x < width && source[x] != pDraw->ucTransparent)
            {
                line[count++] = palette[source[x]];
                x++;
            }

            if (count)
                tft.pushImage(_offsetX + pDraw->iX + start, y, count, 1, line);
        }
    }
    else
    {
        for (int x = 0; x < width; x++)
            line[x] = palette[source[x]];

        tft.pushImage(_offsetX + pDraw->iX, y, width, 1, line);
    }
}

static void *GIFOpen(const char *name, int32_t *size)
{
    _file = gfs()->open(name, "r");
    if (!_file)
        return NULL;

    *size = _file.size();
    return (void *)&_file;
}

static void GIFClose(void *handle)
{
    (void)handle;
    if (_file)
        _file.close();
}

static int32_t GIFRead(GIFFILE *page, uint8_t *buffer, int32_t length)
{
    File *file = static_cast<File *>(page->fHandle);

    int32_t remaining = page->iSize - page->iPos;
    if (length > remaining)
        length = remaining;
    if (length <= 0)
        return 0;

    int32_t read = file->read(buffer, length);
    page->iPos = file->position();
    return read;
}

static int32_t GIFSeek(GIFFILE *page, int32_t position)
{
    File *file = static_cast<File *>(page->fHandle);
    file->seek(position);
    page->iPos = file->position();
    return page->iPos;
}

void GifScreen_close()
{
    if (_open)
    {
        _gif.close();
        _open = false;
    }
}

void GifScreen_setup(int slot)
{
    TFT_eSPI &tft = display_tft();

    config_lock();
    String path = config()["screens"][slot]["file"].as<String>();
    config_unlock();

    display_header("GIF");
    display_clear_body();

    GifScreen_close();

    if (path.length() == 0 || !fs_ready() || !gfs()->exists(path.c_str()))
    {
        tft.setTextColor(TFT_ORANGE, TFT_BLACK);
        tft.drawString("No GIF uploaded", 20, BODY_TOP + 40, 4);
        tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
        tft.drawString("Upload one from the web interface", 20, BODY_TOP + 80, 2);
        return;
    }

    // BIG_ENDIAN_PIXELS hands back a palette already in SPI byte order, so
    // TFT_eSPI must not swap it a second time
    tft.setSwapBytes(false);
    _gif.begin(BIG_ENDIAN_PIXELS);

    if (!_gif.open(path.c_str(), GIFOpen, GIFClose, GIFRead, GIFSeek, GIFDraw))
    {
        tft.setTextColor(TFT_RED, TFT_BLACK);
        tft.drawString("Cannot decode GIF", 20, BODY_TOP + 40, 4);
        tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
        tft.drawString(path, 20, BODY_TOP + 80, 2);
        _log("GIF open failed: %s (error %d)\n", path.c_str(), _gif.getLastError());
        return;
    }

    // centre it in the body area
    _offsetX = (tft.width() - _gif.getCanvasWidth()) / 2;
    _offsetY = BODY_TOP + (tft.height() - BODY_TOP - _gif.getCanvasHeight()) / 2;
    if (_offsetX < 0)
        _offsetX = 0;
    if (_offsetY < BODY_TOP)
        _offsetY = BODY_TOP;

    _open = true;
    _nextFrame = 0;

    _log("Playing %s (%dx%d)\n", path.c_str(), _gif.getCanvasWidth(), _gif.getCanvasHeight());
}

void GifScreen_render(int slot)
{
    (void)slot;

    if (!_open)
        return;

    // playFrame reports how long the frame should stay up, so the
    // animation runs at its own pace instead of the display pass rate
    if (millis() < _nextFrame)
        return;

    int delayMs = 0;
    if (_gif.playFrame(false, &delayMs) == 0)
    {
        // end of the animation, loop it
        _gif.reset();
        delayMs = 0;
    }

    _nextFrame = millis() + (delayMs > 0 ? delayMs : 20);
}

bool GifScreen_key(int index, bool pressed)
{
    (void)index;
    (void)pressed;

    // decoration only, keys keep their host bindings
    return false;
}
