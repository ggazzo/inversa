// Wire-level types for the HIL JSONL protocol.
//
// Every inbound message carries `t` (virtual ms snapshot at the moment the
// firmware emitted the line) so the host can correlate against the virtual
// clock it advances via clock commands.

import type { VirtualClock } from "./clock.js";
import type { HostBus } from "./bus.js";

// ── Telemetry (firmware → host) ─────────────────────────────
export type StateMsg = {
  event: "state";
  t: number;
  currentTemp: number;
  targetTemp: number;
  tempSensorOk: boolean;
  heaterOn: boolean;
  pidOutput: number;
  watchdogTripped: boolean;
  watchdogLastCause: number;
  watchdogTripCount: number;
  ambientSensorC: number;
  ambientSensorOk: boolean;
  mode: number;
  rtcAvailable: boolean;
  rtcTimestamp: number;
  ssr: 0 | 1;
};

export type BusMsg = {
  event: "bus";
  t: number;
  type: string;
  floatValue: number;
  intValue: number;
  boolValue: boolean;
};

export type SsrMsg = { event: "ssr"; t: number; level: 0 | 1 };
export type AckMsg = { event: "ack"; t: number; cmd: string; ok: boolean };
export type ErrMsg = { event: "err"; t: number; msg: string; raw: string };
export type HelloMsg = { event: "hello"; t: number; build: string };

export type TelemetryMsg =
  | StateMsg
  | BusMsg
  | SsrMsg
  | AckMsg
  | ErrMsg
  | HelloMsg;

// ── Commands (host → firmware) ──────────────────────────────
export type SetCmd = {
  cmd: "set";
  path:
    | "ntc.c"
    | "ntc.bypassKalman"
    | "ntc.passthrough"
    | "ambient.c"
    | "ambient.ok";
  value: number | boolean;
};

export type ClockCmd =
  | { cmd: "clock"; op: "advance"; ms: number }
  | { cmd: "clock"; op: "freeze" }
  | { cmd: "clock"; op: "unfreeze" }
  | { cmd: "clock"; op: "epoch"; value: number };

export type ForceCmd =
  | {
      cmd: "force";
      path: "watchdog.trip";
      value: "OVERTEMP" | "LOOP_STUCK" | "PIN_STUCK" | "SENSOR_FAULT" | "GRADIENT" | "MANUAL";
    }
  | {
      cmd: "force";
      path: "watchdog.reset";
    };

export type GetCmd = { cmd: "get"; path: "state" };

export type SubCmd =
  | { cmd: "sub"; topic: "state"; hz: number }
  | { cmd: "sub"; topic: "events"; enable: boolean }
  | { cmd: "sub"; topic: "ssr"; enable: boolean };

export type RecipeCmd =
  | { cmd: "recipe"; op: "load"; name: string; content: string }
  | { cmd: "recipe"; op: "start" }
  | { cmd: "recipe"; op: "stop" }
  | { cmd: "recipe"; op: "pause" }
  | { cmd: "recipe"; op: "resume" };

export type CommandMsg = SetCmd | ClockCmd | ForceCmd | GetCmd | SubCmd | RecipeCmd;

// ── Plugin contract ─────────────────────────────────────────
//
// Host-side plugins fall into two kinds:
//   - "sim"      — produces sensor inputs. Reads SSR/heater state, writes
//                  ntc.c / ambient.c via the transport. Closed loop.
//   - "analyzer" — observes telemetry, asserts invariants, accumulates a
//                  Report. Pure reader.
//
// Multiple sims can coexist (e.g. a thermal sim + a sensor-fault injector)
// but only the LAST one to write a given input wins. Analyzers are always
// additive. Each plugin's onShutdown() is gathered into the final report.

export type Report = {
  plugin: string;
  ok: boolean;
  failures: ReportFailure[];
  metrics: Record<string, number>;
  notes: string[];
};

export type ReportFailure = {
  // virtual-ms when the failure was observed
  t: number;
  rule: string;
  detail: string;
};

export type HilPluginContext = {
  clock: VirtualClock;
  bus: HostBus;
  send(cmd: CommandMsg): void;
};

export type HilPlugin = {
  name: string;
  kind: "sim" | "analyzer";

  // Called once after the firmware "hello" arrives.
  setup(ctx: HilPluginContext): Promise<void> | void;

  // Called for every inbound telemetry message.
  onTelemetry?(msg: TelemetryMsg, ctx: HilPluginContext): void;

  // Called on a fixed cadence (default 50 Hz wall-clock) to let sims
  // integrate / push inputs. ctx.clock.now() returns the virtual ms.
  onTick?(virtualMs: number, ctx: HilPluginContext): void;

  // Called once at end of run. Plugin returns its Report.
  onShutdown(ctx: HilPluginContext): Promise<Report> | Report;
};
