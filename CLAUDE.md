# CLAUDE.md

## Project overview

ESP-IDF firmware for the **receiver** side of a two-ESP32 RC car. Receives joystick commands over **ESP-NOW** (from a paired transmitter) and drives two DC motors via PWM.

## Build & flash

```sh
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor   # adjust port as needed
idf.py menuconfig                        # for log level changes
```

Requires ESP-IDF v5.5+. A dev container config is provided in `.devcontainer/`.

## Project structure

```
main/
├── main.c              # Entry point, FreeRTOS tasks (control_task, led_task)
├── espnow_receiver.c/h # WiFi/NVS init, ESP-NOW callback, DISCOVER/HELLO handshake
├── joystick_mixer.c/h  # Arcade drive mixing algorithm
├── motor_control.c/h   # LEDC PWM motor driver (1 kHz, 8-bit)
└── Kconfig.projbuild   # Project configuration menu (empty — no config needed)
```

## Hardware target

- **MCU**: ESP32 (standard Xtensa dual-core, not S2/S3/C3)
- **Motor pins**: GPIO 16/17 (Motor A), GPIO 18/19 (Motor B) — expects an H-bridge driver
- **Status LED**: GPIO 2

## Code conventions

- **Language**: C (C11), pure ESP-IDF — no external dependencies
- **Function naming**: `module_action()` — e.g. `motor_control_drive()`, `arcade_mix()`
- **Constants**: `UPPER_SNAKE_CASE` — e.g. `PWM_FREQ`, `LED_PIN`, `MOTOR_MAX_SPEED`
- **Types**: `_t` suffix — e.g. `joystick_packet_t`, `motor_output_t`
- **Static variables**: `s_` prefix — e.g. `s_packet_queue`, `s_paired`
- **Log tags**: uppercase module name — e.g. `"MAIN"`, `"ESPNOW"`, `"MOTOR"`
- **Error handling**: `ESP_ERROR_CHECK()` for critical init that must not fail; `bool` returns for recoverable errors
- **No dynamic allocation** — all structures are stack-allocated, queues are pre-allocated at init

## FreeRTOS patterns

- **Queues** for data transfer: size-1 queue with `xQueueOverwrite()` (always latest packet) and `xQueueReceive()` with timeout
- **Task notifications** for signaling: `xTaskNotify()` from control_task to led_task (connected/disconnected state)
- **Task priorities**: control_task at priority 5 (real-time motor control), led_task at priority 1 (visual feedback)
- **Stack sizes**: 4096 for control_task, 1024 for led_task
- Never use `vTaskDelay()` as a watchdog — use queue/notification timeouts instead

## Joystick input range

The transmitter sends axis values in **-100..100** (after ADC scaling and deadzone). `JOYSTICK_INPUT_MAX 100` in `joystick_mixer.h` matches this range. Do not change it to 2048 (12-bit raw ADC) — the transmitter already normalizes values before sending.

## Architecture

```
app_main()
  ├─ GPIO init (LED)
  ├─ motor_control_init() → LEDC timer + 4 PWM channels
  ├─ xQueueCreate(1, sizeof(joystick_packet_t))
  ├─ espnow_init(queue) → NVS, event loop, WiFi STA ch1, ESP-NOW, broadcast peer
  └─ spawn control_task + led_task

ESP-NOW callback (runs in WiFi task context):
  1-byte packet:
    MSG_DISCOVER (0x01) → add_peer(src) + send MSG_HELLO (0x02) + s_paired = true
  10-byte packet + s_paired:
    size check → xQueueOverwrite()

control_task loop:
  xQueueReceive(500 ms timeout)
    → packet: arcade_mix() → motor_control_drive(), notify LED (connected)
    → timeout: motor_control_stop(), notify LED (disconnected)
      (pairing is NOT reset — transmitter resumes automatically)

led_task loop:
  xTaskNotifyWait(300 ms timeout)
    → notified: update connected state, set LED
    → timeout + disconnected: toggle LED (blink)
```

## Pairing protocol

No pre-configuration needed. Uses a DISCOVER/HELLO handshake compatible with the companion transmitter firmware (`espidf-esp32-joystick-universal`):

1. Both devices power on — receiver blinks LED, transmitter starts broadcasting `MSG_DISCOVER` (1 byte = `0x01`) every 500 ms on WiFi channel 1
2. Receiver gets DISCOVER → registers transmitter as peer → sends `MSG_HELLO` (1 byte = `0x02`) → `s_paired = true`
3. Transmitter receives HELLO → enters CONNECTED state → starts sending joystick packets
4. Receiver LED goes solid on first joystick packet
5. If signal is lost (500 ms timeout): motors stop, LED blinks — pairing stays active, resumes automatically when signal returns
6. If transmitter restarts: it sends DISCOVER again; receiver always responds, re-pairing immediately

## ESP-IDF best practices for this project

- **Initialization order matters**: NVS → event loop → WiFi init → WiFi start → `esp_wifi_set_channel()` → ESP-NOW init. Do not reorder.
- **ESP-NOW callbacks run in the WiFi task** — keep them short, no blocking calls. Copy data and post to a queue.
- **LEDC PWM**: use `LEDC_LOW_SPEED_MODE` on standard ESP32. High-speed mode is for specific use cases.
- **Packed structs** (`__attribute__((packed))`) ensure binary compatibility with the transmitter. Always validate packet size before `memcpy`.
- **WiFi must be in STA mode** for ESP-NOW to work, but no actual connection to an AP is needed.
- **Channel must match**: both receiver and transmitter use channel 1 (`#define CHANNEL 1`). Change both if needed.
- **Broadcast peer must be registered** (`FF:FF:FF:FF:FF:FF`) before calling `esp_now_send()` to a broadcast address — needed to send the HELLO response.
- When adding new source files, register them in `main/CMakeLists.txt` under `idf_component_register(SRCS ...)`.

## Debug logging

Packet receive logs are at `INFO` level (visible by default). To reduce verbosity, raise the log level via `idf.py menuconfig` → Component config → Log output → Default log verbosity.
