# ESP32 RC Car — Receiver

ESP-IDF firmware for the receiver side of a two-ESP32 RC car. Receives joystick commands over ESP-NOW and drives two DC motors via PWM.

## Architecture

```
Transmitter broadcasts MSG_DISCOVER (1 byte) every 500 ms
        │
        ▼  ESP-NOW WiFi ch 1
Receiver responds MSG_HELLO (1 byte) → pairing done
        │  xQueueOverwrite
        ▼
 joystick_queue (size 1 — always latest)
        │  xQueueReceive (500 ms timeout = safety watchdog)
        ▼
 control_task (priority 5)  ──TaskNotify──▶  led_task (priority 1)
  packet received → arcade mix → PWM          connected  → solid ON
  timeout         → motors stop               disconnect → blink
```

No configuration needed — the receiver pairs automatically with any transmitter running the companion firmware.

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
- ESP32 board (standard Xtensa dual-core — not S2/S3/C3)
- H-bridge motor driver
- A matching transmitter running [`espidf-esp32-joystick-universal`](https://github.com/agnezi/espidf-esp32-joystick-universal)

## Setup

**1. Build and flash**

```sh
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor
```

No pre-configuration required. Power on both devices and they pair automatically.

**2. Pairing**

1. Receiver starts and blinks the LED — waiting for a transmitter
2. Enable search on the transmitter (toggle button)
3. Transmitter broadcasts `DISCOVER` on WiFi channel 1
4. Receiver responds with `HELLO` — pairing complete
5. Transmitter starts sending joystick packets — receiver LED goes solid

If signal is lost (500 ms without a packet), motors stop and the LED blinks. Pairing resumes automatically when the signal returns. If the transmitter restarts, it re-pairs without any intervention.

## Packet format

The receiver expects joystick packets matching this struct exactly (10 bytes, no padding):

```c
typedef struct __attribute__((packed)) {
    int16_t joy1_x;      // joystick 1 X axis: -100..100 (steering)
    int16_t joy1_y;      // joystick 1 Y axis: -100..100 (throttle)
    int16_t joy2_x;      // joystick 2 X axis: -100..100
    int16_t joy2_y;      // joystick 2 Y axis: -100..100
    uint8_t joy1_button; // joystick 1 button: 0 or 1 (toggle)
    uint8_t joy2_button; // joystick 2 button: 0 or 1 (toggle)
} joystick_packet_t;
```

`joy1_y` (throttle) and `joy1_x` (steering) are fed into an arcade drive mixer that scales the -100..100 input range to -255..255 motor PWM duty cycle.

## Safety

If no packet is received for 500 ms (transmitter out of range or powered off), both motors stop immediately. The LED switches from solid to blinking to indicate loss of signal.

## Debug logging

Received packet data is logged at `INFO` level and visible by default in the serial monitor.
