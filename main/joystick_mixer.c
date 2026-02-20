#include "joystick_mixer.h"
#include <stdlib.h>
#include <sys/param.h>

#define CLAMP(v, lo, hi) ((v) < (lo) ? (lo) : ((v) > (hi) ? (hi) : (v)))

static int16_t scaleToMotor(int16_t value) {
    return (int16_t)CLAMP(
        (int32_t)value * MOTOR_MAX_SPEED / JOYSTICK_INPUT_MAX,
        -MOTOR_MAX_SPEED, MOTOR_MAX_SPEED
    );
}

motor_output_t arcade_mix(int16_t throttle, int16_t steering) {
    motor_output_t output;

    // Apply direction setting and scale to motor range
    int16_t thr = scaleToMotor(throttle * THROTTLE_DIRECTION);
    int16_t str = scaleToMotor(steering);

    // Arcade mix
    int16_t left  = thr + str;
    int16_t right = thr - str;

    // Proportional scaling to preserve steering ratio at full throttle
    int16_t maxVal = MAX(abs(left), abs(right));
    if (maxVal > MOTOR_MAX_SPEED) {
        left  = (int16_t)((int32_t)left  * MOTOR_MAX_SPEED / maxVal);
        right = (int16_t)((int32_t)right * MOTOR_MAX_SPEED / maxVal);
    }

    output.left  = left;
    output.right = right;
    return output;
}
