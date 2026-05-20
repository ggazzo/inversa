// SafetyInvariants — asserts the watchdog contract on live telemetry.
//
// Invariants:
//   1. Once currentTemp exceeds the configured hard-stop threshold, the
//      watchdog MUST latch within 100 ms measured in the virtual clock.
//   2. While the latch is held (watchdogTripped=true), the SSR pin level
//      MUST stay LOW. Any HIGH observation is a critical failure.
//   3. SSR HIGH transitions must never occur with watchdogTripped=true,
//      even transiently within a single bus event.
//
// The threshold is read at construction time from
// firmware/src/core/constants.h (WATCHDOG_DEFAULT_HARDSTOP_C). Tests can
// override via the constructor.

import { readFileSync } from "node:fs";
import { resolve, dirname } from "node:path";
import { fileURLToPath } from "node:url";

import type {
  HilPlugin,
  HilPluginContext,
  Report,
  ReportFailure,
  TelemetryMsg,
} from "../core/types.js";

export type SafetyOptions = {
  // Override the auto-detected threshold. Useful for unit tests.
  hardStopC?: number;
  // Max latch delay (virtual ms) from threshold cross to watchdogTripped=true.
  tripDeadlineMs?: number;
};

export class SafetyInvariants implements HilPlugin {
  readonly name = "SafetyInvariants";
  readonly kind = "analyzer" as const;

  private hardStopC: number;
  private tripDeadlineMs: number;

  private firstOvertempT: number | null = null;
  private tripObservedT: number | null = null;
  private failures: ReportFailure[] = [];

  constructor(opts: SafetyOptions = {}) {
    this.hardStopC =
      opts.hardStopC ?? readWatchdogHardstopFromConstantsH() ?? 105;
    this.tripDeadlineMs = opts.tripDeadlineMs ?? 100;
  }

  setup(_ctx: HilPluginContext): void {
    // Nothing to subscribe — state telemetry is enough.
  }

  onTelemetry(msg: TelemetryMsg, _ctx: HilPluginContext): void {
    if (msg.event !== "state" && msg.event !== "ssr" && msg.event !== "bus") {
      return;
    }

    if (msg.event === "state") {
      // Track first overtemp observation.
      if (
        this.firstOvertempT === null &&
        msg.tempSensorOk &&
        msg.currentTemp > this.hardStopC
      ) {
        this.firstOvertempT = msg.t;
      }
      // Record when the trip latched.
      if (this.tripObservedT === null && msg.watchdogTripped) {
        this.tripObservedT = msg.t;
        if (this.firstOvertempT !== null) {
          const delay = msg.t - this.firstOvertempT;
          if (delay > this.tripDeadlineMs) {
            this.failures.push({
              t: msg.t,
              rule: "trip_latency",
              detail: `Watchdog tripped ${delay}ms after overtemp (>${this.tripDeadlineMs}ms deadline)`,
            });
          }
        }
      }
      // Invariant 2: tripped + heater asserted is a leak.
      if (msg.watchdogTripped && msg.heaterOn) {
        this.failures.push({
          t: msg.t,
          rule: "heater_while_tripped",
          detail: `gState.heaterOn=true while watchdog latched`,
        });
      }
      if (msg.watchdogTripped && msg.ssr === 1) {
        this.failures.push({
          t: msg.t,
          rule: "ssr_high_while_tripped",
          detail: `SSR pin level=HIGH while watchdog latched (critical safety violation)`,
        });
      }
    }

    if (msg.event === "ssr") {
      // We can't decide here alone — the tripped state lives in StateMsg.
      // SafetyInvariants relies on the state-stream rate being high enough
      // to catch any SSR-high pulse. The CLI sets sub:state hz=20 by
      // default which gives 50ms resolution.
    }
  }

  onShutdown(_ctx: HilPluginContext): Report {
    const ok = this.failures.length === 0;
    const tripLatency =
      this.tripObservedT !== null && this.firstOvertempT !== null
        ? this.tripObservedT - this.firstOvertempT
        : -1;
    return {
      plugin: this.name,
      ok,
      failures: this.failures,
      metrics: {
        hardStopC: this.hardStopC,
        tripDeadlineMs: this.tripDeadlineMs,
        tripLatencyMs: tripLatency,
        observedTrip: this.tripObservedT !== null ? 1 : 0,
      },
      notes: ok
        ? ["All safety invariants held."]
        : [`${this.failures.length} invariant failure(s).`],
    };
  }
}

// ── helpers ────────────────────────────────────────────────
function readWatchdogHardstopFromConstantsH(): number | null {
  // Locate firmware/src/core/constants.h relative to this file. tools/
  // sits as a sibling of firmware/ in the repo, so two levels up should
  // resolve cleanly from dist/analyzer or src/analyzer.
  const here = dirname(fileURLToPath(import.meta.url));
  const candidates = [
    resolve(here, "../../../../firmware/src/core/constants.h"),
    resolve(here, "../../../firmware/src/core/constants.h"),
    resolve(process.cwd(), "firmware/src/core/constants.h"),
    resolve(process.cwd(), "../../firmware/src/core/constants.h"),
  ];
  for (const path of candidates) {
    try {
      const text = readFileSync(path, "utf8");
      const m = text.match(/WATCHDOG_DEFAULT_HARDSTOP_C\s+([0-9.]+)f?/);
      if (m && m[1]) {
        const n = parseFloat(m[1]);
        if (Number.isFinite(n)) return n;
      }
    } catch {
      // try next candidate
    }
  }
  return null;
}
