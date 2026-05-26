# hil-bridge

Host bridge for the BrewPilot Hardware-in-the-Loop firmware build. Drives
sensor inputs and the virtual clock of a firmware compiled with
`HIL_BUILD=1` over USB CDC (JSONL), and runs analyzer plugins on the
returned telemetry.

## Install

```bash
npm install
```

## Flashing the firmware

```bash
cd ../../firmware
pio run -e hil_s3_mini -t upload
```

Confirm the serial port:

```bash
pio device list
```

## Run a cenario

```bash
npm run hil-bridge -- run scenario overtemp --port /dev/cu.usbmodem<XXX>
```

Exit code is `0` on PASS, `1` on FAIL (analyzer veredito), `2` on bridge
error.

## Run ad-hoc

```bash
npm run hil-bridge -- run \
  --sim thermal \
  --analyzer safety,pid \
  --port /dev/cu.usbmodem<XXX> \
  --duration 30s
```

List available plugins and cenarios:

```bash
npm run hil-bridge -- list
```

## Trace a session for repro

Add `--trace path/to/file.jsonl`. The bridge records every command (host
→ ESP) and every telemetry frame (ESP → host) one per line with a
wall-clock millisecond timestamp.

## Command surface (host → ESP)

| Command | Payload | Effect |
|---------|---------|--------|
| `set` | `{path, value}` | Override `ntc.c`, `ntc.bypassKalman`, `ntc.passthrough`, `ambient.c`, `ambient.ok` |
| `clock` | `{op: advance, ms}` | Add `ms` to virtual clock offset |
| `clock` | `{op: freeze}` | Stop virtual clock at its current value |
| `clock` | `{op: unfreeze}` | Resume virtual clock |
| `clock` | `{op: epoch, value}` | Set RTC virtual epoch (unix seconds) |
| `force` | `{path: "watchdog.trip", value: cause}` | Synchronously trip the watchdog. Causes: `OVERTEMP`, `SENSOR_FAULT`, `LOOP_STUCK`, `GRADIENT`, `MANUAL`, `PIN_STUCK` (alias of LOOP_STUCK) |
| `get` | `{path: "state"}` | Emit one `event:state` |
| `sub` | `{topic: "state", hz}` | Emit `event:state` at `hz` Hz |
| `sub` | `{topic: "events", enable}` | Tee curated EventBus topics as `event:bus` |
| `sub` | `{topic: "ssr", enable}` | Emit `event:ssr` on every SSR pin level change |

## Writing a plugin

`HilPlugin` shape in `src/core/types.ts`:

- `kind: "sim"` plugins drive inputs; they react to telemetry / ticks
  and call `ctx.send({ cmd: ... })`.
- `kind: "analyzer"` plugins read telemetry and produce a `Report` from
  `onShutdown()`. Verdict `FAIL` makes the run exit non-zero.

Add the plugin to `src/core/registry.ts` to make it selectable from the
CLI.

## Writing a cenario

A cenario is a small TypeScript module that picks plugins, sends an
initial command sequence, and waits for events. See
`src/scenarios/overtemp.ts` as the reference.

Wire it into `src/scenarios/index.ts` so `run scenario <name>` finds it.

## Bench suite

End-to-end `node:test` suite that runs the full HIL contract against a
real ESP.

```bash
HIL_PORT=/dev/cu.usbmodem<XXX> npm run bench
```

Filter to a subset via node:test's own `--test-name-pattern`:

```bash
HIL_PORT=/dev/cu.usbmodem<XXX> npm run bench -- --test-name-pattern recipe
HIL_PORT=/dev/cu.usbmodem<XXX> npm run bench -- --test-name-pattern WAIT_CONFIRM
```

If `HIL_PORT` is unset, the suite emits a single `skip` so `npm run
bench` is harmless without a connected ESP.

### Coverage (25 cases)

- **connection** — state frame shape
- **sensors** — NTC override, ntc.bypassKalman lands without Kalman lag,
  ambient override + ok flag
- **clock** — advance, freeze, unfreeze, RTC virtual epoch
- **watchdog** — force OVERTEMP / MANUAL / PIN_STUCK alias, reset, detections
  off suppresses gradient, clock advance does not trip LOOP_STUCK
- **subscriptions** — sub state hz emits + caps + 0 stops, sub events relays
- **errors** — unknown_cmd, unknown_force_path, still_unsafe on reset
- **recipe** — load + start + step + completed, WAIT_TIMER advances via
  virtual clock, WAIT_CONFIRM blocks until confirm, pause + resume,
  BLE notify shape for `evt:recipe:step`

Each case is self-cleaning (`resetState` at entry) so a failure in one
doesn't poison the next.

### Manual UI suite

Opt-in cases that pause for visual verification or actual taps in the
connected PWA. Useful for validating app wiring end to end against real
firmware.

```bash
HIL_MANUAL=1 HIL_PORT=/dev/cu.usbmodem<XXX> npm run bench -- --test-name-pattern "manual UI"
```

Per-prompt timeout (default 60 s):

```bash
HIL_MANUAL=1 HIL_MANUAL_TIMEOUT=120 HIL_PORT=... npm run bench -- --test-name-pattern "manual UI"
```

Cases (6):

- **transitions** — recipe started → step → completed shows in UI
- **confirm dialog** — WAIT_CONFIRM unblocks when you tap confirm
- **pause/resume** — app buttons drive the firmware state
- **hop alerts** — ADD_HOP fires `evt:boil:addition` notifications
- **watchdog warning** — trip is visible, reset clears it
- **step label** — STEP "name" displays in the UI

Each case prints what the operator should see and what to do, then
asserts the firmware-side reaction. Without `HIL_MANUAL=1` the entire
`manual UI` describe is skipped.
