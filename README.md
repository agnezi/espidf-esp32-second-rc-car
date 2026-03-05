# ESP32 RC Car — Receiver

ESP-IDF firmware for the receiver side of a two-ESP32 RC car. Accepts joystick commands over **ESP-NOW** (from a paired transmitter) or **UDP over WiFi** (from a Steam Deck) and drives two DC motors via PWM.

On boot, both inputs listen simultaneously. The **first valid packet** locks the input source for the session — the other is ignored until power cycle.

## Architecture

```
Power on → wifi_init() (STA mode, connects to home WiFi)
         → espnow_init(queue)       ← STA interface
         → udp_receiver_init(queue)  ← STA interface (Steam Deck on same LAN)

ESP-NOW callback ──┐
                   ├─ input_lock ─► xQueueOverwrite (size 1 — always latest)
UDP recv task ─────┘
                          │  xQueueReceive (500 ms timeout = safety watchdog)
                          ▼
                   control_task (priority 5)  ──TaskNotify──▶  led_task (priority 1)
                    packet → arcade mix → PWM                  connected  → solid ON
                    timeout → motors stop                      disconnect → blink
```

## Hardware

| Signal | GPIO |
|--------|------|
| Motor A IN1 | 16 |
| Motor A IN2 | 17 |
| Motor B IN1 | 18 |
| Motor B IN2 | 19 |
| Status LED | 2 |

Motor driver expects an H-bridge IC (e.g. L298N, DRV8833) wired to the IN pins. PWM is 1 kHz, 8-bit resolution.

## Requirements

- [ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/get-started/) v5.5+
- ESP32 board
- H-bridge motor driver
- A matching transmitter running the companion firmware (ESP-NOW), **or** a Steam Deck / any device sending UDP packets over WiFi

## Setup

**1. Configure the project**

```sh
idf.py menuconfig
```

Navigate to **RC Car Configuration** and set:

| Setting | Default | Description |
|---------|---------|-------------|
| Transmitter MAC | `00:00:00:00:00:00` | ESP-NOW transmitter MAC (must be set before flashing) |
| WiFi SSID | *(empty)* | WiFi network SSID (ESP32 and Steam Deck must be on same network) |
| WiFi Password | *(empty)* | WiFi network password |
| UDP Port | `4210` | Port for joystick packets from Steam Deck |
| WiFi Channel | `1` | Must match ESP-NOW transmitter channel |

To find the transmitter's MAC, flash the transmitter and look for:
```
I ESPNOW: MAC Address: AA:BB:CC:DD:EE:FF
```

**2. Build and flash**

```sh
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor
```

## UDP control (Steam Deck)

The ESP32 connects to your home WiFi in STA mode. Connect the Steam Deck to the same network and send 10-byte UDP packets to `<ESP32_IP>:<UDP_PORT>` (the IP is logged on boot).

### Packet format

The receiver expects the same packed struct used by ESP-NOW (little-endian):

```c
typedef struct __attribute__((packed)) {
    int16_t joy1_x;      // steering  (~-2048 to +2047)
    int16_t joy1_y;      // throttle  (~-2048 to +2047)
    int16_t joy2_x;
    int16_t joy2_y;
    uint8_t joy1_button;
    uint8_t joy2_button;
} joystick_packet_t;
```

### Test with Python

```python
import struct, socket

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
# joy1_x, joy1_y, joy2_x, joy2_y, btn1, btn2
packet = struct.pack('<hhhhBB', 0, 1000, 0, 0, 0, 0)  # forward throttle
sock.sendto(packet, ('<ESP32_IP>', 4210))
```

## Safety

- **Watchdog**: if no packet is received for 500 ms (transmitter out of range or powered off), both motors stop immediately. The LED switches from solid to blinking.
- **Input lock**: once the first valid packet arrives (ESP-NOW or UDP), that source is locked in. The other source is silently ignored until a power cycle.

## Debug logging

Packet receive logs are at `DEBUG` level (silent by default). To enable:

```sh
idf.py menuconfig
# Component config → Log output → Default log verbosity → Debug
```

Or at runtime:
```c
esp_log_level_set("ESPNOW", ESP_LOG_DEBUG);
esp_log_level_set("UDP", ESP_LOG_DEBUG);
```
