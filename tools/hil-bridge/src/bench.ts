#!/usr/bin/env node
// HIL bench — workbench test suite.
//
// Usage:
//   npm run bench -- --port /dev/cu.usbmodem<XXX>
//
// Exercises the HIL contract end-to-end against a real ESP running the
// `hil_s3_mini` or `hil_c3_mini` firmware. Each test is independent
// and self-cleaning. Exit code 0 if all pass, 1 if any fail.

import { SerialPort } from "serialport";
import { argv, exit } from "node:process";
import type { CommandMsg } from "./core/types.js";

interface Frame {
  event?: string;
  type?: string;
  t?: number;
  strValue?: string;
  floatValue?: number;
  intValue?: number;
  boolValue?: boolean;
  [k: string]: unknown;
}

interface BenchCtx {
  send(cmd: CommandMsg): void;
  drain(): Frame[];
  waitFor(predicate: (f: Frame) => boolean, timeoutMs?: number): Promise<Frame>;
  waitForState(predicate: (s: Frame) => boolean, timeoutMs?: number): Promise<Frame>;
  sleep(ms: number): Promise<void>;
  // Most recent state frame received.
  lastState(): Frame | null;
}

type TestFn = (ctx: BenchCtx) => Promise<void>;
interface Test { group: string; name: string; fn: TestFn }

class AssertionError extends Error {}
function assert(cond: unknown, msg: string): asserts cond {
  if (!cond) throw new AssertionError(msg);
}
function assertEq<T>(actual: T, expected: T, msg = ""): void {
  if (actual !== expected) throw new AssertionError(`${msg}: expected ${String(expected)}, got ${String(actual)}`);
}
function assertClose(actual: number, expected: number, tol: number, msg = ""): void {
  if (Math.abs(actual - expected) > tol) {
    throw new AssertionError(`${msg}: expected ${expected}±${tol}, got ${actual}`);
  }
}

// ── Test list (kept inline so the file is self-contained) ───────────
const TESTS: Test[] = [
  // ── connection ────────────────────────────────────────────────
  {
    group: "connection",
    name: "get state returns valid frame",
    fn: async (ctx) => {
      ctx.send({ cmd: "get", path: "state" });
      const f = await ctx.waitFor((m) => m.event === "state", 1500);
      assert(typeof f.t === "number" && f.t > 0, "state.t must be positive");
      assert("currentTemp" in f, "state must have currentTemp");
      assert("watchdogTripped" in f, "state must have watchdogTripped");
      assert("ssr" in f, "state must have ssr");
    },
  },

  // ── NTC override ──────────────────────────────────────────────
  {
    group: "sensors",
    name: "set ntc.c is reflected in currentTemp",
    fn: async (ctx) => {
      ctx.send({ cmd: "set", path: "ntc.bypassKalman", value: true });
      ctx.send({ cmd: "set", path: "ntc.c", value: 42 });
      await ctx.sleep(1500);
      ctx.send({ cmd: "get", path: "state" });
      const s = await ctx.waitForState((s) => Math.abs((s.currentTemp as number) - 42) < 0.1);
      assertClose(s.currentTemp as number, 42, 0.1, "currentTemp");
    },
  },
  {
    group: "sensors",
    name: "ambient override propagates to gState",
    fn: async (ctx) => {
      ctx.send({ cmd: "set", path: "ambient.c", value: 18.5 });
      ctx.send({ cmd: "set", path: "ambient.ok", value: true });
      await ctx.sleep(1200);
      ctx.send({ cmd: "get", path: "state" });
      const s = await ctx.waitForState((s) => Math.abs((s.ambientSensorC as number) - 18.5) < 0.1);
      assertClose(s.ambientSensorC as number, 18.5, 0.1, "ambientSensorC");
      assertEq(s.ambientSensorOk, true, "ambientSensorOk");
    },
  },

  // ── virtual clock ─────────────────────────────────────────────
  {
    group: "clock",
    name: "clock advance bumps state.t",
    fn: async (ctx) => {
      ctx.send({ cmd: "get", path: "state" });
      const before = await ctx.waitFor((m) => m.event === "state");
      ctx.send({ cmd: "clock", op: "advance", ms: 10000 });
      await ctx.sleep(200);
      ctx.send({ cmd: "get", path: "state" });
      const after = await ctx.waitForState((s) => (s.t as number) > (before.t as number) + 9500);
      assert((after.t as number) - (before.t as number) >= 9500, "advance >= 9500 ms");
    },
  },
  {
    group: "clock",
    name: "clock freeze stops t from advancing",
    fn: async (ctx) => {
      ctx.send({ cmd: "clock", op: "freeze" });
      await ctx.sleep(300);
      ctx.send({ cmd: "get", path: "state" });
      const t1 = (await ctx.waitFor((m) => m.event === "state")).t as number;
      await ctx.sleep(500);
      ctx.send({ cmd: "get", path: "state" });
      const t2 = (await ctx.waitFor((m) => m.event === "state")).t as number;
      assert(Math.abs(t2 - t1) <= 10, `frozen clock: t1=${t1} t2=${t2}`);
      ctx.send({ cmd: "clock", op: "unfreeze" });
    },
  },
  {
    group: "clock",
    name: "RTC reflects virtual epoch",
    fn: async (ctx) => {
      const epoch = 1700000000;
      ctx.send({ cmd: "clock", op: "epoch", value: epoch });
      // RTCPlugin propagates now() into gState.rtcTimestamp at 1 Hz, so
      // wait for the predicate to actually hit the virtual band rather
      // than a stale wall-clock value.
      ctx.send({ cmd: "get", path: "state" });
      const s = await ctx.waitForState(
        (s) => {
          const t = s.rtcTimestamp as number;
          return t >= epoch && t < epoch + 60;
        },
        4000,
      );
      const t = s.rtcTimestamp as number;
      assert(t >= epoch && t < epoch + 60, `rtc=${t} expected ~${epoch}`);
    },
  },

  // ── watchdog ──────────────────────────────────────────────────
  {
    group: "watchdog",
    name: "force OVERTEMP latches wd and drops SSR",
    fn: async (ctx) => {
      ctx.send({ cmd: "set", path: "ntc.c", value: 22 });
      await ctx.sleep(1200);
      for (let i = 0; i < 5; i++) {
        ctx.send({ cmd: "force", path: "watchdog.reset" });
        await ctx.sleep(400);
      }
      ctx.send({ cmd: "force", path: "watchdog.trip", value: "OVERTEMP" });
      await ctx.sleep(300);
      ctx.send({ cmd: "get", path: "state" });
      const s = await ctx.waitForState((s) => s.watchdogTripped === true);
      assertEq(s.watchdogTripped, true, "wd tripped");
      assertEq(s.watchdogLastCause, 1, "cause = OVERTEMP (1)");
      assertEq(s.ssr, 0, "SSR LOW after trip");
    },
  },
  {
    group: "watchdog",
    name: "watchdog.reset clears latch when temp safe",
    fn: async (ctx) => {
      ctx.send({ cmd: "set", path: "ntc.c", value: 22 });
      await ctx.sleep(1200);
      // Trip first
      ctx.send({ cmd: "force", path: "watchdog.trip", value: "MANUAL" });
      await ctx.sleep(400);
      // Then reset
      ctx.send({ cmd: "force", path: "watchdog.reset" });
      await ctx.sleep(400);
      ctx.send({ cmd: "get", path: "state" });
      const s = await ctx.waitForState((s) => s.watchdogTripped === false, 2500);
      assertEq(s.watchdogTripped, false, "wd cleared");
    },
  },
  {
    group: "watchdog",
    name: "detections=false suppresses gradient on NTC step",
    fn: async (ctx) => {
      // Bring temp down first so reset is safe.
      ctx.send({ cmd: "set", path: "ntc.c", value: 22 });
      await ctx.sleep(1500);
      for (let i = 0; i < 5; i++) {
        ctx.send({ cmd: "force", path: "watchdog.reset" });
        await ctx.sleep(400);
      }
      ctx.send({ cmd: "watchdog", op: "detections", enable: false });
      await ctx.sleep(300);
      // Big NTC jump — would trip GRADIENT with detections on.
      ctx.send({ cmd: "set", path: "ntc.c", value: 80 });
      await ctx.sleep(2000);
      ctx.send({ cmd: "get", path: "state" });
      const s = await ctx.waitForState((s) => (s.currentTemp as number) > 70);
      assertEq(s.watchdogTripped, false, "wd should NOT trip with detections off");
      ctx.send({ cmd: "watchdog", op: "detections", enable: true });
      ctx.send({ cmd: "set", path: "ntc.c", value: 22 });
      await ctx.sleep(1500);
    },
  },
  {
    group: "watchdog",
    name: "clock advance does not trip LOOP_STUCK",
    fn: async (ctx) => {
      ctx.send({ cmd: "set", path: "ntc.c", value: 22 });
      await ctx.sleep(1200);
      for (let i = 0; i < 5; i++) {
        ctx.send({ cmd: "force", path: "watchdog.reset" });
        await ctx.sleep(400);
      }
      // Default LOOP_STUCK threshold is 5000 ms; advance well past it.
      ctx.send({ cmd: "clock", op: "advance", ms: 30000 });
      await ctx.sleep(800);
      ctx.send({ cmd: "get", path: "state" });
      const s = await ctx.waitForState((s) => true);
      assertEq(s.watchdogTripped, false, "advance should not trigger LOOP_STUCK");
    },
  },

  // ── error paths ───────────────────────────────────────────────
  {
    group: "errors",
    name: "unknown cmd returns err",
    fn: async (ctx) => {
      ctx.send({ cmd: "bogus" } as unknown as CommandMsg);
      const e = await ctx.waitFor((m) => m.event === "err");
      assertEq(e.msg, "unknown_cmd", "err.msg");
    },
  },
  {
    group: "errors",
    name: "unknown force.path returns err",
    fn: async (ctx) => {
      ctx.send({ cmd: "force", path: "bogus" } as unknown as CommandMsg);
      const e = await ctx.waitFor((m) => m.event === "err");
      assertEq(e.msg, "unknown_force_path", "err.msg");
    },
  },

  // ── recipe ────────────────────────────────────────────────────
  {
    group: "recipe",
    name: "load + start + step transitions + completed",
    fn: async (ctx) => {
      ctx.send({ cmd: "watchdog", op: "detections", enable: false });
      ctx.send({ cmd: "set", path: "ntc.c", value: 22 });
      await ctx.sleep(1200);
      ctx.send({ cmd: "force", path: "watchdog.reset" });
      await ctx.sleep(400);
      ctx.drain();
      const dsl =
        'STEP "A"\nSET_TEMP 30\nWAIT_TEMP 1.0\nSTEP "B"\nHEATER_OFF';
      ctx.send({ cmd: "recipe", op: "load", name: "bench", content: dsl });
      const ack1 = await ctx.waitFor((m) => m.event === "ack");
      assertEq(ack1.ok, true, "load ack.ok");
      ctx.send({ cmd: "recipe", op: "start" });
      await ctx.waitFor(
        (m) =>
          m.event === "bus" &&
          typeof m.strValue === "string" &&
          m.strValue.includes("evt:recipe:state") &&
          m.strValue.includes("started"),
        2000,
      );
      ctx.send({ cmd: "set", path: "ntc.c", value: 30 });
      await ctx.waitFor(
        (m) =>
          m.event === "bus" &&
          typeof m.strValue === "string" &&
          m.strValue.includes("evt:recipe:state") &&
          m.strValue.includes("completed"),
        8000,
      );
      ctx.send({ cmd: "watchdog", op: "detections", enable: true });
    },
  },
  {
    group: "recipe",
    name: "WAIT_CONFIRM blocks until confirm is requested",
    fn: async (ctx) => {
      ctx.send({ cmd: "watchdog", op: "detections", enable: false });
      ctx.send({ cmd: "set", path: "ntc.c", value: 22 });
      await ctx.sleep(1200);
      ctx.send({ cmd: "force", path: "watchdog.reset" });
      await ctx.sleep(400);
      ctx.drain();
      const dsl =
        'STEP "Pre"\nSET_TEMP 30\nWAIT_TEMP 5.0\nWAIT_CONFIRM "go"\nSTEP "Post"\nHEATER_OFF';
      ctx.send({ cmd: "recipe", op: "load", name: "wc", content: dsl });
      await ctx.waitFor((m) => m.event === "ack" && m.ok === true);
      ctx.send({ cmd: "recipe", op: "start" });
      ctx.send({ cmd: "set", path: "ntc.c", value: 30 });
      // Wait for the WAIT notify
      await ctx.waitFor(
        (m) =>
          m.event === "bus" &&
          typeof m.strValue === "string" &&
          m.strValue.includes("evt:recipe:wait"),
        5000,
      );
      // No step transition should occur over the next 3 s.
      const before = Date.now();
      let advanced = false;
      const unsub = ctx;
      // poll for 3 s, fail if a "completed" event appears
      while (Date.now() - before < 3000) {
        for (const f of ctx.drain()) {
          if (
            f.event === "bus" &&
            typeof f.strValue === "string" &&
            f.strValue.includes("evt:recipe:state") &&
            f.strValue.includes("completed")
          ) {
            advanced = true;
            break;
          }
        }
        if (advanced) break;
        await ctx.sleep(150);
      }
      assertEq(advanced, false, "recipe must remain blocked at WAIT_CONFIRM");
      ctx.send({ cmd: "recipe", op: "stop" });
      ctx.send({ cmd: "watchdog", op: "detections", enable: true });
    },
  },
];

// ── Bench driver ─────────────────────────────────────────────────────

interface Args {
  port: string;
  group?: string;
  only?: string;
}
function parseArgs(): Args {
  const a = argv.slice(2);
  const out: Partial<Args> = {};
  for (let i = 0; i < a.length; i++) {
    const k = a[i];
    const v = a[i + 1];
    if (k === "--port" && v) { out.port = v; i++; }
    else if (k === "--group" && v) { out.group = v; i++; }
    else if (k === "--only" && v) { out.only = v; i++; }
    else if (k === "--help" || k === "-h") {
      console.log("Usage: bench --port /dev/cu.usbmodem<XXX> [--group <g>] [--only <substr>]");
      exit(0);
    }
  }
  if (!out.port) {
    console.error("error: --port is required");
    exit(2);
  }
  return out as Args;
}

async function main(): Promise<void> {
  const args = parseArgs();
  const port = new SerialPort({ path: args.port, baudRate: 115200, autoOpen: false });
  let buf = "";
  let synced = false;
  const inbox: Frame[] = [];
  let lastState: Frame | null = null;

  port.on("data", (c: Buffer) => {
    buf += c.toString("utf8");
    let nl: number;
    while ((nl = buf.indexOf("\n")) >= 0) {
      const line = buf.slice(0, nl).trim();
      buf = buf.slice(nl + 1);
      if (!synced) { synced = true; continue; }
      if (line.length === 0 || line.startsWith("#")) continue;
      try {
        const m = JSON.parse(line) as Frame;
        if (m.event === "state") lastState = m;
        inbox.push(m);
      } catch { /* ignore */ }
    }
  });

  await new Promise<void>((resolve, reject) => {
    port.open((e: Error | null) => (e ? reject(e) : resolve()));
  });

  const send = (cmd: CommandMsg): void => {
    port.write(JSON.stringify(cmd) + "\n");
  };
  const drain = (): Frame[] => { const r = inbox.slice(); inbox.length = 0; return r; };
  const sleep = (ms: number) => new Promise<void>((r) => setTimeout(r, ms));
  const waitFor = async (pred: (f: Frame) => boolean, timeoutMs = 1500): Promise<Frame> => {
    const start = Date.now();
    while (Date.now() - start < timeoutMs) {
      for (const f of inbox) if (pred(f)) {
        // Remove this frame and earlier ones up to it.
        const idx = inbox.indexOf(f);
        inbox.splice(0, idx + 1);
        return f;
      }
      inbox.length = 0;
      await sleep(50);
    }
    throw new AssertionError(`timeout after ${timeoutMs}ms waiting for frame`);
  };
  const waitForState = (pred: (s: Frame) => boolean, timeoutMs = 2500) =>
    waitFor((m) => m.event === "state" && pred(m), timeoutMs);

  const ctx: BenchCtx = {
    send, drain, waitFor, waitForState, sleep,
    lastState: () => lastState,
  };

  // ── Suite-level setup ──────────────────────────────────────────
  console.log(`[bench] connecting to ${args.port}`);
  await sleep(600);
  send({ cmd: "sub", topic: "events", enable: true });
  send({ cmd: "sub", topic: "state", hz: 2 });
  send({ cmd: "set", path: "ntc.bypassKalman", value: true });
  send({ cmd: "set", path: "ntc.c", value: 22 });
  send({ cmd: "set", path: "ambient.c", value: 22 });
  send({ cmd: "set", path: "ambient.ok", value: true });
  await sleep(1500);
  // Drain hello + initial telemetry.
  drain();

  const filtered = TESTS.filter((t) => {
    if (args.group && t.group !== args.group) return false;
    if (args.only && !t.name.includes(args.only)) return false;
    return true;
  });

  console.log(`[bench] running ${filtered.length} test${filtered.length === 1 ? "" : "s"}\n`);

  const results: { test: Test; ok: boolean; ms: number; err?: string }[] = [];
  let currentGroup = "";
  for (const t of filtered) {
    if (t.group !== currentGroup) {
      console.log(`── ${t.group} ──`);
      currentGroup = t.group;
    }
    const start = Date.now();
    process.stdout.write(`  ${t.name.padEnd(54)} `);
    try {
      await t.fn(ctx);
      const ms = Date.now() - start;
      console.log(`✓ PASS  (${ms}ms)`);
      results.push({ test: t, ok: true, ms });
    } catch (e) {
      const ms = Date.now() - start;
      const msg = e instanceof Error ? e.message : String(e);
      console.log(`✗ FAIL  (${ms}ms) — ${msg}`);
      results.push({ test: t, ok: false, ms, err: msg });
    }
    // Per-test best-effort cleanup so a flaky earlier test doesn't poison
    // the next one. Idempotent ops.
    send({ cmd: "recipe", op: "stop" });
    send({ cmd: "watchdog", op: "detections", enable: true });
    send({ cmd: "set", path: "ntc.c", value: 22 });
    send({ cmd: "clock", op: "unfreeze" });
    await sleep(400);
    send({ cmd: "force", path: "watchdog.reset" });
    await sleep(400);
    drain();
  }

  // Suite-level cleanup.
  send({ cmd: "sub", topic: "state", hz: 0 });
  send({ cmd: "sub", topic: "events", enable: false });
  await sleep(400);
  await new Promise<void>((r) => port.close(() => r()));

  // Summary.
  const pass = results.filter((r) => r.ok).length;
  const fail = results.length - pass;
  console.log("\n" + "═".repeat(60));
  console.log(`  ${pass} pass · ${fail} fail · ${results.length} total`);
  if (fail > 0) {
    console.log("\n  Failures:");
    for (const r of results) {
      if (!r.ok) console.log(`    ✗ [${r.test.group}] ${r.test.name}: ${r.err}`);
    }
  }
  console.log("═".repeat(60));
  exit(fail > 0 ? 1 : 0);
}

main().catch((e) => {
  console.error(`[bench] fatal: ${e instanceof Error ? e.message : String(e)}`);
  exit(2);
});
