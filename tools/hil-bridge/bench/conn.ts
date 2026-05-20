// Shared connection helper for the HIL bench.
//
// Opens a single SerialPort, exposes send/waitFor primitives, and is
// shared across all node:test cases via the top-level `before`/`after`
// hooks. Concurrency is set to 1 in the test runner so cases see the
// firmware sequentially — there is one ESP and one transport.

import { SerialPort } from "serialport";
import type { CommandMsg } from "../src/core/types.js";

export interface Frame {
  event?: string;
  type?: string;
  t?: number;
  strValue?: string;
  floatValue?: number;
  intValue?: number;
  boolValue?: boolean;
  [k: string]: unknown;
}

export interface HilConn {
  send(cmd: CommandMsg): void;
  drain(): Frame[];
  waitFor(predicate: (f: Frame) => boolean, timeoutMs?: number): Promise<Frame>;
  waitForState(predicate: (s: Frame) => boolean, timeoutMs?: number): Promise<Frame>;
  sleep(ms: number): Promise<void>;
  // Bring the ESP to a quiescent state: NTC at 22°C, ambient at 22°C,
  // detections on, no streams, clock unfrozen, watchdog reset.
  resetState(): Promise<void>;
  close(): Promise<void>;
}

export async function openConn(path: string): Promise<HilConn> {
  const port = new SerialPort({ path, baudRate: 115200, autoOpen: false });
  let buf = "";
  let synced = false;
  const inbox: Frame[] = [];

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
        inbox.push(m);
      } catch {
        // Drop non-JSON noise silently — the parser test exercises
        // malformed input explicitly.
      }
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
      for (let i = 0; i < inbox.length; i++) {
        if (pred(inbox[i]!)) {
          const f = inbox[i]!;
          inbox.splice(0, i + 1);
          return f;
        }
      }
      inbox.length = 0;
      await sleep(40);
    }
    throw new Error(`timeout after ${timeoutMs}ms waiting for frame`);
  };
  const waitForState = (pred: (s: Frame) => boolean, timeoutMs = 2500) =>
    waitFor((m) => m.event === "state" && pred(m), timeoutMs);

  // Initial subscription so cases that look for `state` or `bus` frames
  // see them without each having to subscribe individually.
  await sleep(600);
  send({ cmd: "sub", topic: "events", enable: true });
  send({ cmd: "sub", topic: "state", hz: 2 });
  await sleep(800);
  inbox.length = 0;

  const resetState = async (): Promise<void> => {
    send({ cmd: "recipe", op: "stop" });
    send({ cmd: "watchdog", op: "detections", enable: true });
    send({ cmd: "clock", op: "unfreeze" });
    send({ cmd: "set", path: "ntc.bypassKalman", value: true });
    send({ cmd: "set", path: "ntc.c", value: 22 });
    send({ cmd: "set", path: "ambient.c", value: 22 });
    send({ cmd: "set", path: "ambient.ok", value: true });
    await sleep(1500);
    // Try a couple of times — reset is rejected while temp is unsafe.
    for (let i = 0; i < 5; i++) {
      send({ cmd: "force", path: "watchdog.reset" });
      await sleep(400);
    }
    drain();
  };

  const close = async (): Promise<void> => {
    send({ cmd: "sub", topic: "state", hz: 0 });
    send({ cmd: "sub", topic: "events", enable: false });
    send({ cmd: "sub", topic: "ssr", enable: false });
    await sleep(300);
    await new Promise<void>((r) => port.close(() => r()));
  };

  return { send, drain, waitFor, waitForState, sleep, resetState, close };
}
