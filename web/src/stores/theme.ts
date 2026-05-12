// theme.ts — theme selection signal persisted to localStorage.
// Tamagui's `useThemeName` reads this and the TamaguiProvider picks the
// matching theme from `tamagui.config.ts`. We apply on import so the
// first render sees the right tokens (no flash).

import { signal } from '@preact/signals-react';

export type Theme = 'dark' | 'light';

const KEY = 'inversa.theme';
const VALID: readonly Theme[] = ['dark', 'light'];

function readInitial(): Theme {
    try {
        const v = localStorage.getItem(KEY);
        if (v && (VALID as readonly string[]).includes(v)) return v as Theme;
    } catch { /* localStorage may throw in some private modes */ }

    if (typeof matchMedia === 'function' &&
        matchMedia('(prefers-color-scheme: light)').matches) {
        return 'light';
    }
    return 'dark';
}

export const theme = signal<Theme>(readInitial());

function apply(t: Theme): void {
    if (typeof document === 'undefined') return;
    document.documentElement.setAttribute('data-theme', t);
}
apply(theme.value);

export function toggleTheme(): void {
    theme.value = theme.value === 'dark' ? 'light' : 'dark';
    try { localStorage.setItem(KEY, theme.value); } catch { /* noop */ }
    apply(theme.value);
}
