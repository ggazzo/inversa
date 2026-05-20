// VirtualClock — mirror of the firmware-side hil_clock_now().
//
// Tracks the cumulative ms we have asked the firmware to advance, plus the
// latest `t` value observed in telemetry. Either is a valid proxy depending
// on context:
//   - `appliedOffsetMs` is what we sent; useful BEFORE the first telemetry
//     line arrives.
//   - `lastObservedT` is what the firmware actually saw; preferred once we
//     have telemetry in flight (because real wall-clock ms also pass).
//
// `now()` returns the larger of the two so analyzer deadlines never go
// backwards as messages arrive out of order with our send pipeline.

export type Transport = {
  send(line: string): void;
};

export class VirtualClock {
  private appliedOffsetMs = 0;
  private lastObservedT = 0;
  private frozen = false;

  constructor(private transport: Transport) {}

  /** Best-effort virtual-ms estimate. */
  now(): number {
    return Math.max(this.appliedOffsetMs, this.lastObservedT);
  }

  /** Called by the transport for every inbound `t` field. */
  observe(t: number): void {
    if (t > this.lastObservedT) this.lastObservedT = t;
  }

  advance(ms: number): void {
    if (ms <= 0) return;
    this.appliedOffsetMs += ms;
    this.transport.send(
      JSON.stringify({ cmd: "clock", op: "advance", ms }) + "\n",
    );
  }

  freeze(): void {
    this.frozen = true;
    this.transport.send(JSON.stringify({ cmd: "clock", op: "freeze" }) + "\n");
  }

  unfreeze(): void {
    this.frozen = false;
    this.transport.send(
      JSON.stringify({ cmd: "clock", op: "unfreeze" }) + "\n",
    );
  }

  setEpoch(unixSeconds: number): void {
    this.transport.send(
      JSON.stringify({ cmd: "clock", op: "epoch", value: unixSeconds }) + "\n",
    );
  }

  isFrozen(): boolean {
    return this.frozen;
  }
}
