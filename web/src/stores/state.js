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

  // Add to history
  addTelemetryPoint(
    data.ct ?? currentTemp.value,
    data.tt ?? targetTemp.value,
    data.out ?? pidOutput.value
  );
}
