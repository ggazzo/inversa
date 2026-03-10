import { useState } from 'preact/hooks';
import { TemperatureChart } from '../components/TemperatureChart';
import { ConnectionManager } from '../services/ConnectionManager';
import {
  currentTemp, targetTemp, pidOutput, heaterOn, pumpOn,
  mode, modeLabel, isRecipeRunning, recipeName, recipeStep,
  recipeTotalSteps, recipeState, formattedTimer, isConnected,
  tempSensorOk, safetyShutoff, hasRecovery, recoveryRecipeName, showToast,
} from '../stores/state';

export function Dashboard() {
  const connected = isConnected.value;
  const sensorFailed = connected && !tempSensorOk.value;
  const safetyActive = connected && safetyShutoff.value;
  const showRecovery = connected && hasRecovery.value;
  const [recovering, setRecovering] = useState(false);

  async function handleResumeRecovery() {
    setRecovering(true);
    try {
      await ConnectionManager.resumeRecovery();
      hasRecovery.value = false;
      showToast('Receita retomada!', 'success');
    } catch (e) {
      showToast(e.message, 'error');
    } finally {
      setRecovering(false);
    }
  }

  async function handleDiscardRecovery() {
    setRecovering(true);
    try {
      await ConnectionManager.discardRecovery();
      hasRecovery.value = false;
      recoveryRecipeName.value = '';
      showToast('Recuperacao descartada', 'info');
    } catch (e) {
      showToast(e.message, 'error');
    } finally {
      setRecovering(false);
    }
  }

  return (
    <div class="flex flex-col gap-4">
      {/* Safety Alerts */}
      {safetyActive && (
        <div class="alert alert-error shadow-md">
          <svg xmlns="http://www.w3.org/2000/svg" class="stroke-current shrink-0 h-6 w-6" fill="none" viewBox="0 0 24 24">
            <path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M12 9v2m0 4h.01m-6.938 4h13.856c1.54 0 2.502-1.667 1.732-3L13.732 4c-.77-1.333-2.694-1.333-3.464 0L3.34 16c-.77 1.333.192 3 1.732 3z" />
          </svg>
          <div>
            <h3 class="font-bold">Desligamento de Seguranca</h3>
            <p class="text-sm">O aquecedor foi desligado por protecao. Verifique o sensor e a temperatura.</p>
          </div>
        </div>
      )}

      {sensorFailed && !safetyActive && (
        <div class="alert alert-warning shadow-md">
          <svg xmlns="http://www.w3.org/2000/svg" class="stroke-current shrink-0 h-6 w-6" fill="none" viewBox="0 0 24 24">
            <path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M12 9v2m0 4h.01m-6.938 4h13.856c1.54 0 2.502-1.667 1.732-3L13.732 4c-.77-1.333-2.694-1.333-3.464 0L3.34 16c-.77 1.333.192 3 1.732 3z" />
          </svg>
          <div>
            <h3 class="font-bold">Falha no Sensor</h3>
            <p class="text-sm">O sensor de temperatura nao esta respondendo corretamente.</p>
          </div>
        </div>
      )}

      {/* Recovery Dialog */}
      {showRecovery && (
        <div class="alert alert-info shadow-md">
          <svg xmlns="http://www.w3.org/2000/svg" class="stroke-current shrink-0 h-6 w-6" fill="none" viewBox="0 0 24 24">
            <path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M4 4v5h.582m15.356 2A8.001 8.001 0 004.582 9m0 0H9m11 11v-5h-.581m0 0a8.003 8.003 0 01-15.357-2m15.357 2H15" />
          </svg>
          <div class="flex-1">
            <h3 class="font-bold">Receita Interrompida</h3>
            <p class="text-sm">A receita "{recoveryRecipeName.value}" pode ser retomada.</p>
          </div>
          <div class="flex gap-2">
            <button 
              class="btn btn-sm btn-ghost" 
              onClick={handleDiscardRecovery}
              disabled={recovering}
            >
              Descartar
            </button>
            <button 
              class="btn btn-sm btn-primary" 
              onClick={handleResumeRecovery}
              disabled={recovering}
            >
              {recovering ? <span class="loading loading-spinner loading-xs" /> : 'Retomar'}
            </button>
          </div>
        </div>
      )}

      {/* Temperature Display */}
      <div class="card bg-base-100 shadow-md">
        <div class="card-body p-4">
          <div class="flex items-center justify-between">
            <div>
              <div class="text-xs uppercase text-base-content/50 font-medium">Temperatura</div>
              <div class="text-4xl font-bold text-warning">
                {connected ? `${currentTemp.value.toFixed(1)}` : '--.-'}
                <span class="text-lg text-base-content/40 ml-1">°C</span>
              </div>
            </div>
            <div class="text-right">
              <div class="text-xs uppercase text-base-content/50 font-medium">Alvo</div>
              <div class="text-2xl font-semibold text-error">
                {connected && targetTemp.value > 0 ? `${targetTemp.value.toFixed(1)}` : '--.-'}
                <span class="text-sm text-base-content/40 ml-1">°C</span>
              </div>
            </div>
          </div>

          {/* PID Output bar */}
          {connected && (
            <div class="mt-2">
              <div class="flex justify-between text-xs text-base-content/50 mb-1">
                <span>PID</span>
                <span>{pidOutput.value.toFixed(0)}%</span>
              </div>
              <progress
                class="progress progress-info w-full"
                value={pidOutput.value}
                max="100"
              />
            </div>
          )}
        </div>
      </div>

      {/* Chart */}
      <div class="card bg-base-100 shadow-md">
        <div class="card-body p-4">
          <TemperatureChart />
        </div>
      </div>

      {/* Status Cards */}
      <div class="grid grid-cols-3 gap-2">
        <StatusCard
          label="Modo"
          value={connected ? modeLabel.value : '--'}
          color={mode.value !== 'idle' ? 'text-primary' : ''}
        />
        <StatusCard
          label="Aquecedor"
          value={connected ? (heaterOn.value ? 'ON' : 'OFF') : '--'}
          color={heaterOn.value ? 'text-error' : ''}
        />
        <StatusCard
          label="Bomba"
          value={connected ? (pumpOn.value ? 'ON' : 'OFF') : '--'}
          color={pumpOn.value ? 'text-info' : ''}
        />
      </div>

      {/* Recipe Status (only when running) */}
      {connected && isRecipeRunning.value && (
        <div class="card bg-base-100 shadow-md">
          <div class="card-body p-4">
            <div class="text-xs uppercase text-base-content/50 font-medium mb-2">Receita</div>
            <div class="flex items-center justify-between">
              <div>
                <div class="font-semibold">{recipeName.value || 'Sem nome'}</div>
                <div class="text-sm text-base-content/60">
                  Passo {recipeStep.value}/{recipeTotalSteps.value}
                </div>
              </div>
              <div class="text-right">
                <div class="text-xs text-base-content/50">{recipeStateLabel(recipeState.value)}</div>
                <div class="font-mono text-lg">{formattedTimer.value}</div>
              </div>
            </div>
          </div>
        </div>
      )}
    </div>
  );
}

function StatusCard({ label, value, color = '' }) {
  return (
    <div class="card bg-base-100 shadow-sm">
      <div class="card-body p-3 items-center text-center">
        <div class="text-xs uppercase text-base-content/50">{label}</div>
        <div class={`text-sm font-bold ${color}`}>{value}</div>
      </div>
    </div>
  );
}

function recipeStateLabel(state) {
  switch (state) {
    case 'running': return 'Executando';
    case 'paused': return 'Pausado';
    case 'waiting_temp': return 'Aguardando temp.';
    case 'waiting_timer': return 'Temporizador';
    case 'waiting_confirm': return 'Confirmacao';
    case 'completed': return 'Concluido';
    default: return 'Inativo';
  }
}
