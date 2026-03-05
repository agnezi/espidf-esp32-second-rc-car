#ifndef WIFI_INIT_H
#define WIFI_INIT_H

#include <stdbool.h>

// Initialize NVS, event loop, netif, and WiFi in AP+STA mode.
// Must be called before espnow_init() and udp_receiver_init().
bool wifi_init(void);

#endif
