import { useState } from 'preact/hooks';
import { ConnectionManager } from '../services/ConnectionManager';
import {
  isConnected, showToast,
  boilActive, boilTotal, boilRemaining, boilAlerts,
  currentTemp
} from '../stores/state';

export function Boil() {
  const connected = isConnected.value;
  const [boilMinutes, setBoilMinutes] = useState(60);
  const [additions, setAdditions] = useState([
    { min: 60, name: 'Amargor' },
    { min: 15, name: 'Sabor' },
    { min: 5, name: 'Aroma' },
    { min: 0, name: 'Flameout' }
  ]);
  const [newAddMin, setNewAddMin] = useState(30);
  const [newAddName, setNewAddName] = useState('');
  const [loading, setLoading] = useState(false);

  async function startBoil() {
    setLoading(true);
    try {
      await ConnectionManager.startBoil(boilMinutes, additions);
    } catch (e) {
      showToast(e.message, 'error');
    } finally {
      setLoading(false);
    }
  }

  async function stopBoil() {
    setLoading(true);
    try {
      await ConnectionManager.stopBoil();
    } catch (e) {
      showToast(e.message, 'error');
    } finally {
      setLoading(false);
    }
  }

  async function pauseBoil() {
    try {
      await ConnectionManager.pauseBoil();
    } catch (e) {
      showToast(e.message, 'error');
    }
  }

  async function resumeBoil() {
    try {
      await ConnectionManager.resumeBoil();
    } catch (e) {
      showToast(e.message, 'error');
    }
  }

  function addAddition() {
    if (!newAddName.trim()) {
      showToast('Digite o nome da adicao', 'error');
      return;
    }
    setAdditions([...additions, { min: newAddMin, name: newAddName }]
      .sort((a, b) => b.min - a.min));
    setNewAddName('');
  }

  function removeAddition(index) {
    setAdditions(additions.filter((_, i) => i !== index));
  }

  function formatTime(seconds) {
    const h = Math.floor(seconds / 3600);
    const m = Math.floor((seconds % 3600) / 60);
    const s = seconds % 60;
    if (h > 0) return `${h}:${String(m).padStart(2, '0')}:${String(s).padStart(2, '0')}`;
    return `${String(m).padStart(2, '0')}:${String(s).padStart(2, '0')}`;
  }

  if (!connected) {
    return (
      <div class="flex flex-col items-center justify-center py-16 text-base-content/50">
        <svg xmlns="http://www.w3.org/2000/svg" class="h-12 w-12 mb-4" fill="none" viewBox="0 0 24 24" stroke="currentColor">
          <path stroke-linecap="round" stroke-linejoin="round" stroke-width="1.5" d="M17.657 18.657A8 8 0 016.343 7.343S7 9 9 10c0-2 .5-5 2.986-7C14 5 16.09 5.777 17.656 7.343A7.975 7.975 0 0120 13a7.975 7.975 0 01-2.343 5.657z" />
          <path stroke-linecap="round" stroke-linejoin="round" stroke-width="1.5" d="M9.879 16.121A3 3 0 1012.015 11L11 14H9c0 .768.293 1.536.879 2.121z" />
        </svg>
        <p>Conecte ao dispositivo para usar o timer de fervura.</p>
      </div>
    );
  }

  return (
    <div class="flex flex-col gap-4">
      {/* Timer Display */}
      {boilActive.value && (
        <div class="card bg-error text-error-content shadow-lg">
          <div class="card-body items-center text-center p-6">
            <h2 class="text-lg font-bold uppercase tracking-wider opacity-80">Fervura</h2>
            <div class="text-6xl font-mono font-bold my-4">
              {formatTime(boilRemaining.value)}
            </div>
            <div class="text-sm opacity-70">
              {currentTemp.value.toFixed(1)}°C
            </div>
            <div class="flex gap-2 mt-4">
              <button class="btn btn-sm btn-outline btn-error-content" onClick={pauseBoil}>
                Pausar
              </button>
              <button class="btn btn-sm btn-outline btn-error-content" onClick={stopBoil}>
                Parar
              </button>
            </div>
          </div>
        </div>
      )}

      {/* Alerts */}
      {boilAlerts.value.length > 0 && (
        <div class="card bg-warning text-warning-content shadow-md">
          <div class="card-body p-4">
            <h3 class="font-bold">Adicoes Alertadas</h3>
            <ul class="text-sm space-y-1">
              {boilAlerts.value.map((alert, i) => (
                <li key={i} class="flex justify-between">
                  <span>{alert.name}</span>
                  <span class="opacity-70">{alert.min} min</span>
                </li>
              ))}
            </ul>
          </div>
        </div>
      )}

      {/* Setup */}
      {!boilActive.value && (
        <>
          {/* Boil Time */}
          <div class="card bg-base-100 shadow-md">
            <div class="card-body p-4">
              <h3 class="text-sm font-semibold uppercase text-base-content/60 mb-3">Tempo de Fervura</h3>
              
              <div class="flex items-center gap-2">
                <input
                  type="number"
                  class="input input-bordered input-lg w-24 text-center font-mono"
                  value={boilMinutes}
                  onInput={(e) => setBoilMinutes(parseInt(e.target.value) || 60)}
                  min="1"
                  max="180"
                />
                <span class="text-lg">minutos</span>
              </div>

              <div class="flex gap-2 mt-3">
                {[60, 75, 90].map(min => (
                  <button 
                    key={min}
                    class={`btn btn-sm ${boilMinutes === min ? 'btn-primary' : 'btn-outline'}`}
                    onClick={() => setBoilMinutes(min)}
                  >
                    {min} min
                  </button>
                ))}
              </div>
            </div>
          </div>

          {/* Additions */}
          <div class="card bg-base-100 shadow-md">
            <div class="card-body p-4">
              <h3 class="text-sm font-semibold uppercase text-base-content/60 mb-3">
                Adicoes de Lupulo
              </h3>

              {additions.length > 0 ? (
                <ul class="space-y-2 mb-4">
                  {additions.map((add, i) => (
                    <li key={i} class="flex items-center justify-between bg-base-200 rounded-lg px-3 py-2">
                      <div>
                        <span class="font-semibold">{add.name}</span>
                        <span class="text-sm text-base-content/50 ml-2">@ {add.min} min</span>
                      </div>
                      <button 
                        class="btn btn-ghost btn-xs text-error"
                        onClick={() => removeAddition(i)}
                      >
                        X
                      </button>
                    </li>
                  ))}
                </ul>
              ) : (
                <p class="text-sm text-base-content/50 mb-4">Nenhuma adicao configurada</p>
              )}

              {/* Add new */}
              <div class="flex gap-2">
                <input
                  type="number"
                  class="input input-bordered input-sm w-16 font-mono"
                  value={newAddMin}
                  onInput={(e) => setNewAddMin(parseInt(e.target.value) || 0)}
                  min="0"
                  max="180"
                  placeholder="min"
                />
                <input
                  type="text"
                  class="input input-bordered input-sm flex-1"
                  value={newAddName}
                  onInput={(e) => setNewAddName(e.target.value)}
                  placeholder="Nome (ex: Cascade 30g)"
                />
                <button class="btn btn-sm btn-outline" onClick={addAddition}>
                  +
                </button>
              </div>
            </div>
          </div>

          {/* Start Button */}
          <button 
            class="btn btn-error btn-lg w-full"
            onClick={startBoil}
            disabled={loading}
          >
            {loading ? (
              <span class="loading loading-spinner" />
            ) : (
              <>
                <svg xmlns="http://www.w3.org/2000/svg" class="h-6 w-6 mr-2" fill="none" viewBox="0 0 24 24" stroke="currentColor">
                  <path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M17.657 18.657A8 8 0 016.343 7.343S7 9 9 10c0-2 .5-5 2.986-7C14 5 16.09 5.777 17.656 7.343A7.975 7.975 0 0120 13a7.975 7.975 0 01-2.343 5.657z" />
                </svg>
                Iniciar Fervura
              </>
            )}
          </button>
        </>
      )}
    </div>
  );
}
