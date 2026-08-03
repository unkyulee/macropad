#include "Ntp.h"
#include "app/app.h"

#include <time.h>

// any timestamp past this is clearly a real one and not the epoch the
// system clock starts at
#define PLAUSIBLE_TIME 1700000000

static bool _started = false;
static char _zone[48] = "UTC0";

static void apply_zone()
{
    setenv("TZ", _zone, 1);
    tzset();
}

void ntp_set_zone(const char *tz)
{
    if (tz == NULL || tz[0] == 0)
        tz = "UTC0";

    if (strcmp(_zone, tz) == 0)
        return;

    strlcpy(_zone, tz, sizeof(_zone));
    apply_zone();

    _log("Timezone set to %s\n", _zone);
}

void ntp_begin()
{
    if (_started)
        return;

    // configTime with a zero offset starts SNTP in UTC. The zone is
    // re-applied straight after because configTime overwrites TZ.
    configTime(0, 0, "pool.ntp.org", "time.nist.gov");
    apply_zone();

    _started = true;
    _log("NTP started, timezone %s\n", _zone);
}

bool ntp_synced()
{
    return time(nullptr) > PLAUSIBLE_TIME;
}
