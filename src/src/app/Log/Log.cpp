#include "Log.h"
#include <Arduino.h>

//
// APP LOG
//
void _log(const char *format, ...)
{
    // Buffer to hold the formatted message
    char message[512];

    va_list args;
    va_start(args, format);
    vsnprintf(message, sizeof(message), format, args);
    va_end(args);

    Serial.printf("[%d][%d] %s", xPortGetCoreID(), millis(), message);
}

//
// DEBUG LOG
//
void _debug(const char *format, ...)
{
#if defined(_DEBUG)
    char message[512];

    va_list args;
    va_start(args, format);
    vsnprintf(message, sizeof(message), format, args);
    va_end(args);

    Serial.printf("[%d][%d] %s", xPortGetCoreID(), millis(), message);
#endif
}
