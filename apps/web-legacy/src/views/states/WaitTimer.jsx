// WaitTimer.jsx — a recipe step is counting down (WAIT_TIMER N min).
// The view is intentionally austere: massive countdown + step label, so
// a brewer leaving the kitchen can glance at it from the doorway.

import {
    timerLeft, recipeName, brewingStepName,
    recipeStep, recipeTotalSteps,
} from '../../stores/state';
import { ConnectionManager } from '../../services/ConnectionManager';

function pad(n) { return String(n).padStart(2, '0'); }

function fmt(secs) {
    if (!secs || secs < 0) return '--:--';
    const h = Math.floor(secs / 3600);
    const m = Math.floor((secs % 3600) / 60);
    const s = secs % 60;
    return h > 0 ? `${h}:${pad(m)}:${pad(s)}` : `${pad(m)}:${pad(s)}`;
}

export function WaitTimer() {
    const secs = timerLeft.value;
    return (
        <div class="card bg-base-100 shadow-sm">
            <div class="card-body p-4 sm:p-6 gap-3 items-center text-center">
                <div class="w-full flex items-start justify-between text-xs uppercase tracking-wider text-base-content/50">
                    <span>{recipeName.value || '—'}</span>
                    <span>Passo {recipeStep.value}/{recipeTotalSteps.value || '?'}</span>
                </div>

                <h2 class="text-2xl font-semibold">
                    {brewingStepName.value || 'Aguardando timer'}
                </h2>

                <div class="text-7xl sm:text-8xl font-mono tabular-nums font-bold tracking-tight my-3">
                    {fmt(secs)}
                </div>

                <div class="text-base-content/50 text-xs">
                    Restante até o próximo passo
                </div>

                <div class="grid grid-cols-2 gap-2 w-full mt-3">
                    <button class="btn btn-ghost btn-sm"
                            onClick={() => ConnectionManager.pauseRecipe().catch(() => {})}>
                        Pausar
                    </button>
                    <button class="btn btn-error btn-outline btn-sm"
                            onClick={() => { if (confirm('Parar receita?')) ConnectionManager.stopRecipe(); }}>
                        Parar
                    </button>
                </div>
            </div>
        </div>
    );
}
