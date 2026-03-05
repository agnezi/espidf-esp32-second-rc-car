#include "udp_receiver.h"
#include "joystick_packet.h"
#include "input_lock.h"
#include "esp_log.h"
#include "lwip/sockets.h"
#include <string.h>

static const char *TAG = "UDP";

#define UDP_TASK_STACK_SIZE 4096
#define UDP_TASK_PRIORITY   4

static QueueHandle_t s_packet_queue;

static void udp_recv_task(void *pv) {
    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock < 0) {
        ESP_LOGE(TAG, "Failed to create socket: errno %d", errno);
        vTaskDelete(NULL);
        return;
    }

    struct sockaddr_in addr = {
        .sin_family      = AF_INET,
        .sin_port        = htons(CONFIG_RC_UDP_PORT),
        .sin_addr.s_addr = htonl(INADDR_ANY),
    };

    if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        ESP_LOGE(TAG, "Socket bind failed: errno %d", errno);
        close(sock);
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(TAG, "Listening on port %d", CONFIG_RC_UDP_PORT);

    uint8_t buf[64];
    bool first_packet = true;

    while (true) {
        struct sockaddr_in src_addr;
        socklen_t src_len = sizeof(src_addr);
        int len = recvfrom(sock, buf, sizeof(buf), 0,
                           (struct sockaddr *)&src_addr, &src_len);
        if (len < 0) {
            ESP_LOGE(TAG, "recvfrom failed: errno %d", errno);
            continue;
        }

        char src_ip[INET_ADDRSTRLEN];
        inet_ntoa_r(src_addr.sin_addr, src_ip, sizeof(src_ip));

        ESP_LOGI(TAG, "Packet received from %s:%d (%d bytes)",
                 src_ip, ntohs(src_addr.sin_port), len);

        if (len != JOYSTICK_PACKET_SIZE) {
            ESP_LOGW(TAG, "Ignored packet: wrong size %d (expected %d)",
                     len, (int)JOYSTICK_PACKET_SIZE);
            continue;
        }

        if (!input_lock_try_claim(INPUT_UDP)) {
            continue;
        }

        joystick_packet_t packet;
        memcpy(&packet, buf, sizeof(joystick_packet_t));

        if (first_packet) {
            first_packet = false;
            ESP_LOGI(TAG, "First valid packet from %s:%d — UDP input active",
                     src_ip, ntohs(src_addr.sin_port));
        }

        ESP_LOGD(TAG, "rx: j1(%d,%d) j2(%d,%d) btn(%u,%u)",
                 packet.joy1_x, packet.joy1_y,
                 packet.joy2_x, packet.joy2_y,
                 packet.joy1_button, packet.joy2_button);

        xQueueOverwrite(s_packet_queue, &packet);
    }
}

bool udp_receiver_init(QueueHandle_t packet_queue) {
    s_packet_queue = packet_queue;

    BaseType_t ret = xTaskCreate(udp_recv_task, "udp", UDP_TASK_STACK_SIZE,
                                 NULL, UDP_TASK_PRIORITY, NULL);
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create UDP task");
        return false;
    }

    return true;
}
