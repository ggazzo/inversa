// Smoke parity check against `firmware/sim/ThermalSim.h`. Drives the TS
// port with the default kettle and confirms the physics is in the right
// ballpark: heater-always-on warms the pot, heater-off cools it. Tight
// numerical equivalence with the C++ reference is out of scope for the
// unit layer — a separate manual run can do the byte-identical compare.
//
// If this drifts noticeably, either the constants here or the C++ header
// changed and the two need to be reconciled.

import { test } from "node:test";
import { strict as assert } from "node:assert";
import { ThermalSim } from "../src/sim/ThermalSim.js";
import type { CommandMsg, HilPluginContext, TelemetryMsg } from "../src/core/types.js";

// Minimal context stub — ThermalSim only touches `ctx.send`. The clock
// and bus references stay unused for the physics tests but the type
// requires them to be present.
function makeCtx(): { ctx: HilPluginContext; sent: CommandMsg[] } {
  const sent: CommandMsg[] = [];
  const ctx = {
    clock: { now: () => 0 } as unknown,
    bus: { subscribe: () => () => {}, publish: () => {} } as unknown,
    send: (cmd: CommandMsg) => {
      sent.push(cmd);
    },
  } as HilPluginContext;
  return { ctx, sent };
}

function ssrFrame(level: 0 | 1, t: number): TelemetryMsg {
  return { event: "ssr", t, level } as TelemetryMsg;
}

test("ThermalSim warms up when heater is on", () => {
  const sim = new ThermalSim();
  const { ctx } = makeCtx();
  sim.setup(ctx);
  const before = sim.currentTemp();
  sim.onTelemetry?.(ssrFrame(1, 0), ctx);
  // Drive 600 × 100 ms = 60 s of virtual time with heater on.
  for (let i = 1; i <= 600; i++) sim.onTick?.(i * 100, ctx);
  const after = sim.currentTemp();
  // 60 s of 3 kW * 0.9 efficiency into 25 L water at 1 kg/L, c=4186 J/(kg·K)
  // ≈ +1.5 K. We assert a conservative > +1 °C bound to absorb cooling
  // losses without hiding a real physics regression.
  assert.ok(after > before + 1, `expected warming: before=${before.toFixed(2)}, after=${after.toFixed(2)}`);
});

test("ThermalSim cools down when heater is off", () => {
  const sim = new ThermalSim({ initialTempC: 80, ambientTempC: 22 });
  const { ctx } = makeCtx();
  sim.setup(ctx);
  const before = sim.currentTemp();
  sim.onTelemetry?.(ssrFrame(0, 0), ctx);
  for (let i = 1; i <= 600; i++) sim.onTick?.(i * 100, ctx);
  const after = sim.currentTemp();
  assert.ok(after < before, `expected cooling: before=${before.toFixed(2)}, after=${after.toFixed(2)}`);
});

test("ThermalSim sends an initial ntc.c command on setup", () => {
  const sim = new ThermalSim({ initialTempC: 22.5 });
  const { ctx, sent } = makeCtx();
  sim.setup(ctx);
  const initial = sent.find(
    (c): c is Extract<CommandMsg, { cmd: "set"; path: "ntc.c" }> =>
      c.cmd === "set" && c.path === "ntc.c",
  );
  assert.ok(initial, "expected initial ntc.c send");
  assert.equal(initial?.value, 22.5);
});
