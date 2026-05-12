// telemetryFreshness.js — observes how stale the device's last telemetry is.
//
// `ConnectionManager._handleMessage` sets `lastTelemetryMs` on every
// `evt:status` arrival. UI components read it via the `isStale` computed
// (true when >3 s without an update) so the temp display can dim and a
// "STALE" badge can show — building trust during long brews and giving
// the operator an obvious signal when the device hangs.

import { signal, computed } from '@preact/signals';

export const lastTelemetryMs = signal(0);

// Tick once a second so `isStale` re-evaluates without relying on a new
// telemetry message to flip from fresh → stale.
const now = signal(Date.now());
setInterval(() => { now.value = Date.now(); }, 1000);

export const STALE_THRESHOLD_MS = 3000;
export const isStale = computed(() =>
    lastTelemetryMs.value !== 0 &&
    (now.value - lastTelemetryMs.value) > STALE_THRESHOLD_MS
);
