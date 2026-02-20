#include "espnow_receiver.h"
#include "joystick_mixer.h"
#include "motor_control.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

static const char *TAG = "MAIN";

#define SAFETY_TIMEOUT_MS     500
#define LED_PIN               GPIO_NUM_2
#define LED_BLINK_INTERVAL_MS 300

static TaskHandle_t s_led_task_handle;

// LED task: blinks while disconnected, solid when connected.
// Receives task notifications: 1 = connected, 0 = disconnected.
static void led_task(void *pv) {
    bool connected = false;
    bool level = false;

    while (true) {
        uint32_t val;
        BaseType_t notified = xTaskNotifyWait(0, 0, &val, pdMS_TO_TICKS(LED_BLINK_INTERVAL_MS));

        if (notified == pdTRUE) {
            connected = (val == 1);
            gpio_set_level(LED_PIN, connected ? 1 : 0);
            level = connected;
        } else if (!connected) {
            // Timeout with no notification — blink
            level = !level;
            gpio_set_level(LED_PIN, level);
        }
    }
}

// Control task: blocks on joystick queue.
// Drives motors on each received packet; stops them on safety timeout.
static void control_task(void *pv) {
    QueueHandle_t queue = (QueueHandle_t)pv;
    joystick_packet_t packet;
    bool connected = false;

    while (true) {
        if (xQueueReceive(queue, &packet, pdMS_TO_TICKS(SAFETY_TIMEOUT_MS)) == pdTRUE) {
            motor_output_t out = arcade_mix(packet.joy1_y, packet.joy1_x);
            motor_control_drive(out.left, out.right);

            if (!connected) {
                connected = true;
                ESP_LOGI(TAG, "Transmitter connected.");
                xTaskNotify(s_led_task_handle, 1, eSetValueWithOverwrite);
            }
        } else {
            // Safety timeout — no packet received within SAFETY_TIMEOUT_MS
            motor_control_stop();

            if (connected) {
                connected = false;
                ESP_LOGW(TAG, "Transmitter lost. Waiting for reconnect...");
                xTaskNotify(s_led_task_handle, 0, eSetValueWithOverwrite);
            }
        }
    }
}

void app_main(void) {
    gpio_set_direction(LED_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(LED_PIN, 0);

    ESP_LOGI(TAG, "=== ESP32 RC Car (ESP-NOW) ===");

    motor_control_init();

    QueueHandle_t joystick_queue = xQueueCreate(1, sizeof(joystick_packet_t));

    if (!espnow_init(joystick_queue)) {
        ESP_LOGE(TAG, "ESP-NOW init failed! Halting.");
        while (true) { vTaskDelay(pdMS_TO_TICKS(1000)); }
    }

    xTaskCreate(led_task,     "led",  1024, NULL,           1, &s_led_task_handle);
    xTaskCreate(control_task, "ctrl", 4096, joystick_queue, 5, NULL);

    ESP_LOGI(TAG, "Ready. Waiting for joystick input...");
}
