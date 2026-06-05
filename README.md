# AetherDeej

Arduino sketch for a 4-slider, deej-style serial controller.

The sketch reads analog sliders, applies non-blocking smoothing, and emits pipe-delimited percentage values over serial.

## Features

- Non-blocking sampling (`millis()` driven, no runtime delays)
- Non-blocking startup warm-up and slider priming
- Change-triggered sending with periodic keepalive
- Repeated transmission queue for host reliability
- Fixed-size `char` payload formatting with `snprintf` (no dynamic `String` in runtime payload path)
- Endpoint snap logic to reduce `99/100` and `0/1` jitter

## Hardware

- Arduino board with at least 4 analog inputs (tested pattern: Nano/Uno class AVR)
- 4 sliders/potentiometers wired to:
  - `A0`
  - `A1`
  - `A2`
  - `A3`

Typical potentiometer wiring per channel:

- One outer pin to `5V`
- Other outer pin to `GND`
- Wiper (middle) to analog input (`A0..A3`)

## Serial Protocol

- Baud: `115200`
- Line format: `v0|v1|v2|v3`
- Value range: `0..100`
- Example:

```text
12|47|83|100
```

Each logical update is sent `SEND_REPEAT_COUNT` times (default `2`), spaced by `SEND_REPEAT_INTERVAL_MS` (default `2` ms), using a non-blocking send queue.

## Runtime Behavior

- Sampling:
  - One sample taken every `SAMPLE_INTERVAL_MS` (default `1` ms)
  - `SAMPLES_PER_SLIDER` samples averaged per channel (default `3`)
- Startup:
  - Waits `STARTUP_STABILIZE_MS` (default `100` ms)
  - Waits until all sliders are primed with averaged data
- Send trigger:
  - Change detected if raw delta `> SLIDER_NOISE_THRESHOLD` (default `2`)
  - Minimum send spacing: `MIN_SEND_INTERVAL` (default `50` ms)
  - Keepalive interval: `PERIODIC_SEND_INTERVAL` (default `2000` ms)

## Jitter Handling

`scaleSliderToPercent()` uses rounded conversion and endpoint snapping:

- `>= 99` percent snaps to `100`
- `<= 1` percent snaps to `0`

This helps prevent occasional edge fluctuations when sliders are physically at min/max.

## Configuration

Main tuning constants are in [AetherDeej.ino](AetherDeej.ino):

- `SLIDER_NOISE_THRESHOLD`
- `SAMPLES_PER_SLIDER`
- `SAMPLE_INTERVAL_MS`
- `MIN_SEND_INTERVAL`
- `PERIODIC_SEND_INTERVAL`
- `SEND_REPEAT_COUNT`
- `SEND_REPEAT_INTERVAL_MS`
- `SNAP_TO_MAX_PERCENT`
- `SNAP_TO_MIN_PERCENT`

## Build and Upload

### Arduino IDE

1. Open [AetherDeej.ino](AetherDeej.ino).
2. Select board and COM port.
3. Upload.
4. Open Serial Monitor at `115200` baud.

### arduino-cli (example)

```bash
arduino-cli board list
arduino-cli compile --fqbn arduino:avr:nano .
arduino-cli upload -p COM3 --fqbn arduino:avr:nano .
```

Replace `COM3` and `fqbn` as needed for your board.

## Project Docs

- Design details: [DESIGN.md](DESIGN.md)
- Sketch: [AetherDeej.ino](AetherDeej.ino)
