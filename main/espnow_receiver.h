#ifndef ESPNOW_RECEIVER_H
#define ESPNOW_RECEIVER_H

#include <stdint.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

// Joystick packet structure - must match transmitter exactly
typedef struct __attribute__((packed)) {
    int16_t joy1_x;
    int16_t joy1_y;
    int16_t joy2_x;
    int16_t joy2_y;
    uint8_t joy1_button;
    uint8_t joy2_button;
} joystick_packet_t;

// Initialize WiFi in STA mode (no connection) and ESP-NOW receiver.
// Received packets are posted to packet_queue via xQueueOverwrite.
// packet_queue must be created with a length of 1 and item size sizeof(joystick_packet_t).
bool espnow_init(QueueHandle_t packet_queue);

#endif
