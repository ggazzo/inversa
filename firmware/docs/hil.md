# Hardware-in-the-Loop (HIL) build

Real firmware on real ESP32-S3; sensor inputs and the application-visible
clock are mocked from a host process over USB CDC.

## Build & flash

```bash
pio run -e hil_s3_mini -t upload
```

`HIL_BUILD=1` is injected through `build_src_flags` only — framework
libraries (NimBLE, arduino-esp32) are compiled identically to the
production env. Only translation units in `src/` see the test seams
and the `millis()` macro override.

## Protocol

JSONL on the USB CDC line at 115200 baud. One JSON document per line in
each direction. Lines that begin with `#` (with or without trailing
space) are debug output and must be ignored by the JSON parser; lines
that begin with `{` are protocol frames.

### Host → ESP

| Command | Payload | Effect |
|---------|---------|--------|
| `set` | `{path, value}` | Override `ntc.c`, `ntc.bypassKalman`, `ntc.passthrough`, `ambient.c`, `ambient.ok` |
| `clock` | `{op: "advance", ms}` | Add `ms` to the virtual clock offset |
| `clock` | `{op: "freeze"}` | Stop the virtual clock at its current value |
| `clock` | `{op: "unfreeze"}` | Resume virtual clock |
| `clock` | `{op: "epoch", value}` | Set RTC virtual epoch (unix seconds) |
| `force` | `{path: "watchdog.trip", value: cause}` | Trip watchdog. Causes: `OVERTEMP`, `SENSOR_FAULT`, `LOOP_STUCK`, `GRADIENT`, `MANUAL`, `PIN_STUCK` (alias of LOOP_STUCK) |
| `get` | `{path: "state"}` | Emit one `event:state` |
| `sub` | `{topic: "state", hz}` | Emit `event:state` at `hz` Hz |
| `sub` | `{topic: "events", enable}` | Tee curated EventBus topics as `event:bus` |
| `sub` | `{topic: "ssr", enable}` | Emit `event:ssr` on every SSR pin level change |

### ESP → host

| Event | Fields | Notes |
|-------|--------|-------|
| `hello` | `t`, `firmware`, `schema` | Emitted once at boot |
| `state` | `t`, `currentTemp`, `targetTemp`, `tempSensorOk`, `heaterOn`, `pidOutput`, `watchdogTripped`, `watchdogLastCause`, `watchdogTripCount`, `ambientSensorC`, `ambientSensorOk`, `mode`, `rtcAvailable`, `rtcTimestamp`, `ssr` | Periodic when `sub state` |
| `bus` | `t`, `type`, `floatValue`, `intValue`, `boolValue` | When `sub events` |
| `ssr` | `t`, `level` | On level transition |
| `ack` | `t`, `cmd`, `ok` | Echo of an accepted command |
| `err` | `t`, `msg`, `raw` | Unknown cmd, JSON parse error, validation fail |

All `t` fields use the **virtual** clock, so the analyzer can measure
deadlines against the same base the plugins see via `millis()`.

## What is virtualized, what is not

Virtual: every `millis()` consumed by code under `src/`. The watchdog
ISR also reads through the same macro, so virtual-clock advances can
trigger `LOOP_STUCK` without wall-clock waiting.

NOT virtual: `delay()`, `vTaskDelay()`, `micros()`, the firing cadence
of `esp_timer` itself (the application code inside the ISR uses the
macro, but the hardware tick is unchanged), BLE radio events, WiFi
reconnect timers, NTP.

That means a `clock advance 60000` does not actually sleep through
60 seconds of FreeRTOS work — it just makes the application code think
that 60 seconds have passed.

## BLE

NimBLE stays enabled in HIL builds. The app PWA can connect to the same
firmware in parallel with the harness, useful for cross-checking
protocol handling. Cenarios that cannot tolerate BLE traffic should
ensure no client is paired during the run.

## Where to extend

- New harness command: add a branch in `HilHarnessPlugin::handleLine`.
- New sim or analyzer plugin: see `tools/hil-bridge/README.md`.
- New test seam in another plugin: gate the new branch behind
  `#ifdef HIL_BUILD`, read from the HIL state struct, and write to
  `gState` exactly as the real sensor would.
