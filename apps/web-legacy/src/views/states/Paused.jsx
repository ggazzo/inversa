// Paused.jsx — recipe was paused (req:recipe:pause). The pause math is
// handled by RecipePlugin (P3): _timerStart shifts forward on resume so
// the WAIT_TIMER countdown does not lose seconds.

import { recipeName, recipeStep, recipeTotalSteps, brewingStepName } from '../../stores/state';
import { ConnectionManager } from '../../services/ConnectionManager';

export function Paused() {
    return (
        <div class="card bg-warning/10 border border-warning shadow-sm">
            <div class="card-body p-4 sm:p-6 gap-3 items-center text-center">
                <div class="text-5xl">⏸</div>
                <h2 class="text-2xl font-semibold">Receita pausada</h2>
                <p class="text-sm text-base-content/70">
                    <span class="font-mono">{recipeName.value || '—'}</span>
                    {brewingStepName.value && <> · {brewingStepName.value}</>}
                    {recipeTotalSteps.value > 0 && <>
                        {' · passo '}<span class="font-mono">{recipeStep.value}/{recipeTotalSteps.value}</span>
                    </>}
                </p>
                <p class="text-xs text-base-content/50">
                    O timer interno é preservado — quando você retomar, o tempo
                    pausado não é descontado da contagem.
                </p>

                <div class="grid grid-cols-2 gap-2 w-full mt-2">
                    <button class="btn btn-success"
                            onClick={() => ConnectionManager.resumeRecipe().catch(() => {})}>
                        Retomar
                    </button>
                    <button class="btn btn-error btn-outline"
                            onClick={() => { if (confirm('Parar receita?')) ConnectionManager.stopRecipe(); }}>
                        Parar
                    </button>
                </div>
            </div>
        </div>
    );
}
