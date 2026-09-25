#pragma once

#include <Arduino.h>

void wifi_setup();
void wifi_loop();

// Reconnect using the credentials currently in the configuration.
void wifi_reconnect();

// Called on the network task after sending the save response.
void wifi_request_reconnect();
