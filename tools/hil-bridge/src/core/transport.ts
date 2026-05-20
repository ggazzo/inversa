// SerialPort wrapper with JSONL framing and `# `-log demux.
//
// The firmware emits one JSON object per line. Debug output (DEBUG_PRINT*)
// is prefixed with `# ` so we can drop those lines without parser hiccups.
// Lines that fail to parse as JSON after the prefix check are logged at
// debug level and dropped — the run continues.

import { SerialPort } from "serialport";
import type { CommandMsg, TelemetryMsg } from "./types.js";

export type TransportLog = (line: string) => void;

export type TransportOptions = {
  path: string;
  baudRate?: number;
  onLog?: TransportLog;
  onTelemetry: (msg: TelemetryMsg) => void;
  onParseError?: (line: string, err: unknown) => void;
};

export class SerialTransport {
  private port: SerialPort;
  private buffer = "";
  private opts: TransportOptions;
  private opened = false;

  constructor(opts: TransportOptions) {
    this.opts = opts;
    this.port = new SerialPort({
      path: opts.path,
      baudRate: opts.baudRate ?? 115200,
      autoOpen: false,
    });
    this.port.on("data", (chunk: Buffer) => this.onData(chunk));
    this.port.on("error", (err) => {
      // eslint-disable-next-line no-console
      console.error("[transport] serial error:", err);
    });
  }

  async open(): Promise<void> {
    return new Promise((resolve, reject) => {
      this.port.open((err) => {
        if (err) reject(err);
        else {
          this.opened = true;
          resolve();
        }
      });
    });
  }

  async close(): Promise<void> {
    if (!this.opened) return;
    return new Promise((resolve) => {
      this.port.close(() => resolve());
    });
  }

  send(cmd: CommandMsg | string): void {
    const line = typeof cmd === "string" ? cmd : JSON.stringify(cmd) + "\n";
    if (!this.opened) {
      // Drop sends before open — typically only the test harness does this.
      return;
    }
    this.port.write(line);
  }

  private onData(chunk: Buffer): void {
    this.buffer += chunk.toString("utf8");
    let nl: number;
    while ((nl = this.buffer.indexOf("\n")) >= 0) {
      const raw = this.buffer.slice(0, nl).replace(/\r$/, "");
      this.buffer = this.buffer.slice(nl + 1);
      this.handleLine(raw);
    }
  }

  private handleLine(line: string): void {
    if (line.length === 0) return;
    // `# ` prefix means firmware-side debug log. Strip and forward.
    if (line.startsWith("# ")) {
      this.opts.onLog?.(line.slice(2));
      return;
    }
    // Tolerate single `#` too (no space) — some libraries trim trailing ws.
    if (line.startsWith("#")) {
      this.opts.onLog?.(line.slice(1));
      return;
    }
    try {
      const msg = JSON.parse(line) as TelemetryMsg;
      this.opts.onTelemetry(msg);
    } catch (err) {
      this.opts.onParseError?.(line, err);
    }
  }
}

// Stdin/stdout transport — handy for testing the bridge against a fake
// firmware that just dumps JSONL to stdout. Same wire format.
export class StdioTransport {
  private buffer = "";
  private opts: TransportOptions;

  constructor(opts: TransportOptions) {
    this.opts = opts;
  }

  async open(): Promise<void> {
    process.stdin.setEncoding("utf8");
    process.stdin.on("data", (chunk: string | Buffer) => {
      this.buffer += typeof chunk === "string" ? chunk : chunk.toString("utf8");
      let nl: number;
      while ((nl = this.buffer.indexOf("\n")) >= 0) {
        const raw = this.buffer.slice(0, nl).replace(/\r$/, "");
        this.buffer = this.buffer.slice(nl + 1);
        this.handleLine(raw);
      }
    });
  }

  async close(): Promise<void> {
    /* no-op */
  }

  send(cmd: CommandMsg | string): void {
    const line = typeof cmd === "string" ? cmd : JSON.stringify(cmd) + "\n";
    process.stdout.write(line);
  }

  private handleLine(line: string): void {
    if (line.length === 0) return;
    if (line.startsWith("# ")) {
      this.opts.onLog?.(line.slice(2));
      return;
    }
    try {
      const msg = JSON.parse(line) as TelemetryMsg;
      this.opts.onTelemetry(msg);
    } catch (err) {
      this.opts.onParseError?.(line, err);
    }
  }
}
