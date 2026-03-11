// state.js — Global application state using Preact Signals
import { signal, computed } from '@preact/signals';

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
export const boilAdditions = signal([]);  // List of additions
export const boilAlerts = signal([]);     // Triggered alerts

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

// ─── Recipe State ───────────────────────────────────────────
export const recipeName = signal('');
export const recipeStep = signal(0);
export const recipeTotalSteps = signal(0);
export const timerLeft = signal(0);  // seconds
export const recipeState = signal('idle');  // idle, running, paused, waiting_temp, waiting_timer, waiting_confirm, completed

// ─── Temperature History (for chart) ────────────────────────
const MAX_HISTORY = 300;  // 5 minutes at 1Hz
export const tempHistory = signal([]);
export const targetHistory = signal([]);
export const outputHistory = signal([]);
export const timeLabels = signal([]);

export function addTelemetryPoint(temp, target, output) {
  const now = new Date();
  const label = now.toLocaleTimeString('pt-BR', { hour: '2-digit', minute: '2-digit', second: '2-digit' });
  
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
export const toastType = signal('info');  // info, success, error

export function showToast(message, type = 'info', durationMs = 3000) {
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
export function updateFromTelemetry(data) {
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
  if (data.ta !== undefined) timerActive.value = data.ta;
  if (data.tp !== undefined) timerPaused.value = data.tp;
  if (data.tt !== undefined) timerTotal.value = data.tt;
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

  // Add to history
  addTelemetryPoint(
    data.ct ?? currentTemp.value,
    data.tt ?? targetTemp.value,
    data.out ?? pidOutput.value
  );
}
