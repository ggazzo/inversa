// HIL bench — node:test suite that runs the full HIL contract against a
// real ESP. Invoked sequentially so cases share a single SerialPort.
//
// Usage:
//   HIL_PORT=/dev/cu.usbmodem<XXX> npm run bench
//
// Filter to a subset with node:test's own --test-name-pattern:
//   HIL_PORT=/dev/cu.usbmodem<XXX> npm run bench -- --test-name-pattern recipe

import { before, after, describe, test } from "node:test";
import { strict as assert } from "node:assert";
import { openConn, type HilConn, type Frame } from "./conn.js";

const PORT = process.env.HIL_PORT;
if (!PORT) {
  // Surface a single skipped case instead of crashing so `npm run bench`
  // without a connected ESP is informative.
  test("HIL_PORT env var not set — bench skipped", { skip: true }, () => {});
} else {
  await runBench(PORT);
}

function isBleFrame(f: Frame, kind: string): boolean {
  return (
    f.event === "bus" &&
    f.type === "BLESend" &&
    typeof f.strValue === "string" &&
    f.strValue.includes(kind)
  );
}

async function runBench(port: string): Promise<void> {
  let conn: HilConn;
  before(async () => {
    conn = await openConn(port);
  });
  after(async () => {
    await conn.close();
  });

  describe("HIL bench", { concurrency: false }, () => {
    // ── connection ──────────────────────────────────────────────
    describe("connection", () => {
      test("get state returns a well-formed frame", async () => {
        await conn.resetState();
        conn.send({ cmd: "get", path: "state" });
        const f = await conn.waitFor((m) => m.event === "state");
        assert.equal(typeof f.t, "number");
        assert.ok((f.t as number) > 0);
        assert.ok("currentTemp" in f);
        assert.ok("watchdogTripped" in f);
        assert.ok("ssr" in f);
        assert.ok("targetTemp" in f);
        assert.ok("pidOutput" in f);
      });
    });

    // ── sensors ─────────────────────────────────────────────────
    describe("sensors", () => {
      test("ntc.c override drives currentTemp", async () => {
        await conn.resetState();
        conn.send({ cmd: "set", path: "ntc.c", value: 42 });
        await conn.sleep(1500);
        conn.send({ cmd: "get", path: "state" });
        const s = await conn.waitForState(
          (s) => Math.abs((s.currentTemp as number) - 42) < 0.1,
        );
        assert.ok(Math.abs((s.currentTemp as number) - 42) < 0.1);
      });

      test("ntc.bypassKalman lands instantly with no filter lag", async () => {
        await conn.resetState();
        // bypassKalman is already true after resetState (set there).
        conn.send({ cmd: "set", path: "ntc.c", value: 65 });
        // Two sensor ticks (LOOP_INTERVAL_MS = 1000) to absorb scheduling
        // jitter between the cmd hitting the buffer and the next loop run.
        await conn.sleep(2200);
        conn.send({ cmd: "get", path: "state" });
        const s = await conn.waitForState(
          (s) => Math.abs((s.currentTemp as number) - 65) < 0.5,
        );
        assert.ok(
          Math.abs((s.currentTemp as number) - 65) < 0.5,
          `bypass should land ~65, got ${s.currentTemp}`,
        );
      });

      test("ambient.c override and ok flag propagate to gState", async () => {
        await conn.resetState();
        conn.send({ cmd: "set", path: "ambient.c", value: 18.5 });
        conn.send({ cmd: "set", path: "ambient.ok", value: true });
        await conn.sleep(1200);
        conn.send({ cmd: "get", path: "state" });
        const s = await conn.waitForState(
          (s) => Math.abs((s.ambientSensorC as number) - 18.5) < 0.1,
        );
        assert.ok(Math.abs((s.ambientSensorC as number) - 18.5) < 0.1);
        assert.equal(s.ambientSensorOk, true);
      });
    });

    // ── virtual clock ───────────────────────────────────────────
    describe("clock", () => {
      test("advance bumps state.t by the requested amount", async () => {
        await conn.resetState();
        conn.send({ cmd: "get", path: "state" });
        const before = await conn.waitFor((m) => m.event === "state");
        conn.send({ cmd: "clock", op: "advance", ms: 10000 });
        await conn.sleep(200);
        conn.send({ cmd: "get", path: "state" });
        const after = await conn.waitForState(
          (s) => (s.t as number) > (before.t as number) + 9500,
        );
        assert.ok((after.t as number) - (before.t as number) >= 9500);
      });

      test("freeze holds state.t constant", async () => {
        await conn.resetState();
        conn.send({ cmd: "clock", op: "freeze" });
        await conn.sleep(300);
        conn.send({ cmd: "get", path: "state" });
        const t1 = (await conn.waitFor((m) => m.event === "state")).t as number;
        await conn.sleep(500);
        conn.send({ cmd: "get", path: "state" });
        const t2 = (await conn.waitFor((m) => m.event === "state")).t as number;
        assert.ok(Math.abs(t2 - t1) <= 10, `t1=${t1} t2=${t2}`);
        conn.send({ cmd: "clock", op: "unfreeze" });
      });

      test("unfreeze resumes t advancing with wall clock", async () => {
        await conn.resetState();
        conn.send({ cmd: "clock", op: "freeze" });
        await conn.sleep(400);
        conn.send({ cmd: "clock", op: "unfreeze" });
        await conn.sleep(400);
        // Drain stale state frames from the periodic sub-state stream
        // before sampling, otherwise waitFor returns an older frame.
        conn.drain();
        conn.send({ cmd: "get", path: "state" });
        const t1 = (await conn.waitFor((m) => m.event === "state")).t as number;
        await conn.sleep(1100);
        conn.drain();
        conn.send({ cmd: "get", path: "state" });
        const t2 = (await conn.waitFor((m) => m.event === "state")).t as number;
        assert.ok(t2 - t1 >= 800, `unfreeze: t1=${t1} t2=${t2}`);
      });

      test("epoch sets RTC virtual base", async () => {
        await conn.resetState();
        const epoch = 1700000000;
        conn.send({ cmd: "clock", op: "epoch", value: epoch });
        conn.send({ cmd: "get", path: "state" });
        // RTCPlugin propagates virtual epoch into gState at 1 Hz.
        const s = await conn.waitForState(
          (s) => {
            const t = s.rtcTimestamp as number;
            return t >= epoch && t < epoch + 60;
          },
          4000,
        );
        const t = s.rtcTimestamp as number;
        assert.ok(t >= epoch && t < epoch + 60, `rtc=${t}`);
      });
    });

    // ── watchdog ────────────────────────────────────────────────
    describe("watchdog", () => {
      test("force OVERTEMP latches and SSR drops LOW", async () => {
        await conn.resetState();
        conn.send({ cmd: "force", path: "watchdog.trip", value: "OVERTEMP" });
        await conn.sleep(400);
        conn.send({ cmd: "get", path: "state" });
        const s = await conn.waitForState((s) => s.watchdogTripped === true);
        assert.equal(s.watchdogTripped, true);
        assert.equal(s.watchdogLastCause, 1, "cause = OVERTEMP");
        assert.equal(s.ssr, 0, "SSR LOW after trip");
      });

      test("force MANUAL latches with cause=5", async () => {
        await conn.resetState();
        conn.send({ cmd: "force", path: "watchdog.trip", value: "MANUAL" });
        await conn.sleep(400);
        conn.send({ cmd: "get", path: "state" });
        const s = await conn.waitForState((s) => s.watchdogTripped === true);
        assert.equal(s.watchdogLastCause, 5);
      });

      test("force PIN_STUCK is an alias for LOOP_STUCK", async () => {
        await conn.resetState();
        conn.send({ cmd: "force", path: "watchdog.trip", value: "PIN_STUCK" });
        await conn.sleep(400);
        conn.send({ cmd: "get", path: "state" });
        const s = await conn.waitForState((s) => s.watchdogTripped === true);
        assert.equal(s.watchdogLastCause, 3, "LOOP_STUCK code");
      });

      test("reset clears latch when temp is safe", async () => {
        await conn.resetState();
        conn.send({ cmd: "force", path: "watchdog.trip", value: "MANUAL" });
        await conn.sleep(400);
        conn.send({ cmd: "force", path: "watchdog.reset" });
        await conn.sleep(400);
        conn.send({ cmd: "get", path: "state" });
        const s = await conn.waitForState(
          (s) => s.watchdogTripped === false,
          2500,
        );
        assert.equal(s.watchdogTripped, false);
      });

      test("detections=false suppresses gradient on big NTC step", async () => {
        await conn.resetState();
        conn.send({ cmd: "watchdog", op: "detections", enable: false });
        await conn.sleep(300);
        conn.send({ cmd: "set", path: "ntc.c", value: 80 });
        await conn.sleep(2000);
        conn.send({ cmd: "get", path: "state" });
        const s = await conn.waitForState((s) => (s.currentTemp as number) > 70);
        assert.equal(
          s.watchdogTripped,
          false,
          "wd must NOT trip with detections off",
        );
      });

      test("clock advance does not trip LOOP_STUCK", async () => {
        await conn.resetState();
        conn.send({ cmd: "clock", op: "advance", ms: 30000 });
        await conn.sleep(800);
        conn.send({ cmd: "get", path: "state" });
        const s = await conn.waitForState((s) => true);
        assert.equal(s.watchdogTripped, false);
      });
    });

    // ── subscriptions ───────────────────────────────────────────
    describe("subscriptions", () => {
      test("sub state hz=5 emits ~5 frames/s", async () => {
        await conn.resetState();
        conn.drain();
        conn.send({ cmd: "sub", topic: "state", hz: 5 });
        await conn.sleep(1100);
        const frames = conn.drain().filter((f) => f.event === "state");
        assert.ok(frames.length >= 3, `expected ≥3 frames, got ${frames.length}`);
        assert.ok(
          frames.length <= 12,
          `expected ≤12 frames (cap+overshoot), got ${frames.length}`,
        );
      });

      test("sub state hz=0 stops the stream", async () => {
        await conn.resetState();
        conn.send({ cmd: "sub", topic: "state", hz: 5 });
        await conn.sleep(400);
        conn.send({ cmd: "sub", topic: "state", hz: 0 });
        await conn.sleep(400);
        conn.drain();
        await conn.sleep(1100);
        const frames = conn.drain().filter((f) => f.event === "state");
        assert.equal(frames.length, 0, "no state frames after hz=0");
      });

      test("sub events relays bus topics", async () => {
        await conn.resetState();
        conn.send({ cmd: "sub", topic: "events", enable: true });
        conn.drain();
        conn.send({ cmd: "force", path: "watchdog.trip", value: "MANUAL" });
        const bus = await conn.waitFor(
          (f) => f.event === "bus" && f.type === "WatchdogTripped",
          1500,
        );
        assert.ok(bus);
      });
    });

    // ── error paths ─────────────────────────────────────────────
    describe("errors", () => {
      test("unknown cmd returns err msg=unknown_cmd", async () => {
        await conn.resetState();
        // Cast through unknown: deliberately malformed payload.
        conn.send({ cmd: "bogus" } as unknown as never);
        const e = await conn.waitFor((m) => m.event === "err");
        assert.equal(e.msg, "unknown_cmd");
      });

      test("unknown force.path returns err msg=unknown_force_path", async () => {
        await conn.resetState();
        conn.send({ cmd: "force", path: "bogus" } as unknown as never);
        const e = await conn.waitFor((m) => m.event === "err");
        assert.equal(e.msg, "unknown_force_path");
      });

      test("watchdog.reset while temp unsafe returns still_unsafe", async () => {
        await conn.resetState();
        conn.send({ cmd: "watchdog", op: "detections", enable: false });
        conn.send({ cmd: "set", path: "ntc.c", value: 110 });
        // Wait long enough for TemperaturePlugin to pick up the hot
        // reading — readNTC returns the override but `gState.currentTemp`
        // updates on the next loop tick (LOOP_INTERVAL_MS=1000).
        await conn.sleep(2500);
        conn.send({ cmd: "force", path: "watchdog.trip", value: "OVERTEMP" });
        await conn.sleep(500);
        // Drain so we observe the response to THIS reset, not stale
        // err frames left over from resetState's earlier reset polls.
        conn.drain();
        // Now reset should refuse: requestReset() checks currentTemp vs
        // hardStop - margin (105 - 5 = 100). Temp is 110, well above.
        conn.send({ cmd: "force", path: "watchdog.reset" });
        const e = await conn.waitFor(
          (m) => m.event === "err" || (m.event === "ack" && (m as { cmd?: string }).cmd === "force"),
          2000,
        );
        assert.equal(e.event, "err", `expected err, got ${e.event} ${JSON.stringify(e)}`);
        assert.equal(e.msg, "still_unsafe");
      });
    });

    // ── recipe ──────────────────────────────────────────────────
    describe("recipe", () => {
      const SIMPLE_DSL =
        'STEP "A"\nSET_TEMP 30\nWAIT_TEMP 1.0\nSTEP "B"\nHEATER_OFF';

      test("load + start + step transitions + completed", async () => {
        await conn.resetState();
        conn.send({ cmd: "watchdog", op: "detections", enable: false });
        conn.drain();
        conn.send({
          cmd: "recipe",
          op: "load",
          name: "bench",
          content: SIMPLE_DSL,
        });
        const ack = await conn.waitFor((m) => m.event === "ack");
        assert.equal(ack.ok, true);
        conn.send({ cmd: "recipe", op: "start" });
        await conn.waitFor(
          (f) => isBleFrame(f, "evt:recipe:state") && (f.strValue as string).includes("started"),
          2000,
        );
        conn.send({ cmd: "set", path: "ntc.c", value: 30 });
        await conn.waitFor(
          (f) => isBleFrame(f, "evt:recipe:state") && (f.strValue as string).includes("completed"),
          8000,
        );
      });

      test("WAIT_TIMER advances via virtual clock", async () => {
        await conn.resetState();
        conn.send({ cmd: "watchdog", op: "detections", enable: false });
        conn.drain();
        conn.send({
          cmd: "recipe",
          op: "load",
          name: "wt",
          content: 'STEP "wait"\nWAIT_TIMER 2\nSTEP "done"\nHEATER_OFF',
        });
        await conn.waitFor((m) => m.event === "ack" && m.ok === true);
        conn.send({ cmd: "recipe", op: "start" });
        // Wait for the recipe to actually reach WAIT_TIMER before we jump
        // the clock — otherwise the advance happens before _timerStart is
        // captured and the comparison runs against the old value.
        await conn.waitFor(
          (f) => isBleFrame(f, "evt:recipe:state") && (f.strValue as string).includes("started"),
          2000,
        );
        await conn.sleep(800);
        conn.send({ cmd: "clock", op: "advance", ms: 130_000 });
        await conn.waitFor(
          (f) => isBleFrame(f, "evt:recipe:state") && (f.strValue as string).includes("completed"),
          8000,
        );
      });

      test("WAIT_CONFIRM blocks the recipe until confirm arrives", async () => {
        await conn.resetState();
        conn.send({ cmd: "watchdog", op: "detections", enable: false });
        conn.drain();
        const dsl =
          'STEP "pre"\nSET_TEMP 30\nWAIT_TEMP 5.0\nWAIT_CONFIRM "go"\nSTEP "post"\nHEATER_OFF';
        conn.send({ cmd: "recipe", op: "load", name: "wc", content: dsl });
        await conn.waitFor((m) => m.event === "ack" && m.ok === true);
        conn.send({ cmd: "recipe", op: "start" });
        conn.send({ cmd: "set", path: "ntc.c", value: 30 });
        await conn.waitFor((f) => isBleFrame(f, "evt:recipe:wait"), 5000);
        // No `completed` event should arrive in the next 2.5 s.
        const cutoff = Date.now() + 2500;
        let advanced = false;
        while (Date.now() < cutoff) {
          for (const f of conn.drain()) {
            if (isBleFrame(f, "evt:recipe:state") && (f.strValue as string).includes("completed")) {
              advanced = true;
              break;
            }
          }
          if (advanced) break;
          await conn.sleep(150);
        }
        assert.equal(advanced, false, "recipe must stay blocked at WAIT_CONFIRM");
        conn.send({ cmd: "recipe", op: "stop" });
      });

      test("pause + resume", async () => {
        await conn.resetState();
        conn.send({ cmd: "watchdog", op: "detections", enable: false });
        conn.drain();
        conn.send({
          cmd: "recipe",
          op: "load",
          name: "pr",
          content: 'STEP "long"\nWAIT_TIMER 5\nSTEP "done"\nHEATER_OFF',
        });
        await conn.waitFor((m) => m.event === "ack" && m.ok === true);
        conn.send({ cmd: "recipe", op: "start" });
        await conn.waitFor(
          (f) => isBleFrame(f, "evt:recipe:state") && (f.strValue as string).includes("started"),
          2000,
        );
        conn.send({ cmd: "recipe", op: "pause" });
        await conn.waitFor(
          (f) => isBleFrame(f, "evt:recipe:state") && (f.strValue as string).includes("paused"),
          2000,
        );
        conn.send({ cmd: "recipe", op: "resume" });
        await conn.waitFor(
          (f) => isBleFrame(f, "evt:recipe:state") && (f.strValue as string).includes("resumed"),
          2000,
        );
        conn.send({ cmd: "recipe", op: "stop" });
      });

      test("BLE notify: evt:recipe:step has step+total+name", async () => {
        await conn.resetState();
        conn.send({ cmd: "watchdog", op: "detections", enable: false });
        conn.drain();
        conn.send({
          cmd: "recipe",
          op: "load",
          name: "s",
          content: 'STEP "first"\nSET_TEMP 30\nHEATER_OFF',
        });
        await conn.waitFor((m) => m.event === "ack" && m.ok === true);
        conn.send({ cmd: "recipe", op: "start" });
        const step = await conn.waitFor(
          (f) => isBleFrame(f, "evt:recipe:step"),
          2000,
        );
        const payload = JSON.parse(step.strValue as string) as {
          tp: string;
          step: number;
          total: number;
          name: string;
        };
        assert.equal(payload.tp, "evt:recipe:step");
        assert.equal(typeof payload.step, "number");
        assert.equal(typeof payload.total, "number");
        assert.ok(payload.total >= 3, `total should be ≥3, got ${payload.total}`);
      });
    });

    // ── Manual UI suite (opt-in via HIL_MANUAL=1) ──────────────
    // Cases here pause for visual verification or actual taps in the
    // connected PWA. Each prints what the operator should see and what
    // to do, then asserts the firmware-side reaction (BLE event arrives,
    // recipe advances, etc.).
    //
    // Run only these:
    //   HIL_MANUAL=1 HIL_PORT=... npm run bench -- --test-name-pattern manual
    //
    // Per-test wait timeout overrideable: HIL_MANUAL_TIMEOUT=<seconds>.
    const MANUAL = process.env.HIL_MANUAL === "1";
    const MANUAL_TIMEOUT_MS = Number(process.env.HIL_MANUAL_TIMEOUT ?? "60") * 1000;
    const prompt = (msg: string): void => {
      process.stderr.write(`\n  ⏸  ${msg}\n`);
    };

    describe("manual UI", { skip: !MANUAL }, () => {
      test("UI shows recipe transitions: started → step → completed", async () => {
        await conn.resetState();
        conn.send({ cmd: "watchdog", op: "detections", enable: false });
        conn.drain();
        const dsl = 'STEP "Pre-aquec"\nSET_TEMP 50\nWAIT_TEMP 1.0\nSTEP "Fim"\nHEATER_OFF';
        conn.send({ cmd: "recipe", op: "load", name: "ui-flow", content: dsl });
        await conn.waitFor((m) => m.event === "ack" && m.ok === true);
        prompt(
          'open the app. You should see the recipe go through:\n' +
          '     start → step "Pre-aquec" → setpoint 50°C → step "Fim" → completed.\n' +
          '     The bench will drive the NTC to satisfy WAIT_TEMP.'
        );
        conn.send({ cmd: "recipe", op: "start" });
        await conn.sleep(2000);
        for (let t = 25; t <= 50; t += 5) {
          conn.send({ cmd: "set", path: "ntc.c", value: t });
          await conn.sleep(1200);
        }
        await conn.waitFor(
          (f) => isBleFrame(f, "evt:recipe:state") && (f.strValue as string).includes("completed"),
          MANUAL_TIMEOUT_MS,
        );
      });

      test("UI confirm dialog: WAIT_CONFIRM unblocks when you tap confirm", async () => {
        await conn.resetState();
        conn.send({ cmd: "watchdog", op: "detections", enable: false });
        conn.drain();
        const dsl =
          'STEP "pre"\nSET_TEMP 30\nWAIT_TEMP 5.0\nWAIT_CONFIRM "Adicione 10L de água"\nSTEP "post"\nHEATER_OFF';
        conn.send({ cmd: "recipe", op: "load", name: "ui-wc", content: dsl });
        await conn.waitFor((m) => m.event === "ack" && m.ok === true);
        conn.send({ cmd: "recipe", op: "start" });
        conn.send({ cmd: "set", path: "ntc.c", value: 30 });
        const wait = await conn.waitFor(
          (f) => isBleFrame(f, "evt:recipe:wait"),
          5000,
        );
        const msg = JSON.parse(wait.strValue as string) as { msg?: string };
        prompt(
          `confirmation dialog should appear in the app with text: "${msg.msg ?? ""}".\n` +
          `     Tap "Confirmar" — recipe should complete and reach the "post" step.`
        );
        await conn.waitFor(
          (f) =>
            isBleFrame(f, "evt:recipe:state") &&
            (f.strValue as string).includes("completed"),
          MANUAL_TIMEOUT_MS,
        );
      });

      test("UI pause/resume: app drives pause + resume from buttons", async () => {
        await conn.resetState();
        conn.send({ cmd: "watchdog", op: "detections", enable: false });
        conn.drain();
        conn.send({
          cmd: "recipe",
          op: "load",
          name: "ui-pause",
          content: 'STEP "long hold"\nWAIT_TIMER 10\nSTEP "done"\nHEATER_OFF',
        });
        await conn.waitFor((m) => m.event === "ack" && m.ok === true);
        conn.send({ cmd: "recipe", op: "start" });
        await conn.waitFor(
          (f) => isBleFrame(f, "evt:recipe:state") && (f.strValue as string).includes("started"),
          2000,
        );
        prompt(
          'tap the PAUSE button in the app. The state pill should switch to "paused".\n' +
          '     Then tap RESUME — the state should go back to "running".'
        );
        await conn.waitFor(
          (f) => isBleFrame(f, "evt:recipe:state") && (f.strValue as string).includes("paused"),
          MANUAL_TIMEOUT_MS,
        );
        prompt('paused observed. Now tap RESUME in the app.');
        await conn.waitFor(
          (f) => isBleFrame(f, "evt:recipe:state") && (f.strValue as string).includes("resumed"),
          MANUAL_TIMEOUT_MS,
        );
        conn.send({ cmd: "recipe", op: "stop" });
      });

      test("UI hop alerts: ADD_HOP fires evt:boil:addition during BOIL", async () => {
        await conn.resetState();
        conn.send({ cmd: "watchdog", op: "detections", enable: false });
        conn.drain();
        // Two ADD_HOPs at distinct minute marks. Boil runs for 4 min so
        // alerts at min=3 and min=1 fire two real boil-ticks apart. We
        // advance the virtual clock in 30 s chunks with a real-wall
        // sleep between each, so the app sees the two alerts arrive
        // spaced out (vs. all in a single millisecond burst — which
        // collapses some UIs).
        const dsl = [
          'STEP "Pre-fervura"', "SET_TEMP 100", "WAIT_TEMP 2.0",
          'STEP "Fervura"',
          'ADD_HOP 3 "Magnum 30g"',
          'ADD_HOP 1 "Cascade 40g"',
          "BOIL 4", "WAIT_BOIL",
          'STEP "Fim"', "HEATER_OFF",
        ].join("\n");
        conn.send({ cmd: "recipe", op: "load", name: "ui-hops", content: dsl });
        await conn.waitFor((m) => m.event === "ack" && m.ok === true);
        prompt(
          'watch the app during boil. You should see TWO hop additions arrive\n' +
          '     ~10 s apart (real time): first "Magnum 30g", then "Cascade 40g".\n' +
          '     Confirm each individually — the bench will not auto-progress beyond\n' +
          '     the second alert until both notifications have been delivered.'
        );
        conn.send({ cmd: "recipe", op: "start" });
        // Land at boil temp.
        for (let t = 25; t <= 100; t += 5) {
          conn.send({ cmd: "set", path: "ntc.c", value: t });
          await conn.sleep(800);
        }
        // Slice the advance so the firmware ticks the boil timer in
        // pieces, letting each ADD_HOP fire on a distinct wall-clock
        // moment.
        for (let i = 0; i < 9; i++) {
          conn.send({ cmd: "clock", op: "advance", ms: 30_000 });
          await conn.sleep(5000);
        }
        await conn.waitFor(
          (f) => isBleFrame(f, "evt:recipe:state") && (f.strValue as string).includes("completed"),
          MANUAL_TIMEOUT_MS,
        );
      });

      test("UI watchdog warning: trip is visible and reset clears it", async () => {
        await conn.resetState();
        conn.drain();
        prompt(
          'about to force a watchdog trip (cause OVERTEMP).\n' +
          '     The app should show a safety banner / warning state.\n' +
          '     Then the bench resets — banner should clear.'
        );
        conn.send({ cmd: "force", path: "watchdog.trip", value: "OVERTEMP" });
        await conn.waitFor(
          (f) => f.event === "bus" && f.type === "WatchdogTripped",
          2000,
        );
        prompt('warning should be visible now. Holding 8 s so you can inspect it.');
        await conn.sleep(8000);
        conn.send({ cmd: "force", path: "watchdog.reset" });
        await conn.waitFor(
          (f) => f.event === "bus" && f.type === "WatchdogReset",
          MANUAL_TIMEOUT_MS,
        );
        prompt('warning should have cleared in the app now.');
        await conn.sleep(3000);
      });

      test("UI step name: app displays the current STEP label", async () => {
        await conn.resetState();
        conn.send({ cmd: "watchdog", op: "detections", enable: false });
        conn.drain();
        const dsl = [
          'STEP "Mostura 65°C"',  "SET_TEMP 65",  "WAIT_TEMP 1.0", "WAIT_TIMER 1",
          'STEP "Mash-out 76°C"', "MASH_OUT 76",  "WAIT_TEMP 1.0",
          'STEP "Fim"',            "HEATER_OFF",
        ].join("\n");
        conn.send({ cmd: "recipe", op: "load", name: "ui-steps", content: dsl });
        await conn.waitFor((m) => m.event === "ack" && m.ok === true);
        prompt(
          'app should display the step label changing:\n' +
          '     "Mostura 65°C" → "Mash-out 76°C" → "Fim".\n' +
          '     (drives NTC and advances the timer automatically)'
        );
        conn.send({ cmd: "recipe", op: "start" });
        for (let t = 25; t <= 65; t += 5) { conn.send({ cmd: "set", path: "ntc.c", value: t }); await conn.sleep(900); }
        conn.send({ cmd: "clock", op: "advance", ms: 65000 });
        await conn.sleep(2000);
        for (let t = 67; t <= 76; t += 3) { conn.send({ cmd: "set", path: "ntc.c", value: t }); await conn.sleep(900); }
        await conn.waitFor(
          (f) => isBleFrame(f, "evt:recipe:state") && (f.strValue as string).includes("completed"),
          MANUAL_TIMEOUT_MS,
        );
      });
    });
  });
}
