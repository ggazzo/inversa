// util.ts — small UI helpers used across sub-states.

export function pad(n: number): string {
    return String(n).padStart(2, '0');
}

export function fmtMmSs(secs: number): string {
    if (!secs || secs < 0) return '--:--';
    const h = Math.floor(secs / 3600);
    const m = Math.floor((secs % 3600) / 60);
    const s = secs % 60;
    return h > 0 ? `${h}:${pad(m)}:${pad(s)}` : `${pad(m)}:${pad(s)}`;
}

export type TempStatus = {
    color: '$heating' | '$cooling' | '$holding' | '$color';
    label: 'aquecendo' | 'esfriando' | 'no alvo' | 'inativa';
};

export function tempStatus(current: number, target: number, mode: string): TempStatus {
    if (mode === 'idle' || target <= 0) return { color: '$color',    label: 'inativa'   };
    const d = current - target;
    if (d < -1.0)                       return { color: '$heating',  label: 'aquecendo' };
    if (d >  1.0)                       return { color: '$cooling',  label: 'esfriando' };
    return                                       { color: '$holding', label: 'no alvo'   };
}
