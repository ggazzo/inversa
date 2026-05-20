// overtemp scenario — boot ESP, push NTC above the watchdog hard-stop,
// assert latch within 100 ms virtual.
//
// Steps:
//   1. SafetyInvariants analyzer is registered.
//   2. Skip the closed-loop ThermalSim — the scenario drives NTC directly
//      so the cause is unambiguous (a sim+heat loop would also work but
//      is slower and adds noise).
//   3. Sub state @ 50 Hz so the latch transition is captured promptly.
//   4. At t≈+200ms, push NTC above hard-stop with bypassKalman=true so the
//      step lands in gState in one TemperaturePlugin tick.
//   5. Wait the deadline + margin in virtual time, then collect the
//      analyzer report.

import type { HilPlugin, HilPluginContext, Report } from "../core/types.js";
import { SafetyInvariants } from "../analyzer/SafetyInvariants.js";

export type OvertempOptions = {
  // Temperature to inject (must exceed hard-stop). Defaults to 110°C which
  // is safely above the 105°C default.
  injectC?: number;
  // Virtual-ms allowed after the injection before we assert.
  deadlineMs?: number;
};

class OvertempDriver implements HilPlugin {
  readonly name = "OvertempDriver";
  readonly kind = "sim" as const;
  private injected = false;
  private injectedAtVMs = 0;

  constructor(private opts: OvertempOptions) {}

  setup(ctx: HilPluginContext): void {
    // Force a clean baseline first: a fresh NTC reading at room temp with
    // Kalman bypass, so the firmware doesn't latch on stale ADC noise.
    ctx.send({ cmd: "set", path: "ntc.bypassKalman", value: true });
    ctx.send({ cmd: "set", path: "ntc.c", value: 22 });
    ctx.send({ cmd: "set", path: "ambient.c", value: 22 });
  }

  onTick(virtualMs: number, ctx: HilPluginContext): void {
    if (this.injected) return;
    if (virtualMs < 500) return; // let boot settle
    const injectC = this.opts.injectC ?? 110;
    ctx.send({ cmd: "set", path: "ntc.c", value: injectC });
    this.injected = true;
    this.injectedAtVMs = virtualMs;
    process.stderr.write(
      `[overtemp] injected ${injectC}°C at vt=${virtualMs}ms\n`,
    );
  }

  onShutdown(_ctx: HilPluginContext): Report {
    return {
      plugin: this.name,
      ok: this.injected,
      failures: this.injected
        ? []
        : [
            {
              t: 0,
              rule: "no_injection",
              detail: "scenario ended before injecting overtemp",
            },
          ],
      metrics: { injectedAtVMs: this.injectedAtVMs },
      notes: [],
    };
  }
}

export function overtempScenario(opts: OvertempOptions = {}): HilPlugin[] {
  return [
    new OvertempDriver(opts),
    new SafetyInvariants({ tripDeadlineMs: opts.deadlineMs ?? 100 }),
  ];
}
