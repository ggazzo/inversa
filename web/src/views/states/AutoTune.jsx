// AutoTune.jsx — context-panel during a Ziegler-Nichols relay tune.
// Just status + progress + cancel. The actual Kp/Ki/Kd values arrive
// after completion (telemetry mode flips back to idle and a toast
// is shown by ConnectionManager).

import { autoTuneActive, autoTuneProgress, targetTemp, showToast } from '../../stores/state';
import { ConnectionManager } from '../../services/ConnectionManager';

export function AutoTune() {
    function cancel() {
        if (!confirm('Cancelar AutoTune?')) return;
        ConnectionManager.stopAutoTune()
            .then(() => showToast('AutoTune cancelado'))
            .catch(() => {});
    }

    return (
        <div class="card bg-base-100 shadow-sm">
            <div class="card-body p-4 sm:p-6 gap-4 items-center text-center">
                <div class="text-5xl">🎯</div>
                <h2 class="text-2xl font-semibold">AutoTune em andamento</h2>
                <p class="text-sm text-base-content/70 max-w-sm">
                    O firmware está fazendo o controle bang-bang em torno do
                    alvo <span class="font-mono">{targetTemp.value.toFixed(1)}°C</span>
                    {' '}pra calcular Kp/Ki/Kd. Tipicamente leva 15-30 min
                    dependendo do volume e potência.
                </p>

                <div class="w-full">
                    <progress class="progress progress-primary w-full"
                              value={autoTuneProgress.value} max="100"/>
                    <div class="text-xs text-base-content/50 mt-1">
                        {autoTuneProgress.value}% — não interrompa
                    </div>
                </div>

                <button class="btn btn-error btn-outline" onClick={cancel}
                        disabled={!autoTuneActive.value}>
                    Cancelar
                </button>
            </div>
        </div>
    );
}
