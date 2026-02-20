#include "motor_control.h"
#include "driver/ledc.h"
#include "esp_log.h"
#include <sys/param.h>

static const char *TAG = "MOTOR";

// GPIO pin assignments
#define MOTOR_A_IN1  16
#define MOTOR_A_IN2  17
#define MOTOR_B_IN1  18
#define MOTOR_B_IN2  19

// PWM configuration
#define PWM_FREQ       1000
#define PWM_RESOLUTION LEDC_TIMER_8_BIT
#define PWM_SPEED_MODE LEDC_LOW_SPEED_MODE

// LEDC channel assignments
#define CH_A1  LEDC_CHANNEL_0
#define CH_A2  LEDC_CHANNEL_1
#define CH_B1  LEDC_CHANNEL_2
#define CH_B2  LEDC_CHANNEL_3

static void set_duty(ledc_channel_t channel, uint32_t duty) {
    ledc_set_duty(PWM_SPEED_MODE, channel, duty);
    ledc_update_duty(PWM_SPEED_MODE, channel);
}

static void setup_channel(int gpio, ledc_channel_t channel) {
    ledc_channel_config_t cfg = {
        .gpio_num   = gpio,
        .speed_mode = PWM_SPEED_MODE,
        .channel    = channel,
        .intr_type  = LEDC_INTR_DISABLE,
        .timer_sel  = LEDC_TIMER_0,
        .duty       = 0,
        .hpoint     = 0,
    };
    ledc_channel_config(&cfg);
}

static void stop_motor(ledc_channel_t ch1, ledc_channel_t ch2) {
    set_duty(ch1, 0);
    set_duty(ch2, 0);
}

static void forward_motor(ledc_channel_t ch1, ledc_channel_t ch2, uint8_t speed) {
    set_duty(ch1, speed);
    set_duty(ch2, 0);
}

static void reverse_motor(ledc_channel_t ch1, ledc_channel_t ch2, uint8_t speed) {
    set_duty(ch1, 0);
    set_duty(ch2, speed);
}

void motor_control_init(void) {
    ledc_timer_config_t timer_cfg = {
        .speed_mode      = PWM_SPEED_MODE,
        .duty_resolution = PWM_RESOLUTION,
        .timer_num       = LEDC_TIMER_0,
        .freq_hz         = PWM_FREQ,
        .clk_cfg         = LEDC_AUTO_CLK,
    };
    ledc_timer_config(&timer_cfg);

    setup_channel(MOTOR_A_IN1, CH_A1);
    setup_channel(MOTOR_A_IN2, CH_A2);
    setup_channel(MOTOR_B_IN1, CH_B1);
    setup_channel(MOTOR_B_IN2, CH_B2);

    motor_control_stop();

    ESP_LOGI(TAG, "PWM motor control initialized");
    ESP_LOGI(TAG, "PWM: %d Hz, 8-bit resolution", PWM_FREQ);
    ESP_LOGI(TAG, "Motor A: GPIO %d (CH%d), GPIO %d (CH%d)", MOTOR_A_IN1, CH_A1, MOTOR_A_IN2, CH_A2);
    ESP_LOGI(TAG, "Motor B: GPIO %d (CH%d), GPIO %d (CH%d)", MOTOR_B_IN1, CH_B1, MOTOR_B_IN2, CH_B2);
}

void motor_control_stop(void) {
    stop_motor(CH_A1, CH_A2);
    stop_motor(CH_B1, CH_B2);
}

void motor_control_drive(int16_t left_speed, int16_t right_speed) {
    // Motor A = Left
    if (left_speed > 0) {
        forward_motor(CH_A1, CH_A2, (uint8_t)MIN(left_speed, 255));
    } else if (left_speed < 0) {
        reverse_motor(CH_A1, CH_A2, (uint8_t)MIN(-left_speed, 255));
    } else {
        stop_motor(CH_A1, CH_A2);
    }

    // Motor B = Right
    if (right_speed > 0) {
        forward_motor(CH_B1, CH_B2, (uint8_t)MIN(right_speed, 255));
    } else if (right_speed < 0) {
        reverse_motor(CH_B1, CH_B2, (uint8_t)MIN(-right_speed, 255));
    } else {
        stop_motor(CH_B1, CH_B2);
    }
}
