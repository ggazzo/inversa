// Mash.jsx — generic "recipe is just running" view. Used when the
// recipe engine is in `running` (no WAIT_*), so the panel just shows
// progress + brewing-step label + the standard pause/stop controls.
// Specialist views (WaitTemp, WaitTimer, WaitConfirm, BoilActive,
// Paused) cover the noisier moments; this is the calm middle.

import {
    recipeName, recipeStep, recipeTotalSteps, brewingStepName, showToast,
} from '../../stores/state';
import { ConnectionManager } from '../../services/ConnectionManager';

export function Mash() {
    const total = recipeTotalSteps.value;
    const step  = recipeStep.value;
    const pct   = total > 0 ? Math.min(100, (step / total) * 100) : 0;

    return (
        <div class="card bg-base-100 shadow-sm">
            <div class="card-body p-4 sm:p-6 gap-3">
                <div class="flex items-center justify-between">
                    <div>
                        <div class="text-xs uppercase tracking-wider text-base-content/50">Receita</div>
                        <div class="font-semibold">{recipeName.value || '—'}</div>
                    </div>
                    <span class="badge badge-primary">Executando</span>
                </div>

                <div>
                    <div class="flex justify-between text-sm">
                        <span class="text-base-content/70">Passo</span>
                        <span class="font-mono">
                            <span class="font-bold">{step}</span>
                            <span class="text-base-content/50"> / {total || '?'}</span>
                        </span>
                    </div>
                    <progress class="progress progress-primary w-full mt-1"
                              value={pct} max="100"/>
                </div>

                {brewingStepName.value && (
                    <div class="text-center text-3xl font-semibold py-3">
                        {brewingStepName.value}
                    </div>
                )}

                <div class="grid grid-cols-2 gap-2 mt-1">
                    <button class="btn btn-ghost btn-sm"
                            onClick={() => ConnectionManager.pauseRecipe().catch(() => {})}>
                        Pausar
                    </button>
                    <button class="btn btn-error btn-outline btn-sm"
                            onClick={() => {
                                if (confirm('Parar receita?')) {
                                    ConnectionManager.stopRecipe()
                                        .then(() => showToast('Receita parada'))
                                        .catch(() => {});
                                }
                            }}>
                        Parar
                    </button>
                </div>
            </div>
        </div>
    );
}
