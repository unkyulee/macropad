#include "KeymapScreen.h"
#include "display/display.h"
#include "app/app.h"
#include "app/Config/Config.h"

// 5 rows x 4 columns laid out over the body area
#define GRID_ROWS 5
#define GRID_COLS 4
#define CELL_W 78
#define CELL_H 40
#define GRID_X 6
#define GRID_Y (BODY_TOP + 2)

static String _labels[KEY_COUNT];
static int _highlight = -1;

static void draw_cell(int index, bool active)
{
    TFT_eSPI &tft = display_tft();

    int row = index / GRID_COLS;
    int col = index % GRID_COLS;
    int x = GRID_X + col * CELL_W;
    int y = GRID_Y + row * CELL_H;

    uint16_t background = active ? TFT_NAVY : TFT_BLACK;
    uint16_t border = active ? TFT_CYAN : 0x2124; // very dark grey

    tft.fillRoundRect(x + 1, y + 1, CELL_W - 3, CELL_H - 3, 4, background);
    tft.drawRoundRect(x + 1, y + 1, CELL_W - 3, CELL_H - 3, 4, border);

    String label = _labels[index];
    if (label.length() == 0)
        label = "-";

    // the cell is narrow, so shorten the long prefixes before truncating:
    // "CTRL+SHIFT+Z" reads better as "^@Z" than as "CTRL+S", and
    // "NUM_ASTERISK" as "N*" rather than "NUM_AST"
    label.replace("CTRL+", "^");
    label.replace("SHIFT+", "@");
    label.replace("ALT+", "!");
    label.replace("GUI+", "#");
    label.replace("NUM_LOCK", "NLOCK");
    label.replace("NUM_ASTERISK", "N*");
    label.replace("NUM_SLASH", "N/");
    label.replace("NUM_MINUS", "N-");
    label.replace("NUM_PLUS", "N+");
    label.replace("NUM_PERIOD", "N.");
    label.replace("NUM_ENTER", "N ENT");
    label.replace("NUM_", "N");
    if (label.length() > 7)
        label = label.substring(0, 7);

    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(active ? TFT_WHITE : TFT_SILVER, background);
    tft.drawString(label, x + CELL_W / 2, y + CELL_H / 2, 2);
    tft.setTextDatum(TL_DATUM);
}

void KeymapScreen_setup(int slot)
{
    config_lock();
    JsonArray keys = config()["screens"][slot]["keys"].as<JsonArray>();
    for (int i = 0; i < KEY_COUNT; i++)
        _labels[i] = keys[i].as<String>();
    config_unlock();

    display_clear_body();

    _highlight = -1;
    for (int i = 0; i < KEY_COUNT; i++)
        draw_cell(i, false);
}

void KeymapScreen_render(int slot)
{
    (void)slot;

    // only the cell under the finger changes, so nothing to do here unless
    // the highlight moved - handled in KeymapScreen_key
}

bool KeymapScreen_key(int index, bool pressed)
{
    if (index < 0 || index >= KEY_COUNT)
        return false;

    if (pressed)
    {
        if (_highlight >= 0 && _highlight != index)
            draw_cell(_highlight, false);

        _highlight = index;
        draw_cell(index, true);
    }
    else if (_highlight == index)
    {
        _highlight = -1;
        draw_cell(index, false);
    }

    // the point of this screen is to show what the keys do, so they still
    // reach the host
    return false;
}
