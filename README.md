# ESP32 RC Car — Receiver

ESP-IDF firmware for the receiver side of a two-ESP32 RC car. Receives joystick commands over ESP-NOW and drives two DC motors via PWM.

## Architecture

```
ESP-NOW callback (WiFi task)
        │  xQueueOverwrite
        ▼
 joystick_queue (size 1 — always latest)
        │  xQueueReceive (500 ms timeout = safety watchdog)
        ▼
 control_task (priority 5)  ──TaskNotify──▶  led_task (priority 1)
  packet received → arcade mix → PWM          connected  → solid ON
  timeout         → motors stop               disconnect → blink
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
- A matching transmitter running the companion firmware

## Setup

**1. Configure your transmitter MAC**

```sh
cp main/secrets.h.example main/secrets.h
```

Edit `main/secrets.h` and replace the placeholder with your transmitter's MAC address:

```c
#define TRANSMITTER_MAC {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF}
```

To find the MAC, flash the transmitter and look for a line like:
```
I ESPNOW: MAC Address: AA:BB:CC:DD:EE:FF
```

**2. Build and flash**

```sh
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor
```

## Packet format

The receiver expects packets from the transmitter matching this struct exactly:

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

## Safety

If no packet is received for 500 ms (transmitter out of range or powered off), both motors stop immediately. The LED switches from solid to blinking to indicate loss of signal.

## Debug logging

Packet receive logs are at `DEBUG` level (silent by default). To enable:

```sh
idf.py menuconfig
# Component config → Log output → Default log verbosity → Debug
```

Or at runtime:
```c
esp_log_level_set("ESPNOW", ESP_LOG_DEBUG);
```
