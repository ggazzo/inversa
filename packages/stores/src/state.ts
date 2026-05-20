// state.js — Global application state using Preact Signals
import { signal, computed } from '@preact/signals-react';

// ─── Connection State ───────────────────────────────────────
export const isConnected = signal(false);
export const deviceName = signal('');

// ─── Telemetry (updated from BLE evt:status) ────────────────
export const currentTemp = signal(0);
export const targetTemp = signal(0);
export const pidOutput = signal(0);
export const heaterOn = signal(false);
export const pumpOn = signal(false);
export const mode = signal('idle');   // idle, manual, recipe, tuning

// UI-only intent flag. User taps "Modo Manual" from Idle → we want
// the Manual sub-state to render before the firmware has heard about
// it (firmware only flips to OperatingMode::Manual on req:set-temp /
// req:heater:on, which the user does *from* the Manual view itself).
// Without this, the first evt:status that arrives after the tap
// resets mode→idle and the Manual view unmounts mid-input. Cleared
// when firmware reports a non-idle mode OR on disconnect.
export const manualIntent = signal<boolean>(false);
export const uptime = signal(0);
export const tempSensorOk = signal(true);
export const safetyShutoff = signal(false);

// ─── Recovery State ─────────────────────────────────────────
export const hasRecovery = signal(false);
export const recoveryRecipeName = signal('');

// ─── WiFi State ─────────────────────────────────────────────
export const wifiConnected = signal(false);
export const wifiSSID = signal('');
export const wifiIP = signal('');
export const wifiConfiguredSSID = signal('');  // Stored SSID in NVS

// ─── OTA State ──────────────────────────────────────────────
export const otaStatus = signal('idle');  // idle, checking, available, downloading, installing, error, up-to-date
export const otaLatestVersion = signal('');
export const otaProgress = signal(0);
export const otaError = signal('');
export const firmwareVersion = signal('');  // Current firmware version

// ─── Ramp State ─────────────────────────────────────────────
export const rampActive = signal(false);
export const rampRate = signal(0);      // °C/min
export const rampTarget = signal(0);
export const rampCurrent = signal(0);

// ─── Brew Log State ─────────────────────────────────────────
export const brewLogActive = signal(false);
export const brewLogEntries = signal(0);
export const brewLogData = signal([]);  // Collected log entries for export

// ─── Notifications State ────────────────────────────────────
export const notificationsEnabled = signal(false);
export const notifyOnTempReached = signal(true);
export const notifyOnStepComplete = signal(true);

// ─── Boil Timer State ───────────────────────────────────────
export const boilActive = signal(false);
export const boilTotal = signal(0);       // Total seconds
export const boilRemaining = signal(0);   // Remaining seconds
// `bad` in telemetry is the count of configured additions, not the list.
// The actual list with names is collected in `boilAlerts` from each
// `evt:boil:addition` event. Legacy state.js typed both as `[]` and
// happened to work in JS; here we model them properly.
export const boilAdditions = signal<number>(0);
export const boilAlerts = signal<Array<{ name: string; min: number; time: Date }>>([]);

// ─── Mash-Out State ─────────────────────────────────────────
export const mashOutEnabled = signal(false);
export const mashOutTemp = signal(76.0);

// ─── RTC State ──────────────────────────────────────────────
export const rtcAvailable = signal(false);
export const rtcTimestamp = signal(0);
export const rtcNtpSynced = signal(false);

// ─── Timer State (generic countdown/alarm) ──────────────────
export const timerActive = signal(false);
export const timerPaused = signal(false);
export const timerTotal = signal(0);       // Total seconds
export const timerRemaining = signal(0);   // Remaining seconds
export const timerMode = signal(0);        // 0=relative, 1=absolute
export const timerAlarmHour = signal(0);   // Alarm hour (absolute mode)
export const timerAlarmMinute = signal(0); // Alarm minute (absolute mode)

// ─── Scheduler State ("be ready at HH:MM") ──────────────────
export const schedulerActive = signal(false);
export const schedulerTargetHour = signal(0);
export const schedulerTargetMinute = signal(0);
export const schedulerTargetTemp = signal(0);
export const schedulerVolume = signal(0);
export const schedulerStatus = signal('');

// ─── Auto-Tune State ────────────────────────────────────────
export const autoTuneActive = signal(false);
export const autoTuneProgress = signal(0);

// ─── Thermal Watchdog State (001-thermal-watchdog) ──────────
// Driven by the `wd:{...}` nested object inside `evt:status` telemetry,
// plus the discrete `evt:watchdog:tripped` and `evt:watchdog:reset`
// events handled in ConnectionManager._handleMessage.
export const watchdogArmed         = signal(false);
export const watchdogTripped       = signal(false);
export const watchdogTripCount     = signal(0);
export const watchdogLastCause     = signal<string>('');   // 'OVERTEMP' | 'SENSOR_FAULT' | 'LOOP_STUCK' | 'GRADIENT' | 'MANUAL' | ''
export const watchdogLastTripUnix  = signal(0);
export const watchdogHardStopC     = signal(105.0);
export const watchdogAutoReset     = signal(true);
// Full config mirror — populated from the `wd` object once the firmware
// reports each field. Defaults match constants.h so the UI renders
// sensible values before the first telemetry frame.
export const watchdogSensorFaultMs   = signal(10000);
export const watchdogLoopStuckMs     = signal(5000);
export const watchdogGradFactor      = signal(5);
export const watchdogGradWindow      = signal(20);
export const watchdogSafeAutoresetC  = signal(40.0);
export const watchdogCoolMinMs       = signal(300000);
// True when the firmware emitted at least one `wd` object — used by the
// UI to know whether to render any watchdog indicator at all.
export const watchdogSupported     = signal(false);

// ─── Ambient (effective + source) ───────────────────────────
// `ambientEffectiveC` is what every consumer on the firmware actually
// uses — sensor when ambientSource==SENSOR and reading is fresh, else
// the manual value. The UI shows the badge based on `ambientSource` +
// `ambientSensorOk` so the user knows whether their fit is anchored to
// a real sensor or a typed-in value.
export const ambientEffectiveC = signal(25.0);
export const ambientSource     = signal<'manual' | 'sensor'>('manual');
export const ambientSensorOk   = signal(false);
export const lidState          = signal<'lidOn' | 'lidOff'>('lidOn');

// ─── LossTune state ─────────────────────────────────────────
// Mirrors LossTunePlugin progress. `lossTuneSamples` is the live cooling
// curve streamed during DECAY (capped at 600 = LOSSTUNE_BUFFER_SIZE).
// `lossTuneFittedCoeff` + `lossTuneR2` + `lossTuneTau` are populated by
// evt:losstune:result; UI uses these to render the accept/reject card.
export type LossTunePhaseName =
  'IDLE' | 'PREFLIGHT' | 'HEAT' | 'SOAK' | 'DECAY' | 'FIT' | 'RESULT' | 'ERROR';

export const lossTuneActive         = signal(false);
export const lossTunePhase          = signal<LossTunePhaseName>('IDLE');
export const lossTuneProgress       = signal(0);
export const lossTuneTargetMode     = signal<'lidOn' | 'lidOff'>('lidOn');
export const lossTuneAmbientStart   = signal(0);
export const lossTuneAmbientEnd     = signal(0);
export const lossTuneAmbientSource  = signal<'manual' | 'sensor'>('manual');
export const lossTuneSampleCount    = signal(0);
export const lossTuneFittedCoeff    = signal(0);
export const lossTuneR2             = signal(0);
export const lossTuneTau            = signal(0);
export const lossTuneError          = signal('');
export const lossTuneSamples        = signal<Array<{ t: number; T: number }>>([]);

// ─── Brewing Step (UI visualization) ────────────────────────
export const brewingStep = signal(0);
export const brewingStepCustom = signal('');

// Step names mapping
const STEP_NAMES: Record<number, string> = {
  0: '',
  1: 'Pre-aquecimento',
  2: 'Mostura',
  3: 'Mash-out',
  4: 'Lavagem',
  5: 'Fervura',
  6: 'Lupulagem',
  7: 'Resfriamento',
  8: 'Concluido'
};

export const brewingStepName = computed(() => {
  if (brewingStepCustom.value) return brewingStepCustom.value;
  return STEP_NAMES[brewingStep.value] || '';
});

// ─── Recipe State ───────────────────────────────────────────
export const recipeName = signal('');
export const recipeStep = signal(0);
export const recipeTotalSteps = signal(0);
export const timerLeft = signal(0);  // seconds
export const recipeState = signal('idle');  // idle, running, paused, waiting_temp, waiting_timer, waiting_confirm, completed
// Raw DSL of the currently-loaded recipe (set on `req:recipe:load`, on
// start from the RecipeSheet, and lazily re-fetched after recovery resume).
// The parser in utils/parseRecipe.js turns this into the structured list
// the RecipeTimeline renders.
export const loadedRecipeContent = signal('');

// WAIT_CONFIRM — driven by telemetry (wc/cm) and evt:recipe:confirm.
export const waitingForConfirm = signal(false);
export const confirmMessage = signal('');

// ─── Temperature History (for chart) ────────────────────────
const MAX_HISTORY = 300;  // 5 minutes at 1Hz
// Throttle interval — the chart only needs one sample per second to
// show a useful trend, and the firmware's real BLE link runs at 1Hz
// anyway. The sim bridge with the default `--scale 60` pushes
// telemetry at ~60Hz; without this gate the chart's polyline rebuild
// + SVG re-render saturates the JS thread on RN and starves the
// native gesture queue, freezing taps and scroll.
const HISTORY_MIN_INTERVAL_MS = 1000;
let _lastHistoryPushMs = 0;

export const tempHistory   = signal<number[]>([]);
export const targetHistory = signal<number[]>([]);
export const outputHistory = signal<number[]>([]);
export const timeLabels    = signal<string[]>([]);

export function addTelemetryPoint(temp: number, target: number, output: number): void {
  const nowMs = Date.now();
  if (nowMs - _lastHistoryPushMs < HISTORY_MIN_INTERVAL_MS) return;
  _lastHistoryPushMs = nowMs;

  const label = new Date(nowMs).toLocaleTimeString('pt-BR', { hour: '2-digit', minute: '2-digit', second: '2-digit' });
  tempHistory.value = [...tempHistory.value.slice(-(MAX_HISTORY - 1)), temp];
  targetHistory.value = [...targetHistory.value.slice(-(MAX_HISTORY - 1)), target];
  outputHistory.value = [...outputHistory.value.slice(-(MAX_HISTORY - 1)), output];
  timeLabels.value = [...timeLabels.value.slice(-(MAX_HISTORY - 1)), label];
}

// ─── Settings ───────────────────────────────────────────────
export const pidKp = signal(20.0);
export const pidKi = signal(0.01);
export const pidKd = signal(2000.0);

// ─── UI State ───────────────────────────────────────────────
export const activeTab = signal('/');
export const toastMessage = signal('');
export const toastType    = signal<'info' | 'success' | 'error' | 'warning'>('info');

// Device picker (RN real BLE). The Disconnected card flips this on
// when `BleClient.needsPicker()` says yes; the DevicePickerSheet
// reads it to decide whether to render itself + start scanning.
export const devicePickerOpen = signal<boolean>(false);

// RSSI of the active link, in dBm (negative, closer to 0 = stronger).
// Populated by the native BLE adapter via readRSSI() polling. Null
// when disconnected, on web (browser doesn't expose RSSI), or sim.
export const signalRssi = signal<number | null>(null);

// Debug ring buffer — last N messages received from the device.
// Populated by ConnectionManager._handleMessage. UI debug panel
// reads this to display a live trace for troubleshooting.
export interface DebugEntry {
    ts: number;          // Date.now()
    tp: string;          // message type
    snippet: string;     // truncated JSON
}
const DEBUG_BUFFER_MAX = 50;
export const debugMessages = signal<DebugEntry[]>([]);
export function pushDebug(data: any): void {
    const entry: DebugEntry = {
        ts: Date.now(),
        tp: data?.tp ?? '(no tp)',
        snippet: JSON.stringify(data).slice(0, 200),
    };
    const next = debugMessages.value.concat(entry);
    debugMessages.value = next.length > DEBUG_BUFFER_MAX
        ? next.slice(-DEBUG_BUFFER_MAX)
        : next;
}

export type ToastKind = 'info' | 'success' | 'error' | 'warning';
export function showToast(message: string, type: ToastKind = 'info', durationMs = 3000): void {
  toastMessage.value = message;
  toastType.value = type;
  setTimeout(() => { toastMessage.value = ''; }, durationMs);
}

// ─── Computed ───────────────────────────────────────────────
export const isRecipeRunning = computed(() => 
  mode.value === 'recipe' && recipeState.value !== 'idle' && recipeState.value !== 'completed'
);

export const modeLabel = computed(() => {
  switch (mode.value) {
    case 'manual': return 'Manual';
    case 'recipe': return 'Receita';
    case 'tuning': return 'Tuning';
    default: return 'Inativo';
  }
});

export const formattedTimer = computed(() => {
  const secs = timerLeft.value;
  if (secs <= 0) return '--:--';
  const m = Math.floor(secs / 60);
  const s = secs % 60;
  return `${String(m).padStart(2, '0')}:${String(s).padStart(2, '0')}`;
});

// Format timer remaining as HH:MM:SS or MM:SS
export const formattedTimerRemaining = computed(() => {
  const secs = timerRemaining.value;
  if (secs <= 0) return '00:00';
  const h = Math.floor(secs / 3600);
  const m = Math.floor((secs % 3600) / 60);
  const s = secs % 60;
  if (h > 0) {
    return `${h}:${String(m).padStart(2, '0')}:${String(s).padStart(2, '0')}`;
  }
  return `${String(m).padStart(2, '0')}:${String(s).padStart(2, '0')}`;
});

// Format RTC time from timestamp
export const formattedRtcTime = computed(() => {
  if (!rtcAvailable.value || rtcTimestamp.value === 0) return '--:--';
  const d = new Date(rtcTimestamp.value * 1000);
  return d.toLocaleTimeString('pt-BR', { hour: '2-digit', minute: '2-digit' });
});

// Format scheduler target time
export const formattedSchedulerTarget = computed(() => {
  if (!schedulerActive.value) return '--:--';
  return `${String(schedulerTargetHour.value).padStart(2, '0')}:${String(schedulerTargetMinute.value).padStart(2, '0')}`;
});

// ─── Update from telemetry event ────────────────────────────
// `data` is a loose JSON object from the BLE/sim stream. We don't shape
// it strictly because new fields are added on the firmware side and
// each one is read defensively with `!== undefined`.
//
// Throttle: the sim bridge with default `--scale 60` pushes ~60 frames
// per wall second. Every frame mutates a dozen signals, which forces
// TopBar / TempInstrument / ContextPanel to re-render at 60Hz. On a
// phone that's enough Tamagui reconciliation to starve the native
// gesture queue (taps stop firing). 5Hz is well above what a human
// eye perceives for temperature swings, so we drop intermediate
// frames at the boundary. Recipe state transitions are uncommon
// enough that the worst-case delay is invisible.
const TELEMETRY_MIN_INTERVAL_MS = 200;  // 5Hz cap
let _lastTelemetryProcessedMs = 0;

export function updateFromTelemetry(data: Record<string, any>): void {
  const nowMs = Date.now();
  if (nowMs - _lastTelemetryProcessedMs < TELEMETRY_MIN_INTERVAL_MS) return;
  _lastTelemetryProcessedMs = nowMs;

  // Clear the local manual-intent flag once firmware confirms any
  // mode that isn't idle — at that point the real `mode` signal is
  // the source of truth and the intent flag is no longer needed.
  if (data.m !== undefined && data.m !== 'idle') manualIntent.value = false;

  if (data.ct !== undefined) currentTemp.value = data.ct;
  if (data.tt !== undefined) targetTemp.value = data.tt;
  if (data.out !== undefined) pidOutput.value = data.out;
  if (data.h !== undefined) heaterOn.value = data.h;
  if (data.p !== undefined) pumpOn.value = data.p;
  if (data.m !== undefined) mode.value = data.m;
  if (data.up !== undefined) uptime.value = data.up;
  if (data.rs !== undefined) recipeStep.value = data.rs;
  if (data.rt !== undefined) recipeTotalSteps.value = data.rt;
  if (data.rn !== undefined) recipeName.value = data.rn;
  if (data.tl !== undefined) timerLeft.value = data.tl;
  if (data.sok !== undefined) tempSensorOk.value = data.sok;
  if (data.saf !== undefined) safetyShutoff.value = data.saf;
  if (data.rst !== undefined) recipeState.value = data.rst;
  
  // Ramp state
  if (data.ra !== undefined) rampActive.value = data.ra;
  if (data.rr !== undefined) rampRate.value = data.rr;
  if (data.rtg !== undefined) rampTarget.value = data.rtg;
  if (data.rc !== undefined) rampCurrent.value = data.rc;
  
  // Brew Log state
  if (data.bla !== undefined) brewLogActive.value = data.bla;
  if (data.ble !== undefined) brewLogEntries.value = data.ble;
  
  // Boil Timer state
  if (data.ba !== undefined) boilActive.value = data.ba;
  if (data.bt !== undefined) boilTotal.value = data.bt;
  if (data.br !== undefined) boilRemaining.value = data.br;
  
  // Mash-Out state
  if (data.moe !== undefined) mashOutEnabled.value = data.moe;
  if (data.mot !== undefined) mashOutTemp.value = data.mot;

  // RTC state
  if (data.rtca !== undefined) rtcAvailable.value = data.rtca;
  if (data.rtct !== undefined) rtcTimestamp.value = data.rtct;
  if (data.rtcn !== undefined) rtcNtpSynced.value = data.rtcn;

  // Timer state
  // P1 — firmware now sends tmrP/tmrT to avoid collision with the message
  // type ("tp") and target-temp ("tt") keys when the timer is active.
  if (data.ta !== undefined) timerActive.value = data.ta;
  if (data.tmrP !== undefined) timerPaused.value = data.tmrP;
  if (data.tmrT !== undefined) timerTotal.value = data.tmrT;
  if (data.tr !== undefined) timerRemaining.value = data.tr;
  if (data.tm !== undefined) timerMode.value = data.tm;
  if (data.tah !== undefined) timerAlarmHour.value = data.tah;
  if (data.tam !== undefined) timerAlarmMinute.value = data.tam;

  // Scheduler state
  if (data.sa !== undefined) schedulerActive.value = data.sa;
  if (data.sth !== undefined) schedulerTargetHour.value = data.sth;
  if (data.stm !== undefined) schedulerTargetMinute.value = data.stm;
  if (data.stt !== undefined) schedulerTargetTemp.value = data.stt;
  if (data.sv !== undefined) schedulerVolume.value = data.sv;
  if (data.ss !== undefined) schedulerStatus.value = data.ss;

  // Auto-Tune state
  if (data.ata !== undefined) autoTuneActive.value = data.ata;
  if (data.atp !== undefined) autoTuneProgress.value = data.atp;

  // Brewing step
  if (data.bs !== undefined) brewingStep.value = data.bs;
  if (data.bsc !== undefined) brewingStepCustom.value = data.bsc;

  // WAIT_CONFIRM — wc is emitted on every telemetry tick so the modal
  // clears automatically when the recipe advances.
  if (data.wc !== undefined) {
    waitingForConfirm.value = data.wc;
    if (!data.wc) confirmMessage.value = '';
  }
  if (data.cm !== undefined) confirmMessage.value = data.cm;

  // Thermal Watchdog telemetry (001-thermal-watchdog). Nested object
  // `wd:{a,t,c,lc,lu,hs,ar}` — `lc/lu` may be absent when never tripped.
  if (data.wd !== undefined) {
    watchdogSupported.value = true;
    const wd = data.wd;
    if (wd.a  !== undefined) watchdogArmed.value        = wd.a;
    if (wd.t  !== undefined) watchdogTripped.value      = wd.t;
    if (wd.c  !== undefined) watchdogTripCount.value    = wd.c;
    if (wd.lc !== undefined) watchdogLastCause.value    = wd.lc;
    if (wd.lu !== undefined) watchdogLastTripUnix.value = wd.lu;
    if (wd.hs !== undefined) watchdogHardStopC.value    = wd.hs;
    if (wd.ar !== undefined) watchdogAutoReset.value    = wd.ar;
    if (wd.sfm !== undefined) watchdogSensorFaultMs.value  = wd.sfm;
    if (wd.lsm !== undefined) watchdogLoopStuckMs.value    = wd.lsm;
    if (wd.gf  !== undefined) watchdogGradFactor.value     = wd.gf;
    if (wd.gw  !== undefined) watchdogGradWindow.value     = wd.gw;
    if (wd.sa  !== undefined) watchdogSafeAutoresetC.value = wd.sa;
    if (wd.cm  !== undefined) watchdogCoolMinMs.value      = wd.cm;
  }

  // Ambient + lid + LossTune in-flight (LossTune nested object only
  // emitted while a tune is active; status/result come on dedicated
  // events handled in ConnectionManager).
  if (data.ambEff      !== undefined) ambientEffectiveC.value = data.ambEff;
  if (data.ambSrc      !== undefined) ambientSource.value     = data.ambSrc;
  if (data.ambSensorOk !== undefined) ambientSensorOk.value   = data.ambSensorOk;
  if (data.lidState    !== undefined) lidState.value          = data.lidState;
  if (data.lt !== undefined) {
    const lt = data.lt;
    if (lt.ph   !== undefined) lossTunePhase.value         = lt.ph;
    if (lt.pct  !== undefined) lossTuneProgress.value      = lt.pct;
    if (lt.mode !== undefined) lossTuneTargetMode.value    = lt.mode;
    if (lt.r2   !== undefined) lossTuneR2.value            = lt.r2;
    if (lt.h    !== undefined) lossTuneFittedCoeff.value   = lt.h;
    if (lt.n    !== undefined) lossTuneSampleCount.value   = lt.n;
    if (lt.err  !== undefined) lossTuneError.value         = lt.err;
    lossTuneActive.value = lt.ph !== 'IDLE' && lt.ph !== 'RESULT' && lt.ph !== 'ERROR';
  }

  // Add to history
  addTelemetryPoint(
    data.ct ?? currentTemp.value,
    data.tt ?? targetTemp.value,
    data.out ?? pidOutput.value
  );
}
