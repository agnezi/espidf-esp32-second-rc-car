#ifndef JOYSTICK_PACKET_H
#define JOYSTICK_PACKET_H

#include <stdint.h>

// Joystick packet structure — shared by ESP-NOW and UDP receivers.
// Must match transmitter format exactly: 10 bytes packed.
typedef struct __attribute__((packed)) {
    int16_t joy1_x;
    int16_t joy1_y;
    int16_t joy2_x;
    int16_t joy2_y;
    uint8_t joy1_button;
    uint8_t joy2_button;
} joystick_packet_t;

#define JOYSTICK_PACKET_SIZE sizeof(joystick_packet_t)

#endif
