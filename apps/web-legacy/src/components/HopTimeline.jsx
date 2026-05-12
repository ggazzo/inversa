// HopTimeline.jsx — horizontal strip showing hop additions along the
// boil. Renders past alerts (`boilAlerts`) as dimmed dots on the right
// (closer to "now") and the remaining boil time as a long line.
//
// The firmware only broadcasts past alerts (`evt:boil:addition`), not
// the full schedule, so the timeline is necessarily retrospective. We
// also show the `boilAdditions` count as a hint ("X adições programadas").

import { boilAlerts, boilTotal, boilRemaining, boilAdditions } from '../stores/state';

function pad(n) { return String(n).padStart(2, '0'); }
function fmtMmSs(secs) {
    if (!secs || secs < 0) return '--:--';
    const m = Math.floor(secs / 60), s = secs % 60;
    return `${pad(m)}:${pad(s)}`;
}

export function HopTimeline() {
    const total = boilTotal.value;
    const left  = boilRemaining.value;
    const elapsed = total > 0 ? total - left : 0;
    const pct = total > 0 ? Math.max(0, Math.min(100, (elapsed / total) * 100)) : 0;

    const alerts = boilAlerts.value;
    const totalConfigured = boilAdditions.value;
    const remaining = Math.max(0, totalConfigured - alerts.length);

    return (
        <div class="bg-base-200/60 rounded-lg p-3">
            <div class="flex items-center justify-between text-xs uppercase tracking-wider text-base-content/50 mb-2">
                <span>Adições</span>
                <span>{alerts.length} feitas · {remaining} pendentes</span>
            </div>

            {/* Timeline line */}
            <div class="relative h-2 bg-base-300 rounded">
                <div class="absolute inset-y-0 left-0 bg-primary/40 rounded"
                     style={{ width: `${pct}%` }} />
                {/* Triggered hops as markers at their proportional position */}
                {alerts.map((a, i) => {
                    // Each alert recorded the minutes-remaining when it fired.
                    // Convert to a percentage along the timeline (left = start).
                    const minutesIn = total > 0 ? (total / 60) - a.min : 0;
                    const left = total > 0 ? (minutesIn * 60 / total) * 100 : 0;
                    return (
                        <div key={i}
                             class="absolute -top-1.5 w-5 h-5 rounded-full bg-warning border-2 border-base-100"
                             style={{ left: `calc(${left}% - 10px)` }}
                             title={`${a.name} (@ ${a.min} min)`}/>
                    );
                })}
            </div>

            {/* Recent alert names (last 3) */}
            {alerts.length > 0 && (
                <ul class="text-xs mt-2 space-y-0.5">
                    {alerts.slice(-3).reverse().map((a, i) => (
                        <li key={i} class="flex justify-between gap-2">
                            <span class="truncate">🌿 {a.name}</span>
                            <span class="text-base-content/50 shrink-0">{a.min} min</span>
                        </li>
                    ))}
                </ul>
            )}

            {/* Remaining boil time */}
            <div class="text-center mt-2 text-2xl font-mono tabular-nums">
                {fmtMmSs(left)}
                <span class="text-xs text-base-content/50 ml-1">restantes</span>
            </div>
        </div>
    );
}
