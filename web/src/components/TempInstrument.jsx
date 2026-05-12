// TempInstrument.jsx — the "left panel" of the new UI.
//
// Renders a glance-friendly temperature display + chart + a strip of
// secondary telemetry (PID%, heater, pump). Color-coded by relation
// between current and target temperature (heating / holding / cooling).

import { useComputed } from '@preact/signals';
import {
    currentTemp, targetTemp, pidOutput, heaterOn, pumpOn, mode,
} from '../stores/state';
import { isStale } from '../stores/telemetryFreshness';
import { TemperatureChart } from './TemperatureChart';

// Decide a status color from the temperature delta. Returns Tailwind /
// DaisyUI class tokens so the consumer just spreads them.
function tempStatus(current, target, mode) {
    if (mode === 'idle' || target <= 0) {
        return { color: 'text-base-content',      label: 'inativa' };
    }
    const d = current - target;
    if (d < -1.0) return { color: 'text-warning',  label: 'aquecendo' };
    if (d >  1.0) return { color: 'text-info',     label: 'esfriando' };
    return            { color: 'text-success',     label: 'no alvo'  };
}

export function TempInstrument() {
    const status = useComputed(() => tempStatus(currentTemp.value, targetTemp.value, mode.value));
    const stale  = isStale.value;
    const pidPct = Math.round((pidOutput.value / 255) * 100);

    return (
        <section class={`card bg-base-100 shadow-sm transition-opacity ${stale ? 'opacity-60' : ''}`}>
            <div class="card-body p-4 sm:p-6 gap-2">
                {/* Big temperature */}
                <div class="flex items-baseline gap-2">
                    <div
                        aria-live="polite"
                        class={`text-6xl sm:text-7xl font-mono tabular-nums leading-none ${status.value.color}`}
                    >
                        {currentTemp.value.toFixed(1)}
                    </div>
                    <div class="text-2xl text-base-content/60">°C</div>
                    {stale && (
                        <span class="badge badge-warning badge-sm ml-auto">STALE</span>
                    )}
                </div>

                {/* Target + status label */}
                <div class="flex items-center justify-between text-sm">
                    <div class="text-base-content/70">
                        {targetTemp.value > 0
                            ? <>alvo <span class="font-semibold text-base-content">{targetTemp.value.toFixed(1)}°C</span></>
                            : <>sem alvo</>}
                    </div>
                    <span class={`text-xs uppercase tracking-wide ${status.value.color}`}>
                        {status.value.label}
                    </span>
                </div>

                {/* Chart */}
                <div class="mt-1">
                    <TemperatureChart />
                </div>

                {/* Secondary telemetry strip */}
                <div class="grid grid-cols-3 gap-2 mt-2 text-center text-xs">
                    <Stat label="PID" value={`${pidPct}%`} />
                    <Stat label="Heater" value={heaterOn.value ? 'ON' : 'off'}
                          highlight={heaterOn.value ? 'text-warning' : ''} />
                    <Stat label="Bomba" value={pumpOn.value ? 'ON' : 'off'}
                          highlight={pumpOn.value ? 'text-info' : ''} />
                </div>
            </div>
        </section>
    );
}

function Stat({ label, value, highlight = '' }) {
    return (
        <div class="bg-base-200/60 rounded px-2 py-1">
            <div class="text-base-content/50 uppercase tracking-wider text-[10px]">{label}</div>
            <div class={`font-mono font-semibold ${highlight}`}>{value}</div>
        </div>
    );
}
