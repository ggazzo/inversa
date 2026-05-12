// Manual.jsx — direct control of the controller without a recipe.
// Setpoint via slider + numeric input, heater + pump toggles, and a
// big "Desligar tudo" emergency-off button. The TempInstrument on the
// left already shows the live readings, so this panel focuses on
// inputs only.

import { useSignal } from '@preact/signals';
import {
    targetTemp, heaterOn, pumpOn, mode, showToast,
} from '../../stores/state';
import { ConnectionManager } from '../../services/ConnectionManager';

export function Manual() {
    // Local mirror so the slider feels responsive without round-tripping
    // every move. Confirmed by the user with "Aplicar".
    const draft = useSignal(targetTemp.value || 65);

    function applyTemp() {
        const v = Math.max(0, Math.min(110, Math.round(draft.value)));
        ConnectionManager.setTemp(v)
            .then(() => showToast(`Alvo: ${v}°C`, 'success'))
            .catch((e) => showToast(e?.message || 'Falha', 'error'));
    }

    function emergencyOff() {
        ConnectionManager.heaterOff()
            .then(() => ConnectionManager.pumpOff().catch(() => {}))
            .then(() => { mode.value = 'idle'; showToast('Desligado'); })
            .catch((e) => showToast(e?.message || 'Falha', 'error'));
    }

    return (
        <div class="card bg-base-100 shadow-sm">
            <div class="card-body p-4 sm:p-6 gap-3">
                <div class="flex items-center justify-between">
                    <h2 class="font-semibold">Modo Manual</h2>
                    <button class="btn btn-ghost btn-xs"
                            onClick={() => { mode.value = 'idle'; }}>
                        Sair
                    </button>
                </div>

                {/* Setpoint controls */}
                <div class="flex items-baseline gap-2">
                    <span class="text-xs uppercase text-base-content/50">Alvo</span>
                    <span class="text-3xl font-mono tabular-nums">{Math.round(draft.value)}</span>
                    <span class="text-base-content/60">°C</span>
                </div>
                <input
                    type="range" min="0" max="110" step="1"
                    value={draft.value}
                    onInput={(e) => { draft.value = parseInt(e.currentTarget.value, 10); }}
                    class="range range-primary"
                />
                <div class="flex justify-between text-xs text-base-content/40 px-1">
                    <span>0</span><span>30</span><span>65</span><span>100</span><span>110</span>
                </div>
                <button class="btn btn-primary"
                        onClick={applyTemp}
                        disabled={Math.round(draft.value) === Math.round(targetTemp.value)}>
                    Aplicar alvo
                </button>

                {/* Actuator toggles */}
                <div class="grid grid-cols-2 gap-2 mt-2">
                    <button
                        class={`btn ${heaterOn.value ? 'btn-warning' : 'btn-outline'}`}
                        onClick={() => (heaterOn.value
                            ? ConnectionManager.heaterOff()
                            : ConnectionManager.heaterOn()).catch(() => {})
                        }
                    >
                        Aquecedor {heaterOn.value ? 'ON' : 'OFF'}
                    </button>
                    <button
                        class={`btn ${pumpOn.value ? 'btn-info' : 'btn-outline'}`}
                        onClick={() => (pumpOn.value
                            ? ConnectionManager.pumpOff()
                            : ConnectionManager.pumpOn()).catch(() => {})
                        }
                    >
                        Bomba {pumpOn.value ? 'ON' : 'OFF'}
                    </button>
                </div>

                <button class="btn btn-error mt-2" onClick={emergencyOff}>
                    Desligar tudo
                </button>
            </div>
        </div>
    );
}
