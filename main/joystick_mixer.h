#ifndef JOYSTICK_MIXER_H
#define JOYSTICK_MIXER_H

#include <stdint.h>

// Input range from transmitter (12-bit ADC centered at 0 after deadzone)
#define JOYSTICK_INPUT_MAX 2048

// PWM max duty cycle (8-bit resolution)
#define MOTOR_MAX_SPEED 255

// Set to -1 to invert Y-axis if joystick up sends negative values
#define THROTTLE_DIRECTION 1

typedef struct {
    int16_t left;   // -255 to +255, negative = reverse
    int16_t right;  // -255 to +255, negative = reverse
} motor_output_t;

// Arcade-drive mixing: throttle + steering -> left/right motor speeds
// Input: pre-centered, deadzoned values from transmitter (~-2048 to +2047)
// Output: motor speeds in range -255 to +255
motor_output_t arcade_mix(int16_t throttle, int16_t steering);

#endif
