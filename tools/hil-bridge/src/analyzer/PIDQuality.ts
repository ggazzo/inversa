// PIDQuality — track classic step-response metrics during a setpoint hold.
//
// Metrics emitted at shutdown:
//   - peakOvershootC: max(currentTemp - targetTemp) once setpoint stabilized
//   - settlingTimeMs: virtual-ms from setpoint set to first sustained entry
//     into a ±SETTLE_BAND of target (must stay inside for SETTLE_HOLD_MS).
//   - iae:            ∫ |error| dt over the hold window.
//
// Scope is intentionally narrow — this analyzer does not assert pass/fail
// on its own (so it can coexist with ad-hoc tuning runs). Scenarios that
// need a gate should construct it with `expect` thresholds.

import type {
  HilPlugin,
  HilPluginContext,
  Report,
  ReportFailure,
  TelemetryMsg,
} from "../core/types.js";

export type PIDQualityOptions = {
  settleBandC?: number;
  settleHoldMs?: number;
  expect?: {
    maxOvershootC?: number;
    maxSettlingMs?: number;
    maxIae?: number;
  };
};

export class PIDQuality implements HilPlugin {
  readonly name = "PIDQuality";
  readonly kind = "analyzer" as const;

  private settleBandC: number;
  private settleHoldMs: number;
  private expect: NonNullable<PIDQualityOptions["expect"]>;

  // Step tracking
  private setpoint: number | null = null;
  private setpointAtT: number | null = null;
  private lastT: number | null = null;
  private peakOvershoot = 0;
  private iae = 0;
  private inBandSinceT: number | null = null;
  private settledAtT: number | null = null;

  private failures: ReportFailure[] = [];

  constructor(opts: PIDQualityOptions = {}) {
    this.settleBandC = opts.settleBandC ?? 0.5;
    this.settleHoldMs = opts.settleHoldMs ?? 30_000;
    this.expect = opts.expect ?? {};
  }

  setup(_ctx: HilPluginContext): void {
    /* no-op */
  }

  onTelemetry(msg: TelemetryMsg, _ctx: HilPluginContext): void {
    if (msg.event !== "state") return;
    // Detect setpoint change → restart metric collection.
    if (msg.targetTemp !== this.setpoint && msg.targetTemp > 0) {
      this.setpoint = msg.targetTemp;
      this.setpointAtT = msg.t;
      this.peakOvershoot = 0;
      this.iae = 0;
      this.inBandSinceT = null;
      this.settledAtT = null;
      this.lastT = msg.t;
      return;
    }
    if (this.setpoint === null) return;
    if (this.lastT === null) this.lastT = msg.t;

    const err = msg.currentTemp - this.setpoint;
    const absErr = Math.abs(err);
    const dtMs = msg.t - this.lastT;
    this.lastT = msg.t;

    if (err > this.peakOvershoot) this.peakOvershoot = err;

    if (dtMs > 0) this.iae += absErr * (dtMs / 1000);

    if (absErr <= this.settleBandC) {
      if (this.inBandSinceT === null) this.inBandSinceT = msg.t;
      if (
        this.settledAtT === null &&
        msg.t - this.inBandSinceT >= this.settleHoldMs
      ) {
        this.settledAtT = this.inBandSinceT;
      }
    } else {
      this.inBandSinceT = null;
    }
  }

  onShutdown(_ctx: HilPluginContext): Report {
    const settlingTimeMs =
      this.settledAtT !== null && this.setpointAtT !== null
        ? this.settledAtT - this.setpointAtT
        : -1;

    if (this.expect.maxOvershootC !== undefined &&
        this.peakOvershoot > this.expect.maxOvershootC) {
      this.failures.push({
        t: this.lastT ?? 0,
        rule: "overshoot",
        detail: `peak overshoot ${this.peakOvershoot.toFixed(2)}°C > ${this.expect.maxOvershootC}°C`,
      });
    }
    if (this.expect.maxSettlingMs !== undefined && settlingTimeMs > this.expect.maxSettlingMs) {
      this.failures.push({
        t: this.lastT ?? 0,
        rule: "settling_time",
        detail: `settling ${settlingTimeMs}ms > ${this.expect.maxSettlingMs}ms`,
      });
    }
    if (this.expect.maxIae !== undefined && this.iae > this.expect.maxIae) {
      this.failures.push({
        t: this.lastT ?? 0,
        rule: "iae",
        detail: `IAE ${this.iae.toFixed(2)} > ${this.expect.maxIae}`,
      });
    }

    return {
      plugin: this.name,
      ok: this.failures.length === 0,
      failures: this.failures,
      metrics: {
        peakOvershootC: this.peakOvershoot,
        settlingTimeMs,
        iae: this.iae,
      },
      notes: [],
    };
  }
}
