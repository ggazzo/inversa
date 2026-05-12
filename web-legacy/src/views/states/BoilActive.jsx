// BoilActive.jsx — context panel during a BOIL step.
// Big countdown + HopTimeline + pause/stop. The overlay for hop alerts
// lives at the App root (HopAlertOverlay) so it works regardless of
// which sub-state is active.

import {
    recipeName, brewingStepName, boilRemaining, recipeState,
} from '../../stores/state';
import { HopTimeline } from '../../components/HopTimeline';
import { ConnectionManager } from '../../services/ConnectionManager';

function pad(n) { return String(n).padStart(2, '0'); }
function fmt(secs) {
    if (!secs || secs < 0) return '--:--';
    const h = Math.floor(secs / 3600);
    const m = Math.floor((secs % 3600) / 60);
    const s = secs % 60;
    return h > 0 ? `${h}:${pad(m)}:${pad(s)}` : `${pad(m)}:${pad(s)}`;
}

export function BoilActive() {
    const paused = recipeState.value === 'paused';

    return (
        <div class="card bg-base-100 shadow-sm">
            <div class="card-body p-4 sm:p-6 gap-3">
                <div class="flex items-center justify-between text-xs uppercase tracking-wider text-base-content/50">
                    <span>{recipeName.value || 'Fervura'} · {brewingStepName.value || 'Fervura'}</span>
                    <span class={`badge badge-sm ${paused ? 'badge-warning' : 'badge-error'}`}>
                        {paused ? 'pausada' : 'ferver'}
                    </span>
                </div>

                {/* Big countdown */}
                <div class="text-center py-2">
                    <div class="text-xs uppercase tracking-wider text-base-content/50">
                        Restante
                    </div>
                    <div class="text-7xl font-mono tabular-nums font-bold">
                        {fmt(boilRemaining.value)}
                    </div>
                </div>

                <HopTimeline />

                <div class="grid grid-cols-2 gap-2 mt-2">
                    {paused ? (
                        <button class="btn btn-success"
                                onClick={() => ConnectionManager.resumeRecipe().catch(() => {})}>
                            Retomar
                        </button>
                    ) : (
                        <button class="btn btn-ghost"
                                onClick={() => ConnectionManager.pauseRecipe().catch(() => {})}>
                            Pausar
                        </button>
                    )}
                    <button class="btn btn-error btn-outline"
                            onClick={() => { if (confirm('Parar receita?')) ConnectionManager.stopRecipe(); }}>
                        Parar
                    </button>
                </div>
            </div>
        </div>
    );
}
