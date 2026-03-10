import { TemperatureChart } from '../components/TemperatureChart';
import {
  currentTemp, targetTemp, pidOutput, heaterOn, pumpOn,
  mode, modeLabel, isRecipeRunning, recipeName, recipeStep,
  recipeTotalSteps, recipeState, formattedTimer, isConnected,
} from '../stores/state';

export function Dashboard() {
  const connected = isConnected.value;

  return (
    <div class="flex flex-col gap-4">
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
