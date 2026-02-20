#ifndef MOTOR_CONTROL_H
#define MOTOR_CONTROL_H

#include <stdint.h>

// Initialize LEDC PWM for all 4 motor pins. Call once at startup.
void motor_control_init(void);

// Drive both motors with signed speed values (-255 to +255)
// Positive = forward, negative = reverse, 0 = stop
void motor_control_drive(int16_t left_speed, int16_t right_speed);

// Emergency stop both motors
void motor_control_stop(void);

#endif
