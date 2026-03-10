import { useState } from 'preact/hooks';
import { ConnectionManager } from '../services/ConnectionManager';
import {
  isConnected, currentTemp, targetTemp, heaterOn, pumpOn,
  mode, showToast,
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
          </div>
        </div>
      </div>
    </div>
  );
}
