# hil-bridge

Host bridge for the Inversa Hardware-in-the-Loop firmware build. Drives
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
