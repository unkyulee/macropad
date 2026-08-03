#include "CalculatorScreen.h"
#include "display/display.h"
#include "app/app.h"
#include "app/Config/Config.h"

// The calculator reads the screen's own keymap: whichever keys are bound
// to "0".."9", ".", "+", "-", "*", "/", "=" and "C" become the calculator
// pad. Everything else on the screen still goes to the host as usual, so
// you can keep a few macro keys alongside the digits.
static char _keys[KEY_COUNT];

static String _entry = "0";
static double _accumulator = 0;
static char _operator = 0;
static bool _fresh = true; // next digit starts a new number
static bool _dirty = true;

static void reset()
{
    _entry = "0";
    _accumulator = 0;
    _operator = 0;
    _fresh = true;
    _dirty = true;
}

static double apply(double left, double right, char op)
{
    switch (op)
    {
    case '+':
        return left + right;
    case '-':
        return left - right;
    case '*':
        return left * right;
    case '/':
        return (right == 0) ? NAN : left / right;
    }
    return right;
}

// Trim a double back to something that fits the display without the
// trailing zero noise printf leaves behind.
static String format(double value)
{
    if (isnan(value) || isinf(value))
        return "error";

    char buffer[24];
    snprintf(buffer, sizeof(buffer), "%.6f", value);

    String text(buffer);
    if (text.indexOf('.') >= 0)
    {
        while (text.endsWith("0"))
            text.remove(text.length() - 1);
        if (text.endsWith("."))
            text.remove(text.length() - 1);
    }

    if (text.length() > 14)
    {
        snprintf(buffer, sizeof(buffer), "%.6g", value);
        text = buffer;
    }

    return text;
}

void CalculatorScreen_setup(int slot)
{
    config_lock();
    JsonArray keys = config()["screens"][slot]["keys"].as<JsonArray>();
    for (int i = 0; i < KEY_COUNT; i++)
    {
        String action = keys[i].as<String>();
        action.trim();

        // only single character bindings can be calculator keys, plus the
        // spelled out clear key
        if (action.length() == 1)
            _keys[i] = action.charAt(0);
        else if (action.equalsIgnoreCase("CLEAR") || action.equalsIgnoreCase("ESC"))
            _keys[i] = 'C';
        else if (action.equalsIgnoreCase("ENTER") || action.equalsIgnoreCase("RETURN"))
            _keys[i] = '=';
        else if (action.equalsIgnoreCase("BACKSPACE"))
            _keys[i] = '<';
        else
            _keys[i] = 0;
    }
    config_unlock();

    reset();

    display_header("CALCULATOR");
    display_clear_body();
}

void CalculatorScreen_render(int slot)
{
    (void)slot;

    if (!_dirty)
        return;
    _dirty = false;

    TFT_eSPI &tft = display_tft();

    // pending operation, shown small above the entry
    char pending[32];
    if (_operator)
        snprintf(pending, sizeof(pending), "%s %c", format(_accumulator).c_str(), _operator);
    else
        pending[0] = 0;

    tft.setTextDatum(TR_DATUM);

    tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
    tft.setTextPadding(tft.width() - 24);
    tft.drawString(pending, tft.width() - 16, BODY_TOP + 26, 4);

    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextPadding(tft.width() - 24);
    tft.drawString(_entry, tft.width() - 16, BODY_TOP + 70, 6);

    tft.setTextPadding(0);
    tft.setTextDatum(TL_DATUM);

    tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
    tft.drawString("digits and + - * / = C come from this screen's keymap",
                   16, BODY_TOP + 150, 2);
}

bool CalculatorScreen_key(int index, bool pressed)
{
    if (index < 0 || index >= KEY_COUNT || !pressed)
        return (index >= 0 && index < KEY_COUNT && _keys[index] != 0);

    char key = _keys[index];
    if (key == 0)
        return false; // not a calculator key, let it reach the host

    if (key >= '0' && key <= '9')
    {
        if (_fresh || _entry == "0")
        {
            _entry = String(key);
            _fresh = false;
        }
        else if (_entry.length() < 14)
            _entry += key;
    }
    else if (key == '.')
    {
        if (_fresh)
        {
            _entry = "0.";
            _fresh = false;
        }
        else if (_entry.indexOf('.') < 0)
            _entry += '.';
    }
    else if (key == '+' || key == '-' || key == '*' || key == '/')
    {
        double current = _entry.toDouble();
        _accumulator = _operator ? apply(_accumulator, current, _operator) : current;
        _operator = key;
        _entry = format(_accumulator);
        _fresh = true;
    }
    else if (key == '=')
    {
        if (_operator)
        {
            _accumulator = apply(_accumulator, _entry.toDouble(), _operator);
            _entry = format(_accumulator);
            _operator = 0;
        }
        _fresh = true;
    }
    else if (key == 'C')
    {
        reset();
    }
    else if (key == '<')
    {
        if (!_fresh && _entry.length() > 1)
            _entry.remove(_entry.length() - 1);
        else
            _entry = "0";
    }
    else
        return false;

    _dirty = true;
    return true; // consumed, do not send to the host
}
