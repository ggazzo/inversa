import { useState } from 'preact/hooks';
import { ConnectionManager } from '../services/ConnectionManager';
import {
  isConnected, currentTemp, targetTemp, heaterOn, pumpOn,
  mode, showToast,
  timerActive, timerPaused, timerRemaining, timerMode,
  timerAlarmHour, timerAlarmMinute, formattedTimerRemaining,
  schedulerActive, schedulerTargetHour, schedulerTargetMinute,
  schedulerTargetTemp, schedulerVolume, schedulerStatus,
  rtcAvailable, formattedRtcTime,
} from '../stores/state';

export function Control() {
  const connected = isConnected.value;
  const [setpoint, setSetpoint] = useState('');
  const [sending, setSending] = useState(false);

  async function handleSetTemp() {
    const val = parseFloat(setpoint);
    if (isNaN(val) || val < 0 || val > 110) {
      showToast('Valor invalido (0-110°C)', 'error');
      return;
    }
    setSending(true);
    try {
      await ConnectionManager.setTemp(val);
      showToast(`Alvo: ${val}°C`, 'success');
      setSetpoint('');
    } catch (e) {
      showToast(e.message, 'error');
    } finally {
      setSending(false);
    }
  }

  async function toggleHeater() {
    try {
      if (heaterOn.value) {
        await ConnectionManager.heaterOff();
      } else {
        await ConnectionManager.heaterOn();
      }
    } catch (e) {
      showToast(e.message, 'error');
    }
  }

  async function togglePump() {
    try {
      if (pumpOn.value) {
        await ConnectionManager.pumpOff();
      } else {
        await ConnectionManager.pumpOn();
      }
    } catch (e) {
      showToast(e.message, 'error');
    }
  }

  if (!connected) {
    return (
      <div class="flex flex-col items-center justify-center py-16 text-base-content/50">
        <svg xmlns="http://www.w3.org/2000/svg" class="h-12 w-12 mb-4" fill="none" viewBox="0 0 24 24" stroke="currentColor">
          <path stroke-linecap="round" stroke-linejoin="round" stroke-width="1.5" d="M18.364 5.636a9 9 0 11-12.728 0M12 3v9" />
        </svg>
        <p>Conecte ao dispositivo para controlar.</p>
      </div>
    );
  }

  return (
    <div class="flex flex-col gap-4">
      {/* Set Temperature */}
      <div class="card bg-base-100 shadow-md">
        <div class="card-body p-4">
          <h3 class="text-sm font-semibold uppercase text-base-content/60 mb-3">Temperatura Alvo</h3>
          <div class="flex items-center gap-2">
            <div class="flex items-center gap-1 flex-1">
              <input
                type="number"
                class="input input-bordered input-sm w-full"
                placeholder={`Atual: ${targetTemp.value.toFixed(1)}°C`}
                value={setpoint}
                onInput={(e) => setSetpoint(e.target.value)}
                onKeyDown={(e) => e.key === 'Enter' && handleSetTemp()}
                min="0"
                max="110"
                step="0.5"
              />
              <span class="text-base-content/40 text-sm">°C</span>
            </div>
            <button
              class="btn btn-primary btn-sm"
              onClick={handleSetTemp}
              disabled={sending || !setpoint}
            >
              {sending ? <span class="loading loading-spinner loading-xs" /> : 'Definir'}
            </button>
          </div>

          {/* Quick temperature buttons */}
          <div class="flex flex-wrap gap-2 mt-3">
            {[50, 60, 65, 68, 72, 78, 100].map((t) => (
              <button
                key={t}
                class={`btn btn-xs ${targetTemp.value === t ? 'btn-warning' : 'btn-ghost'}`}
                onClick={() => {
                  setSetpoint(String(t));
                }}
              >
                {t}°C
              </button>
            ))}
          </div>
        </div>
      </div>

      {/* Actuator Toggles */}
      <div class="card bg-base-100 shadow-md">
        <div class="card-body p-4">
          <h3 class="text-sm font-semibold uppercase text-base-content/60 mb-3">Atuadores</h3>

          {mode.value === 'recipe' && (
            <div class="alert alert-warning text-xs mb-3">
              Controle manual desabilitado durante receita.
            </div>
          )}

          <div class="flex gap-3">
            {/* Heater */}
            <button
              class={`btn flex-1 ${heaterOn.value ? 'btn-error' : 'btn-outline'}`}
              onClick={toggleHeater}
              disabled={mode.value === 'recipe'}
            >
              <svg xmlns="http://www.w3.org/2000/svg" class="h-5 w-5" fill="none" viewBox="0 0 24 24" stroke="currentColor">
                <path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M17.657 18.657A8 8 0 016.343 7.343S7 9 9 10c0-2 .5-5 2.986-7C14 5 16.09 5.777 17.656 7.343A7.975 7.975 0 0120 13a7.975 7.975 0 01-2.343 5.657z" />
                <path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M9.879 16.121A3 3 0 1012.015 11L11 14H9c0 .768.293 1.536.879 2.121z" />
              </svg>
              <span>Aquecedor {heaterOn.value ? 'ON' : 'OFF'}</span>
            </button>

            {/* Pump */}
            <button
              class={`btn flex-1 ${pumpOn.value ? 'btn-info' : 'btn-outline'}`}
              onClick={togglePump}
              disabled={mode.value === 'recipe'}
            >
              <svg xmlns="http://www.w3.org/2000/svg" class="h-5 w-5" fill="none" viewBox="0 0 24 24" stroke="currentColor">
                <path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M4 4v5h.582m15.356 2A8.001 8.001 0 004.582 9m0 0H9m11 11v-5h-.581m0 0a8.003 8.003 0 01-15.357-2m15.357 2H15" />
              </svg>
              <span>Bomba {pumpOn.value ? 'ON' : 'OFF'}</span>
            </button>
          </div>
        </div>
      </div>

      {/* Current readings summary */}
      <div class="card bg-base-100 shadow-md">
        <div class="card-body p-4">
          <h3 class="text-sm font-semibold uppercase text-base-content/60 mb-2">Leituras</h3>
          <div class="grid grid-cols-2 gap-2 text-sm">
            <div class="flex justify-between">
              <span class="text-base-content/50">Temperatura:</span>
              <span class="font-mono">{currentTemp.value.toFixed(1)}°C</span>
            </div>
            <div class="flex justify-between">
              <span class="text-base-content/50">Alvo:</span>
              <span class="font-mono">{targetTemp.value.toFixed(1)}°C</span>
            </div>
            {rtcAvailable.value && (
              <div class="flex justify-between col-span-2">
                <span class="text-base-content/50">Hora:</span>
                <span class="font-mono">{formattedRtcTime.value}</span>
              </div>
            )}
          </div>
        </div>
      </div>

      {/* Timer */}
      <TimerCard />

      {/* Scheduler */}
      <SchedulerCard />
    </div>
  );
}

// ─── Timer Card Component ────────────────────────────────────
function TimerCard() {
  const [timerMinutes, setTimerMinutes] = useState(10);
  const [alarmHour, setAlarmHour] = useState(8);
  const [alarmMinute, setAlarmMinute] = useState(0);
  const [timerType, setTimerType] = useState('countdown'); // countdown | alarm
  const [loading, setLoading] = useState(false);

  async function startTimer() {
    setLoading(true);
    try {
      if (timerType === 'countdown') {
        await ConnectionManager.startTimerMinutes(timerMinutes);
        showToast(`Timer: ${timerMinutes} minutos`, 'success');
      } else {
        if (!rtcAvailable.value) {
          showToast('RTC nao disponivel', 'error');
          return;
        }
        await ConnectionManager.setTimerAlarm(alarmHour, alarmMinute);
        showToast(`Alarme: ${String(alarmHour).padStart(2, '0')}:${String(alarmMinute).padStart(2, '0')}`, 'success');
      }
    } catch (e) {
      showToast(e.message, 'error');
    } finally {
      setLoading(false);
    }
  }

  async function stopTimer() {
    try {
      await ConnectionManager.stopTimer();
      showToast('Timer cancelado', 'info');
    } catch (e) {
      showToast(e.message, 'error');
    }
  }

  async function togglePause() {
    try {
      if (timerPaused.value) {
        await ConnectionManager.resumeTimer();
      } else {
        await ConnectionManager.pauseTimer();
      }
    } catch (e) {
      showToast(e.message, 'error');
    }
  }

  return (
    <div class="card bg-base-100 shadow-md">
      <div class="card-body p-4">
        <h3 class="text-sm font-semibold uppercase text-base-content/60 mb-3">Timer</h3>

        {timerActive.value ? (
          // Active timer display
          <div>
            <div class="text-center mb-3">
              <div class="text-4xl font-mono font-bold">
                {formattedTimerRemaining.value}
              </div>
              <div class="text-xs text-base-content/50 mt-1">
                {timerMode.value === 1 
                  ? `Alarme: ${String(timerAlarmHour.value).padStart(2, '0')}:${String(timerAlarmMinute.value).padStart(2, '0')}`
                  : 'Contagem regressiva'}
              </div>
            </div>
            <div class="flex gap-2">
              <button
                class={`btn btn-sm flex-1 ${timerPaused.value ? 'btn-success' : 'btn-warning'}`}
                onClick={togglePause}
              >
                {timerPaused.value ? 'Retomar' : 'Pausar'}
              </button>
              <button class="btn btn-sm btn-error flex-1" onClick={stopTimer}>
                Parar
              </button>
            </div>
          </div>
        ) : (
          // Timer setup
          <div>
            {/* Timer type selector */}
            <div class="tabs tabs-boxed mb-3">
              <button 
                class={`tab tab-sm flex-1 ${timerType === 'countdown' ? 'tab-active' : ''}`}
                onClick={() => setTimerType('countdown')}
              >
                Contagem
              </button>
              <button 
                class={`tab tab-sm flex-1 ${timerType === 'alarm' ? 'tab-active' : ''}`}
                onClick={() => setTimerType('alarm')}
                disabled={!rtcAvailable.value}
              >
                Alarme
              </button>
            </div>

            {timerType === 'countdown' ? (
              <div class="flex items-center gap-2 mb-3">
                <input
                  type="number"
                  class="input input-bordered input-sm w-20 font-mono"
                  value={timerMinutes}
                  onInput={(e) => setTimerMinutes(parseInt(e.target.value) || 0)}
                  min="1"
                  max="999"
                />
                <span class="text-sm text-base-content/50">minutos</span>
              </div>
            ) : (
              <div class="flex items-center gap-2 mb-3">
                <input
                  type="number"
                  class="input input-bordered input-sm w-16 font-mono text-center"
                  value={alarmHour}
                  onInput={(e) => setAlarmHour(Math.min(23, Math.max(0, parseInt(e.target.value) || 0)))}
                  min="0"
                  max="23"
                />
                <span class="text-lg font-bold">:</span>
                <input
                  type="number"
                  class="input input-bordered input-sm w-16 font-mono text-center"
                  value={alarmMinute}
                  onInput={(e) => setAlarmMinute(Math.min(59, Math.max(0, parseInt(e.target.value) || 0)))}
                  min="0"
                  max="59"
                />
              </div>
            )}

            {/* Quick presets */}
            {timerType === 'countdown' && (
              <div class="flex flex-wrap gap-2 mb-3">
                {[5, 10, 15, 30, 60].map((m) => (
                  <button
                    key={m}
                    class={`btn btn-xs ${timerMinutes === m ? 'btn-primary' : 'btn-ghost'}`}
                    onClick={() => setTimerMinutes(m)}
                  >
                    {m} min
                  </button>
                ))}
              </div>
            )}

            <button
              class="btn btn-primary btn-sm w-full"
              onClick={startTimer}
              disabled={loading}
            >
              {loading ? <span class="loading loading-spinner loading-xs" /> : 'Iniciar Timer'}
            </button>
          </div>
        )}
      </div>
    </div>
  );
}

// ─── Scheduler Card Component ────────────────────────────────
function SchedulerCard() {
  const [targetHour, setTargetHour] = useState(8);
  const [targetMinute, setTargetMinute] = useState(0);
  const [targetTempInput, setTargetTempInput] = useState(65);
  const [volumeInput, setVolumeInput] = useState(20);
  const [loading, setLoading] = useState(false);

  async function startScheduler() {
    if (!rtcAvailable.value) {
      showToast('RTC nao disponivel', 'error');
      return;
    }
    setLoading(true);
    try {
      await ConnectionManager.setScheduler(targetHour, targetMinute, targetTempInput, volumeInput);
      showToast(`Agendado para ${String(targetHour).padStart(2, '0')}:${String(targetMinute).padStart(2, '0')}`, 'success');
    } catch (e) {
      showToast(e.message, 'error');
    } finally {
      setLoading(false);
    }
  }

  async function stopScheduler() {
    try {
      await ConnectionManager.stopScheduler();
      showToast('Agendamento cancelado', 'info');
    } catch (e) {
      showToast(e.message, 'error');
    }
  }

  if (!rtcAvailable.value) {
    return null; // Don't show scheduler if RTC not available
  }

  return (
    <div class="card bg-base-100 shadow-md">
      <div class="card-body p-4">
        <h3 class="text-sm font-semibold uppercase text-base-content/60 mb-3">
          Agendamento
          <span class="text-xs font-normal text-base-content/40 ml-2">"Pronto as..."</span>
        </h3>

        {schedulerActive.value ? (
          // Active scheduler display
          <div>
            <div class="text-center mb-3">
              <div class="text-2xl font-mono font-bold">
                {String(schedulerTargetHour.value).padStart(2, '0')}:{String(schedulerTargetMinute.value).padStart(2, '0')}
              </div>
              <div class="text-sm text-base-content/60 mt-1">
                {schedulerTargetTemp.value.toFixed(0)}°C - {schedulerVolume.value.toFixed(0)}L
              </div>
              <div class="badge badge-info badge-sm mt-2">
                {schedulerStatus.value}
              </div>
            </div>
            <button class="btn btn-sm btn-error w-full" onClick={stopScheduler}>
              Cancelar
            </button>
          </div>
        ) : (
          // Scheduler setup
          <div>
            <p class="text-xs text-base-content/50 mb-3">
              Calcular automaticamente quando iniciar o aquecimento para atingir a temperatura no horario desejado.
            </p>

            {/* Target time */}
            <div class="flex items-center gap-2 mb-3">
              <span class="text-sm text-base-content/50 w-16">Horario:</span>
              <input
                type="number"
                class="input input-bordered input-sm w-16 font-mono text-center"
                value={targetHour}
                onInput={(e) => setTargetHour(Math.min(23, Math.max(0, parseInt(e.target.value) || 0)))}
                min="0"
                max="23"
              />
              <span class="text-lg font-bold">:</span>
              <input
                type="number"
                class="input input-bordered input-sm w-16 font-mono text-center"
                value={targetMinute}
                onInput={(e) => setTargetMinute(Math.min(59, Math.max(0, parseInt(e.target.value) || 0)))}
                min="0"
                max="59"
              />
            </div>

            {/* Target temperature */}
            <div class="flex items-center gap-2 mb-3">
              <span class="text-sm text-base-content/50 w-16">Temp:</span>
              <input
                type="number"
                class="input input-bordered input-sm w-20 font-mono"
                value={targetTempInput}
                onInput={(e) => setTargetTempInput(parseFloat(e.target.value) || 0)}
                min="20"
                max="100"
                step="0.5"
              />
              <span class="text-sm text-base-content/50">°C</span>
            </div>

            {/* Volume */}
            <div class="flex items-center gap-2 mb-3">
              <span class="text-sm text-base-content/50 w-16">Volume:</span>
              <input
                type="number"
                class="input input-bordered input-sm w-20 font-mono"
                value={volumeInput}
                onInput={(e) => setVolumeInput(parseFloat(e.target.value) || 0)}
                min="1"
                max="100"
                step="1"
              />
              <span class="text-sm text-base-content/50">litros</span>
            </div>

            <button
              class="btn btn-primary btn-sm w-full"
              onClick={startScheduler}
              disabled={loading}
            >
              {loading ? <span class="loading loading-spinner loading-xs" /> : 'Agendar'}
            </button>
          </div>
        )}
      </div>
    </div>
  );
}
