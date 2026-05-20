// Runner — wires transport + clock + bus + plugins into a single async
// loop. Owns the tick cadence and shutdown sequencing.

import { HostBus } from "./bus.js";
import { VirtualClock } from "./clock.js";
import { SerialTransport, StdioTransport } from "./transport.js";
import type {
  CommandMsg,
  HilPlugin,
  HilPluginContext,
  Report,
  TelemetryMsg,
} from "./types.js";

export type RunnerOptions = {
  // One of: serial path, or "-" for stdio (testing).
  port: string;
  baudRate?: number;
  plugins: HilPlugin[];
  // Wall-clock ms per tick (50 = 20 Hz). Bridge sends sim updates at this
  // rate. The virtual clock is advanced independently via plugin calls.
  tickMs?: number;
  // Total wall-clock budget; runner exits and shuts down plugins after.
  durationMs: number;
  // State subscription rate to request from firmware.
  stateHz?: number;
  // Log line handler for `# `-prefixed firmware debug. Defaults to stderr.
  onLog?: (line: string) => void;
};

export type RunResult = {
  ok: boolean;
  reports: Report[];
};

export async function runScenario(opts: RunnerOptions): Promise<RunResult> {
  const bus = new HostBus();
  let transport: SerialTransport | StdioTransport;
  // Build the clock with a forwarding shim so we can construct it before
  // the transport (chicken-and-egg around onTelemetry).
  const sendQueue: string[] = [];
  const clock = new VirtualClock({
    send: (line) => {
      if (transport) transport.send(line);
      else sendQueue.push(line);
    },
  });

  const onTelemetry = (msg: TelemetryMsg) => {
    if ("t" in msg && typeof msg.t === "number") clock.observe(msg.t);
    bus.publish(msg);
    for (const p of opts.plugins) p.onTelemetry?.(msg, ctx);
  };

  const transportOpts = {
    path: opts.port,
    baudRate: opts.baudRate ?? 115200,
    onLog: opts.onLog ?? ((line) => process.stderr.write(`[fw] ${line}\n`)),
    onTelemetry,
    onParseError: (line: string, err: unknown) => {
      process.stderr.write(
        `[bridge] parse error: ${(err as Error).message} :: ${line.slice(0, 120)}\n`,
      );
    },
  };
  transport =
    opts.port === "-"
      ? new StdioTransport(transportOpts)
      : new SerialTransport(transportOpts);

  const ctx: HilPluginContext = {
    clock,
    bus,
    send: (cmd: CommandMsg) => transport.send(cmd),
  };

  await transport.open();
  // Flush anything queued before open.
  for (const line of sendQueue) transport.send(line);
  sendQueue.length = 0;

  // Subscribe to state at the requested rate.
  // Default to 10 Hz to match the firmware-side cap and avoid starving
  // the NimBLE host task on the shared USB CDC pipe.
  const hz = opts.stateHz ?? 10;
  transport.send({ cmd: "sub", topic: "state", hz });
  transport.send({ cmd: "sub", topic: "ssr", enable: true });
  transport.send({ cmd: "sub", topic: "events", enable: true });

  // Set up plugins.
  for (const p of opts.plugins) await p.setup(ctx);

  // Tick loop.
  const tickMs = opts.tickMs ?? 50;
  const start = Date.now();
  let stopped = false;
  const tickHandle = setInterval(() => {
    if (stopped) return;
    const vt = clock.now();
    for (const p of opts.plugins) p.onTick?.(vt, ctx);
  }, tickMs);

  // Wait for duration.
  await new Promise((r) => setTimeout(r, opts.durationMs));
  stopped = true;
  clearInterval(tickHandle);

  // Best-effort: silence the firmware-side streams before closing. The
  // ESP keeps subs in RAM until reboot, so leaving them on at 10 Hz
  // starves the NimBLE host task for the next user that just wants the
  // BLE app. Failing to send (port already closed, etc) is ignored.
  try {
    transport.send({ cmd: "sub", topic: "state", hz: 0 });
    transport.send({ cmd: "sub", topic: "ssr", enable: false });
    transport.send({ cmd: "sub", topic: "events", enable: false });
    // Give the writes a chance to drain before we close.
    await new Promise((r) => setTimeout(r, 50));
  } catch {
    // ignore
  }

  // Collect reports.
  const reports: Report[] = [];
  for (const p of opts.plugins) reports.push(await p.onShutdown(ctx));
  await transport.close();
  const elapsedMs = Date.now() - start;
  process.stderr.write(`[bridge] run complete in ${elapsedMs}ms wall\n`);
  return {
    ok: reports.every((r) => r.ok),
    reports,
  };
}
