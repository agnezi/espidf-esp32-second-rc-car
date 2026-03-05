#include "espnow_receiver.h"
#include "input_lock.h"
#include "esp_now.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_mac.h"
#include <string.h>
#include <stdlib.h>

static const char *TAG = "ESPNOW";

static uint8_t s_allowed_mac[6];
static QueueHandle_t s_packet_queue;

// Parse "AA:BB:CC:DD:EE:FF" into a 6-byte array. Returns true on success.
static bool parse_mac(const char *str, uint8_t *out) {
    unsigned int m[6];
    if (sscanf(str, "%02x:%02x:%02x:%02x:%02x:%02x",
               &m[0], &m[1], &m[2], &m[3], &m[4], &m[5]) != 6) {
        return false;
    }
    for (int i = 0; i < 6; i++) out[i] = (uint8_t)m[i];
    return true;
}

static void on_data_recv(const esp_now_recv_info_t *recv_info, const uint8_t *data, int len) {
    const uint8_t *src = recv_info->src_addr;

    if (memcmp(src, s_allowed_mac, 6) != 0) {
        ESP_LOGW(TAG, "Ignored packet from unknown MAC: %02X:%02X:%02X:%02X:%02X:%02X",
                 src[0], src[1], src[2], src[3], src[4], src[5]);
        return;
    }

    if (len != sizeof(joystick_packet_t)) {
        ESP_LOGW(TAG, "Ignored packet: wrong size %d (expected %d)",
                 len, (int)sizeof(joystick_packet_t));
        return;
    }

    if (!input_lock_try_claim(INPUT_ESPNOW)) {
        return;
    }

    joystick_packet_t packet;
    memcpy(&packet, data, sizeof(joystick_packet_t));

    ESP_LOGD(TAG, "rx: j1(%d,%d) j2(%d,%d) btn(%u,%u)",
             packet.joy1_x, packet.joy1_y,
             packet.joy2_x, packet.joy2_y,
             packet.joy1_button, packet.joy2_button);

    xQueueOverwrite(s_packet_queue, &packet);
}

bool espnow_init(QueueHandle_t packet_queue) {
    s_packet_queue = packet_queue;

    // Parse transmitter MAC from Kconfig
    if (!parse_mac(CONFIG_RC_TRANSMITTER_MAC, s_allowed_mac)) {
        ESP_LOGE(TAG, "Invalid transmitter MAC in config: %s", CONFIG_RC_TRANSMITTER_MAC);
        return false;
    }

    ESP_LOGI(TAG, "Expecting TX MAC: %02X:%02X:%02X:%02X:%02X:%02X",
             s_allowed_mac[0], s_allowed_mac[1], s_allowed_mac[2],
             s_allowed_mac[3], s_allowed_mac[4], s_allowed_mac[5]);

    uint8_t ch;
    wifi_second_chan_t second;
    esp_wifi_get_channel(&ch, &second);
    ESP_LOGI(TAG, "WiFi channel: %d (TX must use the same)", ch);

    if (esp_now_init() != ESP_OK) {
        ESP_LOGE(TAG, "ESP-NOW init failed");
        return false;
    }

    esp_now_register_recv_cb(on_data_recv);
    ESP_LOGI(TAG, "Receiver initialized, waiting for packets...");
    return true;
}
