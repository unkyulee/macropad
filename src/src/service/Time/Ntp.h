#pragma once

// Time synchronisation. The clock screen owns the timezone, the network
// layer owns the moment a sync becomes possible, so the two are kept
// apart here: SNTP always runs in UTC and the zone is applied on top.

// Start SNTP. Safe to call on every connect, it only acts once.
void ntp_begin();

// Apply a POSIX TZ string, e.g. "EST5EDT,M3.2.0,M11.1.0". Remembered so a
// later ntp_begin() cannot reset the zone back to UTC.
void ntp_set_zone(const char *tz);

// True once a real time has arrived, rather than the 1970 the clock starts at.
bool ntp_synced();
