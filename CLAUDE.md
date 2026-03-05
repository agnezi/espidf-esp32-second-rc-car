# CLAUDE.md

## Project overview

ESP-IDF firmware for the **receiver** side of a two-ESP32 RC car. Receives joystick commands over **ESP-NOW** (from a paired transmitter) and drives two DC motors via PWM.

## Build & flash

```sh
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor   # adjust port as needed
idf.py menuconfig                        # for config changes (MAC, log level)
```

Requires ESP-IDF v5.5+. A dev container config is provided in `.devcontainer/`.

## Project structure

```
main/
├── main.c              # Entry point, FreeRTOS tasks (control_task, led_task)
├── espnow_receiver.c/h # WiFi/NVS init, ESP-NOW receive callback, MAC filtering
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
- **Constants**: `UPPER_SNAKE_CASE` — e.g. `PWM_FREQ`, `LED_PIN`, `MOTOR_MAX_SPEED`
- **Types**: `_t` suffix — e.g. `joystick_packet_t`, `motor_output_t`
- **Static variables**: `s_` prefix — e.g. `s_packet_queue`, `s_allowed_mac`
- **Log tags**: uppercase module name — e.g. `"MAIN"`, `"ESPNOW"`, `"MOTOR"`
- **Error handling**: `ESP_ERROR_CHECK()` for critical init that must not fail; `bool` returns for recoverable errors
- **No dynamic allocation** — all structures are stack-allocated, queues are pre-allocated at init

## FreeRTOS patterns

- **Queues** for data transfer: size-1 queue with `xQueueOverwrite()` (always latest packet) and `xQueueReceive()` with timeout
- **Task notifications** for signaling: `xTaskNotify()` from control_task to led_task (connected/disconnected state)
- **Task priorities**: control_task at priority 5 (real-time motor control), led_task at priority 1 (visual feedback)
- **Stack sizes**: 4096 for control_task, 1024 for led_task
- Never use `vTaskDelay()` as a watchdog — use queue/notification timeouts instead

## Architecture

```
app_main()
  ├─ GPIO init (LED)
  ├─ motor_control_init() → LEDC timer + 4 PWM channels
  ├─ xQueueCreate(1, sizeof(joystick_packet_t))
  ├─ espnow_init(queue) → NVS, event loop, WiFi STA, ESP-NOW
  └─ spawn control_task + led_task

ESP-NOW callback (runs in WiFi task context):
  MAC filter → size check → xQueueOverwrite()

control_task loop:
  xQueueReceive(500 ms timeout)
    → packet: arcade_mix() → motor_control_drive(), notify LED (connected)
    → timeout: motor_control_stop(), notify LED (disconnected)

led_task loop:
  xTaskNotifyWait(300 ms timeout)
    → notified: update connected state, set LED
    → timeout + disconnected: toggle LED (blink)
```

## Configuration

All settings are in `idf.py menuconfig` → **RC Car Configuration**:

| Setting | Default | Description |
|---------|---------|-------------|
| Transmitter MAC | `00:00:00:00:00:00` | ESP-NOW transmitter MAC (colon-separated hex) |

## ESP-IDF best practices for this project

- **Initialization order matters**: NVS → event loop → WiFi init → WiFi start → ESP-NOW init. Do not reorder.
- **ESP-NOW callbacks run in the WiFi task** — keep them short, no blocking calls. Copy data and post to a queue.
- **LEDC PWM**: use `LEDC_LOW_SPEED_MODE` on standard ESP32. High-speed mode is for specific use cases.
- **Packed structs** (`__attribute__((packed))`) ensure binary compatibility with the transmitter. Always validate packet size before `memcpy`.
- **Kconfig over header files** for secrets/configuration — values flow through `sdkconfig` (gitignored) and are available as `CONFIG_*` macros.
- **WiFi must be in STA mode** for ESP-NOW to work, but no actual connection to an AP is needed.
- When adding new source files, register them in `main/CMakeLists.txt` under `idf_component_register(SRCS ...)`.

## Debug logging

Packet logs are at `DEBUG` level (silent by default). Enable via `idf.py menuconfig` → Component config → Log output → Default log verbosity → Debug.
