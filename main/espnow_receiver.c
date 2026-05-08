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
#include <string.h>

static const char *TAG = "ESPNOW";

#define CHANNEL 1

typedef enum { MSG_DISCOVER = 1, MSG_HELLO = 2 } msg_type_t;

static const uint8_t BROADCAST[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
static volatile bool s_paired = false;
static QueueHandle_t s_packet_queue;

static void add_peer(const uint8_t *mac) {
    if (esp_now_is_peer_exist(mac)) return;
    esp_now_peer_info_t peer = {0};
    memcpy(peer.peer_addr, mac, 6);
    peer.channel = CHANNEL;
    peer.encrypt = false;
    esp_now_add_peer(&peer);
}

static void on_data_recv(const esp_now_recv_info_t *recv_info, const uint8_t *data, int len) {
    const uint8_t *src = recv_info->src_addr;

    if (len == 1) {
        if (data[0] == MSG_DISCOVER) {
            add_peer(src);
            uint8_t hello = MSG_HELLO;
            esp_now_send(src, &hello, 1);
            s_paired = true;
            ESP_LOGI(TAG, "Paired with %02X:%02X:%02X:%02X:%02X:%02X",
                     src[0], src[1], src[2], src[3], src[4], src[5]);
        }
        return;
    }

    if (!s_paired) return;

    if (len != sizeof(joystick_packet_t)) {
        ESP_LOGW(TAG, "Ignored packet: wrong size %d (expected %d)",
                 len, (int)sizeof(joystick_packet_t));
        return;
    }

    joystick_packet_t packet;
    memcpy(&packet, data, sizeof(joystick_packet_t));

    ESP_LOGI(TAG, "rx: j1(%d,%d) j2(%d,%d) btn(%u,%u)",
             packet.joy1_x, packet.joy1_y,
             packet.joy2_x, packet.joy2_y,
             packet.joy1_button, packet.joy2_button);

    xQueueOverwrite(s_packet_queue, &packet);
}

bool espnow_init(QueueHandle_t packet_queue) {
    s_packet_queue = packet_queue;

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        ret = nvs_flash_init();
    }
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "NVS init failed: %s", esp_err_to_name(ret));
        return false;
    }

    ESP_ERROR_CHECK(esp_event_loop_create_default());

    wifi_init_config_t wifi_cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&wifi_cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_set_channel(CHANNEL, WIFI_SECOND_CHAN_NONE));

    uint8_t mac[6];
    esp_wifi_get_mac(WIFI_IF_STA, mac);
    ESP_LOGI(TAG, "This device MAC: %02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    ESP_LOGI(TAG, "WiFi channel: %d — waiting for DISCOVER...", CHANNEL);

    if (esp_now_init() != ESP_OK) {
        ESP_LOGE(TAG, "Init failed");
        return false;
    }

    add_peer(BROADCAST);
    esp_now_register_recv_cb(on_data_recv);
    return true;
}
