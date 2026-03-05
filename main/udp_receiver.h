#ifndef UDP_RECEIVER_H
#define UDP_RECEIVER_H

#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

// Start the UDP receiver task.
// WiFi AP must be initialized and started before calling this.
// Valid joystick packets are posted to packet_queue via xQueueOverwrite.
bool udp_receiver_init(QueueHandle_t packet_queue);

#endif
