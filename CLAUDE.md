# CLAUDE.md

## Project overview

ESP-IDF firmware for the **receiver** side of a two-ESP32 RC car. Receives joystick commands over **ESP-NOW** (from a paired transmitter) or **UDP over WiFi** (from a Steam Deck) and drives two DC motors via PWM. On boot, both inputs listen simultaneously — the first valid packet locks the input source until power cycle.

## Build & flash

```sh
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor   # adjust port as needed
idf.py menuconfig                        # for config changes (MAC, WiFi credentials, UDP port, log level)
```

Requires ESP-IDF v5.5+. A dev container config is provided in `.devcontainer/`.

## Project structure

```
main/
├── main.c              # Entry point, FreeRTOS tasks (control_task, led_task)
├── wifi_init.c/h       # WiFi init (STA mode, NVS, netif)
├── espnow_receiver.c/h # ESP-NOW receive callback, MAC filtering
├── udp_receiver.c/h    # UDP socket server for Steam Deck input
├── input_lock.c/h      # First-connection-wins atomic lock
├── joystick_packet.h   # Shared joystick_packet_t definition
├── joystick_mixer.c/h  # Arcade drive mixing algorithm
├── motor_control.c/h   # LEDC PWM motor driver (1 kHz, 8-bit)
└── Kconfig.projbuild   # Project configuration menu
```

## Hardware target

- **MCU**: ESP32 (standard Xtensa dual-core, not S2/S3/C3)
- **Motor pins**: GPIO 16/17 (Motor A), GPIO 18/19 (Motor B) — expects an H-bridge driver
- **Status LED**: GPIO 2

## Code conventions

- **Language**: C (C11), pure ESP-IDF — no external dependencies
- **Function naming**: `module_action()` — e.g. `motor_control_drive()`, `arcade_mix()`
- **Constants**: `UPPER_SNAKE_CASE` — e.g. `PWM_FREQ`, `LED_PIN`
- **Types**: `_t` suffix — e.g. `joystick_packet_t`, `motor_output_t`
- **Log tags**: uppercase module name — e.g. `"MAIN"`, `"ESPNOW"`, `"UDP"`, `"WIFI"`, `"INPUT"`
- **Error handling**: `ESP_ERROR_CHECK()` for critical init, `bool` returns for recoverable errors
- **FreeRTOS patterns**: queues for data transfer, task notifications for signaling
- **No dynamic allocation** — all structures are stack-allocated

## Architecture

```
Power on → wifi_init() (STA mode, connects to home WiFi)
         → espnow_init(queue)       ← STA interface
         → udp_receiver_init(queue)  ← STA interface (Steam Deck on same LAN)

ESP-NOW callback ──┐
                   ├─ input_lock check ─► xQueueOverwrite (size 1, always latest)
UDP recv task ─────┘
    → control_task (pri 5): arcade mix → PWM
    → xTaskNotify → led_task (pri 1): solid=connected, blink=disconnected
```

500 ms receive timeout acts as a safety watchdog — motors stop on signal loss.

## Configuration

All settings are in `idf.py menuconfig` → **RC Car Configuration**:

| Setting | Default | Description |
|---------|---------|-------------|
| Transmitter MAC | `00:00:00:00:00:00` | ESP-NOW transmitter MAC (colon-separated hex) |
| WiFi SSID | *(empty)* | WiFi network SSID (ESP32 and Steam Deck must be on same network) |
| WiFi Password | *(empty)* | WiFi network password |
| UDP Port | `4210` | Port for joystick packets from Steam Deck |
| WiFi Channel | `1` | Must match ESP-NOW transmitter channel |

## UDP packet format

The Steam Deck sends the same 10-byte packed struct as ESP-NOW (little-endian):

| Offset | Type | Field |
|--------|------|-------|
| 0 | int16_t | joy1_x (steering, -2048 to +2047) |
| 2 | int16_t | joy1_y (throttle, -2048 to +2047) |
| 4 | int16_t | joy2_x |
| 6 | int16_t | joy2_y |
| 8 | uint8_t | joy1_button |
| 9 | uint8_t | joy2_button |

Target: `<ESP32_IP>:<UDP_PORT>` (IP assigned by router, logged on boot).

## Debug logging

Packet logs are at `DEBUG` level (silent by default). Enable via `idf.py menuconfig` → Component config → Log output → Default log verbosity → Debug.
