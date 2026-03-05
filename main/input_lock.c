#include "input_lock.h"
#include "esp_log.h"
#include <stdatomic.h>

static const char *TAG = "INPUT";

static atomic_int s_locked_source = INPUT_NONE;

static const char *source_name(input_source_t src) {
    switch (src) {
        case INPUT_ESPNOW: return "ESP-NOW";
        case INPUT_UDP:    return "UDP";
        default:           return "NONE";
    }
}

bool input_lock_try_claim(input_source_t source) {
    int expected = INPUT_NONE;
    if (atomic_compare_exchange_strong(&s_locked_source, &expected, (int)source)) {
        ESP_LOGI(TAG, "Input locked to %s", source_name(source));
        return true;
    }
    return expected == (int)source;
}

input_source_t input_lock_get(void) {
    return (input_source_t)atomic_load(&s_locked_source);
}
