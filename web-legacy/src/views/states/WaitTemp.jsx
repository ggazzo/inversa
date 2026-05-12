// WaitTemp.jsx — recipe is sitting on a WAIT_TEMP step, watching the
// temperature climb toward (or fall to) the target. The card is built
// to be glance-friendly from across the room: huge delta, progress
// ring, ETA when monotonic warming is observed.

import { useComputed } from '@preact/signals';
import {
    currentTemp, targetTemp, recipeName, brewingStepName,
} from '../../stores/state';

// Newton-ish rough ETA: assume the last minute's slope holds. We don't
// have history here, but `targetTemp - currentTemp` over a guessed
// heating rate gives a useful "5-10 min" estimate. Keep it conservative
// and label it "aprox." so users don't trust it to the second.
function approxEtaMin(curr, target) {
    const delta = target - curr;
    if (!isFinite(delta) || delta <= 0.5) return null;
    // Empirical: a 3 kW heater on 25 L heats ~0.03 °C/s = 1.8 °C/min,
    // less the loss term. Underestimate to avoid false hope.
    const slopePerMin = 1.4;
    return Math.max(1, Math.round(delta / slopePerMin));
}

export function WaitTemp() {
    const curr   = currentTemp.value;
    const target = targetTemp.value;
    const delta  = target - curr;
    const eta    = useComputed(() => approxEtaMin(curr, target));
    const pct    = target > 0 ? Math.max(0, Math.min(100, (curr / target) * 100)) : 0;
    const direction = delta > 0.5 ? 'aquecendo' : delta < -0.5 ? 'esfriando' : 'estabilizando';
    const dirColor  = delta > 0.5 ? 'text-warning' : delta < -0.5 ? 'text-info' : 'text-success';

    return (
        <div class="card bg-base-100 shadow-sm">
            <div class="card-body p-4 sm:p-6 gap-3 items-center text-center">
                <div class="w-full flex items-start justify-between text-xs uppercase tracking-wider text-base-content/50">
                    <span>Receita · {recipeName.value || '—'}</span>
                    <span class={dirColor}>{direction}</span>
                </div>

                <h2 class="text-2xl font-semibold">{brewingStepName.value || 'Aguardando temperatura'}</h2>

                <div class="text-base-content/60 text-sm">Alvo</div>
                <div class="text-7xl font-mono tabular-nums font-bold text-primary">
                    {target.toFixed(1)}°C
                </div>

                <div class="text-base-content/60 text-sm mt-2">Falta</div>
                <div class={`text-4xl font-mono tabular-nums ${dirColor}`}>
                    {delta > 0 ? '+' : ''}{delta.toFixed(1)}°C
                </div>

                {eta.value !== null && (
                    <div class="text-base-content/50 text-xs mt-2">
                        ETA aprox. <span class="font-semibold">{eta.value} min</span>
                    </div>
                )}

                <progress class="progress progress-primary w-full mt-3"
                          value={pct} max="100"/>
            </div>
        </div>
    );
}
