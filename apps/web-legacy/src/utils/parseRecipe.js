// parseRecipe.js — minimal mirror of firmware/src/plugins/RecipePlugin.h's
// parser, focused on what the timeline UI needs to display.
//
// Each output entry mirrors what the firmware turns into a step (`#`
// comments and unparseable lines are skipped), so the index in the
// returned array matches the firmware's `recipeStep`. That alignment is
// the whole point — without it the timeline highlights the wrong row.
//
// Returned shape:
//   { type, label, value, value2, message, raw, kind }
//   - `kind` groups the step for icons: temp/timer/boil/hop/confirm/actuator/marker/ramp.

const KNOWN = [
    'SET_TEMP', 'WAIT_TEMP', 'MASH_OUT',
    'WAIT_TIMER', 'TIMER', 'ALARM',
    'BOIL', 'ADD_HOP', 'WAIT_BOIL',
    'RAMP_OFF', 'RAMP',
    'PUMP_ON', 'PUMP_OFF', 'HEATER_ON', 'HEATER_OFF',
    'WAIT_CONFIRM',
    'STEP',
];

function extractQuoted(rest) {
    const m = rest.match(/"([^"]*)"/);
    return m ? m[1] : rest.trim();
}

function parseLine(line) {
    const raw = line.trim();
    if (!raw || raw.startsWith('#')) return null;

    const upper = raw.toUpperCase();
    // SET_TEMP <v>
    if (upper.startsWith('SET_TEMP')) {
        const v = parseFloat(raw.slice(8));
        return { type: 'SET_TEMP', label: `Setpoint ${isFinite(v) ? v + '°C' : ''}`,
                 value: v, kind: 'temp', raw };
    }
    if (upper.startsWith('WAIT_TEMP')) {
        const tol = parseFloat(raw.slice(9));
        return { type: 'WAIT_TEMP', label: 'Aguardar temperatura',
                 value: isFinite(tol) ? tol : 0.5, kind: 'temp', raw };
    }
    if (upper.startsWith('MASH_OUT')) {
        const t = parseFloat(raw.slice(8));
        return { type: 'MASH_OUT', label: `Mash-out${isFinite(t) ? ` (${t}°C)` : ''}`,
                 value: isFinite(t) ? t : 76, kind: 'temp', raw };
    }
    if (upper.startsWith('WAIT_TIMER')) {
        const m = parseFloat(raw.slice(10));
        return { type: 'WAIT_TIMER', label: `Aguardar ${m} min`,
                 value: m, kind: 'timer', raw };
    }
    if (upper.startsWith('TIMER')) {
        const m = parseFloat(raw.slice(5));
        return { type: 'TIMER', label: `Timer ${m} min`,
                 value: m, kind: 'timer', raw };
    }
    if (upper.startsWith('ALARM')) {
        const hhmm = raw.slice(5).trim();
        return { type: 'ALARM', label: `Alarme ${hhmm}`, message: hhmm, kind: 'timer', raw };
    }
    if (upper.startsWith('WAIT_BOIL')) {
        return { type: 'WAIT_BOIL', label: 'Aguardar fim da fervura', kind: 'boil', raw };
    }
    if (upper.startsWith('BOIL')) {
        const m = parseFloat(raw.slice(4));
        return { type: 'BOIL', label: `Ferver ${m} min`,
                 value: m, kind: 'boil', raw };
    }
    if (upper.startsWith('ADD_HOP')) {
        const rest = raw.slice(7).trim();
        const sp = rest.indexOf(' ');
        const min = sp > 0 ? parseFloat(rest.slice(0, sp)) : parseFloat(rest);
        const name = sp > 0 ? extractQuoted(rest.slice(sp + 1).trim()) : 'Lúpulo';
        return { type: 'ADD_HOP', label: `Lúpulo: ${name}`,
                 value: min, message: name, kind: 'hop', raw };
    }
    if (upper.startsWith('RAMP_OFF')) {
        return { type: 'RAMP_OFF', label: 'Desligar rampa', kind: 'ramp', raw };
    }
    if (upper.startsWith('RAMP')) {
        const r = parseFloat(raw.slice(4));
        return { type: 'RAMP', label: `Rampa ${r}°C/min`,
                 value: r, kind: 'ramp', raw };
    }
    if (upper.startsWith('PUMP_ON'))    return { type: 'PUMP_ON',    label: 'Ligar bomba',     kind: 'actuator', raw };
    if (upper.startsWith('PUMP_OFF'))   return { type: 'PUMP_OFF',   label: 'Desligar bomba',  kind: 'actuator', raw };
    if (upper.startsWith('HEATER_ON'))  return { type: 'HEATER_ON',  label: 'Ligar aquecedor', kind: 'actuator', raw };
    if (upper.startsWith('HEATER_OFF')) return { type: 'HEATER_OFF', label: 'Desligar aquecedor', kind: 'actuator', raw };
    if (upper.startsWith('WAIT_CONFIRM')) {
        const msg = extractQuoted(raw.slice(12));
        return { type: 'WAIT_CONFIRM', label: msg || 'Confirmação manual',
                 message: msg, kind: 'confirm', raw };
    }
    if (upper.startsWith('STEP')) {
        const name = extractQuoted(raw.slice(4));
        return { type: 'STEP', label: name || 'Etapa', message: name, kind: 'marker', raw };
    }

    return null;   // Unknown — firmware also skips
}

export function parseRecipe(content) {
    if (!content) return [];
    return content.split('\n')
        .map(parseLine)
        .filter(Boolean);
}

export const KIND_ICON = {
    temp:     '🌡',
    timer:    '⏳',
    boil:     '🔥',
    hop:      '🌿',
    confirm:  '✋',
    marker:   '🏷',
    actuator: '🎛',
    ramp:     '📈',
};

export { KNOWN };
