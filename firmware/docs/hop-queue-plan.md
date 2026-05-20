# Future plan: firmware-side hop addition queue

> Status: **proposed**, not implemented. Documented here so the design
> decision is captured for the next iteration. Current behavior — fire
> `evt:boil:addition` BLE notify on each scheduled hop, no firmware
> state — stays for now. The PWA queue fix in
> `packages/ui/src/components/HopAlertOverlay.tsx` (see PR adding
> HIL build) covers the common "two hops fire close together" case.

## Why consider it

Today the firmware fires a single BLE notify per scheduled hop and
forgets about it. Consequences:

- If the app is **disconnected** at the moment of firing (WiFi blip, BLE
  drop, PWA backgrounded long enough for the browser to GC the
  connection), the notify is silently lost. The operator returns and
  has no way to recover which hops were due.
- If the firmware **reboots** mid-boil (power blip, watchdog trip and
  recovery), the schedule of pending hops is not part of
  `RecoveryData`, so the recovered boil resumes with empty intent.
- A second client (native app and PWA on a phone, say) sees an
  inconsistent view because each only receives the notify that
  happened to arrive during their connection window.

A firmware queue would absorb all three failure modes. The boil timer
keeps its non-blocking guarantee (chemistry-driven; pausing for a
confirm would skew IBU, hop aroma, etc.) — only the bookkeeping moves
to the firmware.

## Proposed shape

### State (BoilTimerPlugin)

```cpp
struct PendingHop {
    uint16_t id;          // monotonic per boil
    char     name[24];    // truncated copy of ADD_HOP "name"
    uint8_t  minMark;     // the min value from ADD_HOP <minMark> "name"
    uint32_t firedAtMs;   // millis() when the alert fired
};

std::vector<PendingHop> _pendingHops;
uint16_t                _nextHopId = 1;
```

Cap the vector at a sensible MAX (e.g. 16 — well above any realistic
recipe) and reject new pushes past the cap with an error event.

### Protocol additions

| Direction | Frame | Notes |
|-----------|-------|-------|
| ESP → app | `{"tp":"evt:boil:addition","id":<n>,"name":"<…>","min":<n>}` | `id` added to the existing payload |
| app → ESP | `{"tp":"req:boil:confirm_hop","id":<n>}` | New request; CommandHandler routes to BoilTimerPlugin::confirmHop |
| ESP → app | `evt:status` adds `"ph":[{"id","name","min"}, …]` | Snapshot on every periodic status |

### Lifecycle

1. Boil timer hits a scheduled minute mark:
   - Push `PendingHop{id, name, min, firedAtMs}` into `_pendingHops`.
   - Publish existing `BoilAdditionAlert` event for legacy consumers.
   - Publish `BLESend` with `{tp:evt:boil:addition, id, name, min}`.
2. App receives the notify, displays an overlay/toast, and on user
   confirm sends `req:boil:confirm_hop` with the `id`.
3. CommandHandler calls `BoilTimerPlugin::confirmHop(id)`, which
   removes the matching entry and replies `ok`.
4. Periodic `evt:status` always echoes the remaining `_pendingHops` so
   any client (newly connected or reconnected) can render the queue
   without missing entries.

### Recovery

Extend `RecoveryData` to include the pending hop array (DT-06 in
`_reversa_sdd/architecture.md#7` already lists this gap for several
state fields). On boot the recovery path repopulates `_pendingHops`,
the PWA's `boilAlerts` is re-derived from `evt:status.ph`, and the
queue is restored across power loss.

## Why it is not in this PR

- The HIL PR is already broad (firmware + bridge + bench + PWA fix).
  Stacking another firmware behavior change widens the review surface.
- DT-06 is a known recovery gap and is best addressed in a single
  focused pass over `RecoveryData`. The hop queue should land with that
  work rather than as an isolated half-step.
- The immediate user complaint ("confirmed one hop, all marked added")
  is already resolved by the PWA-side queue fix shipped in the same PR.
  That covers the realistic case where the app stays connected.

## When to revisit

Trigger this work the next time any of these happens:

- A user reports losing a hop to a brief disconnect during boil.
- A second client (native app on phone) is added and inconsistent hop
  state across clients becomes visible.
- DT-06 (recovery coverage) is opened — the hop queue should land in
  the same RecoveryData migration.

## References

- `firmware/src/plugins/BoilTimerPlugin.h` — current ADD_HOP scheduling.
- `firmware/src/plugins/CommandHandler.h` — where `req:boil:confirm_hop` would be routed.
- `firmware/src/core/RecoveryData.h` — RecoveryData struct (currently packed binary, format bump required).
- `packages/ui/src/components/HopAlertOverlay.tsx` — PWA queue UI; would consume `id` from notify and `ph` array from status.
- `_reversa_sdd/architecture.md#7` (DT-06) — pre-existing recovery coverage gap.
