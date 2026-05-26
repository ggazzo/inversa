# LossTune — auto-tune of pot heat-loss coefficient

LossTune measures the heat-transfer coefficient `h` (W/m²·K) of the
pot + liquid + ambient system by observing the natural cooling curve
and fitting Newton's law of cooling. The result becomes the
coefficient that the PID feed-forward and the `be ready at` scheduler
use to estimate losses.

> Two coefficients are persisted: one for lid closed
> (`lossCoeffLidOn`) and one for lid open (`lossCoeffLidOff`). Each
> requires its own LossTune run.

---

## Physics in short

With the heater off:

```
m · c · dT/dt = -h · A · (T - T_amb)
T(t) = T_amb + ΔT₀ · exp(-t/τ),   τ = m·c / (h·A)
```

LossTune linearizes `y(t) = ln(T(t) - T_amb)`, runs a single-pass
linear regression (Welford), and derives:

```
h = (m · c) / (τ · A)
```

where `m = volumeL` kg, `c = 4186 J/(kg·K)`, and `A` comes from the
cylindrical geometry in
`ThermalCalc::cylinderSurfaceArea(volumeL, diameterM)`.

---

## When to run

- After **changing the pot** (geometry changed).
- After **changing the liquid** (water ↔ dense wort changes effective `c`).
- After **changing insulation** (thermal blanket, new lid).
- After **changing the environment** (kitchen ↔ ventilated garage).
- Once per lid mode (closed **and** open).

No need to repeat before every brewday — `h` is stable as long as the
setup doesn't change.

---

## Preconditions

| Item | Where to configure |
|------|--------------------|
| Correct `volumeL`, `diameterM` | Equipment wizard |
| Realistic ambient (manual or sensor) | Equipment wizard → `Ambient (°C)` field |
| Temperature sensor calibrated | Calibration sheet |
| No recipe running, no PID AutoTune active | `Idle` state |
| Watchdog **not** latched | Reset first if tripped |

The pot must be filled with the liquid (actual volume ≈ configured
`volumeL`), with the lid in the position being calibrated, in a stable
environment (no significant draft, no stirring).

---

## How to run (UI)

1. Open the **Equipment** wizard.
2. Confirm volume, diameter, ambient.
3. Set the lid in the mode being calibrated (closed or open) and tap
   the matching button in the "Lid in use" card so the firmware knows.
4. In the **"Auto-tune of loss coefficient"** card, tap:
   - **Calibrate lid closed** — writes to `lossCoeffLidOn`.
   - **Calibrate lid open** — writes to `lossCoeffLidOff`.
5. UI switches to the `LossTune` state. Watch the phases (HEAT →
   SOAK → DECAY → FIT → RESULT). Typical total: **10-15 min**.
6. At `RESULT`, check `h`, `τ`, and `R²` in the card.
   - **Save coefficient**: persists in NVS and takes effect immediately.
   - **Discard**: returns to `IDLE` with no changes.

Cancel at any time releases the SSR and zeroes the setpoint.

---

## State-machine phases

| Phase | What it does | Exit |
|-------|--------------|------|
| `PREFLIGHT` | Validates volume, diameter, sensor, watchdog. Computes target `min(T_amb + 50, 85, hardStop - 10)`. | → `HEAT` or `ERROR` |
| `HEAT` | PID drives to target. | → `SOAK` when `T ≥ target - 0.5°C` |
| `SOAK` | Holds ±0.5°C for 30 s to homogenize. | → `DECAY` (releases SSR, saves `T₀`) |
| `DECAY` | SSR off. Samples at 1 Hz, streams in 5 s batches. | → `FIT` when `T - T_amb ≤ 5°C` or 10 min cap |
| `FIT` | Least-squares on `(t, ln(T-T_amb))`, drops first 20 s. | → `RESULT` or `ERROR` |
| `RESULT` | Awaits user decision. | accept/reject |

The watchdog **stays armed** throughout. The HEAT target stays
comfortably below `WATCHDOG_DEFAULT_HARDSTOP_C` (105°C).

---

## Acceptance gates (firmware)

A failed gate rejects the fit with a reason in `lossTuneError`:

| Gate | Constant | Error |
|------|----------|-------|
| `R² ≥ 0.98` | `LOSSTUNE_MIN_R2` | `r2_low` |
| `60s ≤ τ ≤ 7200s` | `LOSSTUNE_TAU_MIN_S`/`_MAX_S` | `tau_out_of_range` |
| `1 ≤ h ≤ 50 W/m²K` | `LOSSTUNE_COEFF_MIN`/`_MAX` | `coeff_out_of_range` |
| Usable samples ≥ 120 (after head-drop of 20 s) | `LOSSTUNE_MIN_SAMPLES` | `samples_low` |
| Ambient drift ≤ 1°C **(sensor mode only)** | `LOSSTUNE_AMBIENT_DRIFT_C` | `ambient_drift` |
| Target ΔT - ambient > 10°C | (PREFLIGHT) | `delta_too_small` |

Other possible errors: `sensor_fault`, `watchdog_trip`, `busy_mode`,
`already_running`, `volume_invalid`, `diameter_invalid`,
`watchdog_latched`, `fit_singular`, `fit_flat`,
`fit_positive_slope`.

---

## Ambient: manual or sensor

LossTune uses `getEffectiveAmbient()`. Today:

- `ambientSource = MANUAL` → reads the value typed in the wizard.
- `ambientSource = SENSOR` → reads `gState.ambientSensorC` if
  `ambientSensorOk` **and** reading is newer than 60 s; otherwise
  falls back to manual.

`AmbientSensorPlugin` is a stub today (no hardware). The UI shows a
badge next to the ambient field:

- **Manual** — typed value.
- **Sensor (ok)** — fresh reading.
- **Sensor (stale → manual)** — sensor configured but stale reading,
  fallback active.

In manual mode the ambient-drift gate is **skipped** (no way to
verify), and the result card shows a warning that the fit was
anchored to a typed value.

---

## When auto-tune fails — what to check

| Error | Typical cause | Action |
|-------|---------------|--------|
| `r2_low` | Draft, pot was moved, steam leak | Redo in a more stable environment |
| `tau_out_of_range` (short) | Actual volume << configured, very warm ambient | Verify volume/ambient |
| `tau_out_of_range` (long) | Extreme insulation, pot too full | Verify geometry |
| `coeff_out_of_range` | Wrong volume/diameter/ambient | Redo configuration |
| `samples_low` | Decay ended early (ΔT too small) | Increase ambient or volume |
| `ambient_drift` | Ambient sensor recorded > 1°C variation | Stabilize the environment |
| `delta_too_small` | Ambient too high (target - amb ≤ 10°C) | Wait for ambient to cool |
| `watchdog_trip` | Watchdog tripped during HEAT | Investigate before retrying |
| `sensor_fault` | Pot sensor faulted during the run | Check NTC wiring |

---

## Bench validation (recommended before saving)

1. **Sim round-trip**: set `heatLossCoeffLidOn = 12.0` in
   `ThermalSim`, run LossTune, expect `h ≈ 12 ± 5%`.
2. **Real, lid closed**: run with the lid on, save.
3. **Real, lid open**: run without the lid, save.
4. Confirm `lossCoeffLidOn < lossCoeffLidOff` (closed lid loses less
   heat). Typical ratio: 1.3-2.0x.
5. Confirm the watchdog **did not trip** during HEAT.
6. In a normal recipe, verify the setpoint stabilizes with smaller
   overshoot (feed-forward better calibrated).

---

## BLE protocol

### Requests (app → device)

```json
{ "tp": "req:losstune:start",  "mode": "lidOn" }   // or "lidOff"
{ "tp": "req:losstune:cancel" }
{ "tp": "req:losstune:accept" }                    // only in RESULT
{ "tp": "req:losstune:reject" }                    // only in RESULT
{ "tp": "req:lid:set",         "mode": "lidOn" }   // runtime lid selector
```

`req:settings:thermal:get/set` now accepts/returns:

```json
{
  "volumeL": 25.0, "powerW": 3000, "ambientC": 22.0, "diameterM": 0.35,
  "lossCoeffLidOn": 8.0, "lossCoeffLidOff": 12.0,
  "lidState": "lidOn",
  "ambientSource": "manual",
  "ambientSensorOk": false, "ambientSensorC": 0.0,
  "ambientEffectiveC": 22.0
}
```

### Events (device → app)

`evt:losstune:status` — emitted on phase change + ~1 Hz during active
phases:

```json
{ "tp": "evt:losstune:status",
  "ph": "DECAY", "pct": 65, "mode": "lidOn",
  "amb0": 22.0, "ambE": 22.1, "src": "manual",
  "n": 240, "err": "" }
```

`evt:losstune:sample` — batch of samples during DECAY (5 s @ 1 Hz):

```json
{ "tp": "evt:losstune:sample",
  "t0": 60,
  "T": [78.2, 77.9, 77.5, 77.1, 76.8] }
```

`evt:losstune:result` — when entering RESULT:

```json
{ "tp": "evt:losstune:result",
  "mode": "lidOn", "h": 8.4, "tau": 1820, "r2": 0.9912,
  "n": 480, "amb0": 22.0, "ambE": 22.2, "src": "manual" }
```

`evt:status` gains a `lt` sub-object when a tune is active:

```json
{ "lt": { "ph": "DECAY", "pct": 65, "mode": "lidOn",
          "r2": 0.0, "h": 0.0, "n": 240, "err": "" } }
```

And always:

```json
{ "ambEff": 22.0, "ambSrc": "manual",
  "ambSensorOk": false, "lidState": "lidOn" }
```

---

## Persistence

New NVS keys (namespace `inversa` (kept for backwards-compat across the BrewPilot rebrand)):

| Key | Type | Content |
|-----|------|---------|
| `therm_loss_on` | float | h, lid closed |
| `therm_loss_off` | float | h, lid open |
| `therm_amb_src` | uint8 | 0=MANUAL, 1=SENSOR |

Migration of the legacy `therm_loss_c` key happens automatically at
boot: if either `therm_loss_on` or `therm_loss_off` is missing, the
legacy value is copied into both slots. Coverage: pre-LossTune
installs upgrade without losing calibration.

---

## Where the code lives

| Layer | Path |
|-------|------|
| Firmware plugin | `firmware/src/plugins/LossTunePlugin.h` |
| Sensor stub | `firmware/src/plugins/AmbientSensorPlugin.h` |
| Constants | `firmware/src/core/constants.h` (`LOSSTUNE_*`) |
| Global state | `firmware/src/models/MachineState.h` (`getEffectiveAmbient`, `chooseLossCoeff`) |
| NVS | `firmware/src/core/NVSStorage.h` (`saveThermalLossCoeff`, `migrateLegacyLossCoeff`) |
| Protocol | `firmware/src/protocol/protocol.h` (`REQ_LOSSTUNE_*`, `EVT_LOSSTUNE_*`) |
| BLE routing | `firmware/src/plugins/CommandHandler.h` |
| Sim | `firmware/sim/ThermalSim.h` (dual coeff + `setLid`) |
| Services TS | `packages/services/src/ConnectionManager.ts` (`startLossTune`, `setLidState`) |
| Stores TS | `packages/stores/src/state.ts` (`lossTune*`, `ambient*`, `lidState`) |
| UI state | `packages/ui/src/views/states/LossTune.tsx` |
| UI entry | `packages/ui/src/components/wizards/WizardEquipment.tsx` |

---

## Future

- **Ambient sensor**: implement `AmbientSensorPlugin::loop()` filling
  `gState.ambientSensorC` + `ambientSensorOk` + `ambientSensorLastMs`.
  No consumer needs to change.
- **Persist R² + samples**: today only `h` is persisted; storing the
  last tune's metadata per mode would help diagnostics.
- **Tune with wort**: `c_water` is hardcoded; dense wort has `c` ~3-5%
  lower. Acceptable inside the ±5% sim round-trip gate; if it becomes
  critical, expose `specificHeat` in the payload.
