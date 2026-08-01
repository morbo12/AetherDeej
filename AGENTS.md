# AetherDeej — Agent Guide

Single-file Arduino sketch (`AetherDeej.ino`) for a 4-slider deej-style serial controller. Part of the AetherSphere ecosystem.

## What's here

| Path | Purpose |
|---|---|
| `AetherDeej.ino` | Sole source file — sketch, config constants, all logic |
| `README.md` | Setup, wiring, protocol docs (accurate, start here) |
| `DESIGN.md` | Architecture and design rationale (accurate, read for deeper context) |
| `.agents/skills/` | Three local skills loaded automatically by OpenCode |

## Build & upload

Arduino IDE or `arduino-cli` — no PlatformIO, no Makefile, no CI.

```bash
# compile
arduino-cli compile --fqbn arduino:avr:nano .

# upload (replace COM3 and fqbn for your board)
arduino-cli upload -p COM3 --fqbn arduino:avr:nano .
```

## Editing the sketch

- **All config constants are at the top of `AetherDeej.ino`** — `NUM_SLIDERS`, `SAMPLE_INTERVAL_MS`, `SEND_REPEAT_COUNT`, etc.
- The file uses Arduino's `.ino` preprocessor (auto-generates forward declarations). Do not add `#include <Arduino.h>` or manual prototypes for `setup()`/`loop()`.
- `#define DEBUG_SERIAL 0` at line 1 toggles a debug-print block inside `loop()`. Set to `1` when troubleshooting.
- **Never use `delay()`** — the design is fully non-blocking (`millis()`-driven sampling, startup warm-up, and send queue).

## Serial protocol

- `115200` baud
- Payload: `v0|v1|v2|v3` (pipe-delimited, `0..100`)
- Each logical update sent 2× (2 ms apart) for host reliability
- Change-triggered (threshold `>2` raw ADC delta) with periodic keepalive every 2000 ms

## Design constraints to preserve

- **No dynamic `String` in runtime payload path** — everything uses fixed `char` buffers + `snprintf`. AVR SRAM is limited.
- **Non-blocking everywhere** — `loop()` iterates fast; sampling, startup, and send repeats are all state-machine-driven.
- **Endpoint snapping** — values ≥99% snap to 100, ≤1% snap to 0. Don't remove or change thresholds without understanding the jitter rationale.

## Local skills (loaded automatically)

- `arduino-code-generator` — patterns for sensors, comms, state machines, filtering
- `arduino-project-builder` — complete multi-component project templates
- `arduino-serial-monitor` — serial data reading, logging, filtering

## What's NOT here

- No unit/integration tests
- No CI pipeline
- No GitHub workflows
- No `.gitignore`
