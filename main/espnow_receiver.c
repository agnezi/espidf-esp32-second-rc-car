#include "espnow_receiver.h"
#include "esp_wifi.h"
#include "esp_now.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "secrets.h"
#include <string.h>

static const char *TAG = "ESPNOW";

// Allowed transmitter MAC address — set in secrets.h (gitignored)
static const uint8_t ALLOWED_MAC[] = TRANSMITTER_MAC;

static QueueHandle_t s_packet_queue;

static void on_data_recv(const esp_now_recv_info_t *recv_info, const uint8_t *data, int len) {
    const uint8_t *src = recv_info->src_addr;

    if (memcmp(src, ALLOWED_MAC, 6) != 0) {
        ESP_LOGW(TAG, "Ignored packet from unknown MAC: %02X:%02X:%02X:%02X:%02X:%02X",
                 src[0], src[1], src[2], src[3], src[4], src[5]);
        return;
    }

    if (len != sizeof(joystick_packet_t)) {
        ESP_LOGW(TAG, "Ignored packet: wrong size %d (expected %d)",
                 len, (int)sizeof(joystick_packet_t));
        return;
    }

    joystick_packet_t packet;
    memcpy(&packet, data, sizeof(joystick_packet_t));

    ESP_LOGD(TAG, "rx: j1(%d,%d) j2(%d,%d) btn(%u,%u)",
             packet.joy1_x, packet.joy1_y,
             packet.joy2_x, packet.joy2_y,
             packet.joy1_button, packet.joy2_button);

    // Overwrite any unread packet — control task always gets the latest position
    xQueueOverwrite(s_packet_queue, &packet);
}

bool espnow_init(QueueHandle_t packet_queue) {
    s_packet_queue = packet_queue;

    // Initialize NVS (required by WiFi driver)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        ret = nvs_flash_init();
    }
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "NVS init failed: %s", esp_err_to_name(ret));
        return false;
    }

    // Initialize event loop (required by WiFi)
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // Initialize WiFi
    wifi_init_config_t wifi_cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&wifi_cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());

    // Print this device's MAC and the transmitter MAC it will accept
    uint8_t mac[6];
    esp_wifi_get_mac(WIFI_IF_STA, mac);
    ESP_LOGI(TAG, "This device MAC:      %02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    ESP_LOGI(TAG, "Expecting TX MAC:     %02X:%02X:%02X:%02X:%02X:%02X",
             ALLOWED_MAC[0], ALLOWED_MAC[1], ALLOWED_MAC[2],
             ALLOWED_MAC[3], ALLOWED_MAC[4], ALLOWED_MAC[5]);

    uint8_t ch;
    wifi_second_chan_t second;
    esp_wifi_get_channel(&ch, &second);
    ESP_LOGI(TAG, "WiFi channel: %d (TX must use the same)", ch);

    // Initialize ESP-NOW
    if (esp_now_init() != ESP_OK) {
        ESP_LOGE(TAG, "Init failed");
        return false;
    }

    esp_now_register_recv_cb(on_data_recv);
    ESP_LOGI(TAG, "Receiver initialized, waiting for packets...");
    return true;
}
