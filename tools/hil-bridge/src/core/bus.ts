// Host-side pub/sub for telemetry. The transport pushes parsed
// TelemetryMsg objects through here; plugins consume via subscribe.

import type { TelemetryMsg } from "./types.js";

type Listener = (msg: TelemetryMsg) => void;

export class HostBus {
  private listeners: Set<Listener> = new Set();

  subscribe(fn: Listener): () => void {
    this.listeners.add(fn);
    return () => this.listeners.delete(fn);
  }

  publish(msg: TelemetryMsg): void {
    // Snapshot to tolerate listeners unsubscribing during dispatch.
    for (const fn of [...this.listeners]) {
      try {
        fn(msg);
      } catch (err) {
        // Don't let one broken plugin take the whole stream down.
        // eslint-disable-next-line no-console
        console.error("[bus] listener threw:", err);
      }
    }
  }
}
