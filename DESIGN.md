# AetherDeej Arduino Sketch Design Document

## Scope

This document describes the behavior and design of the sketch in `AetherDeej.ino`.

## High-Level Purpose

The sketch samples four analog sliders, applies lightweight filtering and edge stabilization, and streams normalized percentages over serial for a host app (for example, deej-style audio control).

## Hardware and IO Assumptions

- Analog inputs: `A0`, `A1`, `A2`, `A3`
- Number of sliders: `4`
- Serial baud rate: `115200`

## Key Constants

- `NUM_SLIDERS = 4`
- `SAMPLES_PER_SLIDER = 3`
- `SAMPLE_INTERVAL_MS = 1`
- `STARTUP_STABILIZE_MS = 100`
- `SLIDER_NOISE_THRESHOLD = 2`
- `MIN_SEND_INTERVAL = 50`
- `PERIODIC_SEND_INTERVAL = 2000`
- `SEND_REPEAT_COUNT = 2`
- `SEND_REPEAT_INTERVAL_MS = 2`
- `SNAP_TO_MAX_PERCENT = 99`
- `SNAP_TO_MIN_PERCENT = 1`

## Data Model

- `analogSliderValues[]`: latest averaged raw ADC values (`0..1023`)
- `prevSliderValues[]`: last sent raw ADC values used for change detection
- `sliderSampleSums[]` and `sliderSampleCounts[]`: rolling accumulation for non-blocking averaging
- `sliderPrimed[]`: tracks whether each channel has produced at least one averaged value
- `pendingPayload[]`, `pendingSendCount`, `lastRepeatSendTime`: non-blocking repeated send state

## Architecture

### Non-blocking sampling

`updateSliderValues()` is a millis-driven sampler:

- Runs at most once per `SAMPLE_INTERVAL_MS`
- Samples one slider channel per run
- Accumulates `SAMPLES_PER_SLIDER` readings
- Commits averaged raw value, resets accumulator, advances to next slider

No runtime `delay()` is used in the sampling path.

### Non-blocking startup warm-up

Startup is state-based and non-blocking:

1. `setup()` configures pins, starts serial, stores `startupStartTime`.
2. `loop()` returns until `STARTUP_STABILIZE_MS` has elapsed.
3. `loop()` continues returning until all sliders are primed (`allSlidersPrimed()`).
4. `prevSliderValues[]` is initialized from current averages.
5. Runtime send timers are initialized and normal operation begins.

### Non-blocking send queue

When a send event is triggered, the sketch does not block to repeat writes. Instead:

1. `queueSliderValuesForSend(now)` builds one payload into `pendingPayload`.
2. `pendingSendCount` is set to `SEND_REPEAT_COUNT`.
3. `processPendingSend(now)` emits one line each `SEND_REPEAT_INTERVAL_MS`.
4. While repeats are pending, new payload generation is deferred.

## Change Detection and Emission Policy

`sliderValuesChanged()` compares current raw averages against `prevSliderValues[]`:

- Change is significant if `abs(delta) > SLIDER_NOISE_THRESHOLD`.
- A send is queued when either:
  - significant change and at least `MIN_SEND_INTERVAL` ms passed since last send, or
  - `PERIODIC_SEND_INTERVAL` ms elapsed (keepalive)

After queueing, current raw values are copied into `prevSliderValues[]`.

## Value Scaling and Edge Stabilization

`scaleSliderToPercent(raw)` performs output normalization:

1. Rounded integer conversion from `0..1023` to `0..100`
2. Endpoint snap logic:
   - `>= 99` becomes `100`
   - `<= 1` becomes `0`

This reduces visual and protocol jitter near slider extremes.

## Serial Protocol

### Payload format

- ASCII line per message
- Pipe-delimited percentages
- 4 channels in fixed order (`A0|A1|A2|A3`)

Example:

`12|47|83|100`

### Repetition behavior

- Each logical update is transmitted `SEND_REPEAT_COUNT` times (currently 2)
- Repeats are spaced by `SEND_REPEAT_INTERVAL_MS` (currently 2 ms)

## Memory Strategy

The sketch avoids dynamic `String` allocation in runtime payload paths:

- Payload assembly uses a fixed-size char buffer plus `snprintf`
- Buffer writes are bounds-checked

This is safer for AVR SRAM over long runtimes.

## Functional Components

- `updateSliderValues()`
  - Non-blocking rolling sampler and averager
- `allSlidersPrimed()`
  - Startup readiness check
- `sliderValuesChanged()`
  - Raw delta threshold check
- `scaleSliderToPercent()`
  - Rounded conversion plus endpoint snapping
- `buildSliderPayload()`
  - Pipe-delimited payload formatting into fixed buffer
- `queueSliderValuesForSend()` and `processPendingSend()`
  - Non-blocking repeated send state machine
- `printSliderValues()`
  - Human-readable debug output

## Tradeoffs

### Pros

- Fully non-blocking loop behavior during normal operation and startup
- Better serial stability for hosts through periodic keepalive and controlled repetition
- Reduced edge jitter at 0% and 100%
- AVR-friendly memory behavior with fixed-size buffers

### Cons

- Repeated transmissions still increase serial traffic
- Fixed `NUM_SLIDERS = 4` requires code edits for different channel counts
- No checksum or sequence number for host-side deduplication/integrity

## Future Improvements

1. Per-slider calibration (`rawMin/rawMax`) and invert options
2. Optional hysteresis by output percent (not just raw ADC threshold)
3. Optional sequence number or checksum in payload
4. Optional compile-time profiles for different boards and slider counts

## Behavioral Summary

The sketch continuously and non-blockingly samples analog sliders, smooths raw inputs, snaps near-endpoint output values, and emits pipe-delimited percentages when meaningful changes occur or every 2 seconds as keepalive.
