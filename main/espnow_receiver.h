#ifndef ESPNOW_RECEIVER_H
#define ESPNOW_RECEIVER_H

#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "joystick_packet.h"

// Initialize ESP-NOW receiver.
// WiFi must be initialized and started before calling this.
// Received packets are posted to packet_queue via xQueueOverwrite.
bool espnow_init(QueueHandle_t packet_queue);

#endif
