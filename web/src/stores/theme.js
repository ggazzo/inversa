// theme.js — DaisyUI theme selection persisted to localStorage.
// We apply on import (early in the app boot) so the UI doesn't flash a
// wrong-theme frame before the first render.

import { signal } from '@preact/signals';

const KEY = 'inversa.theme';
const VALID = ['dark', 'light'];

function readInitial() {
    try {
        const v = localStorage.getItem(KEY);
        if (VALID.includes(v)) return v;
    } catch {}
    // Fall back to system pref when available; default dark.
    if (typeof matchMedia === 'function' && matchMedia('(prefers-color-scheme: light)').matches) {
        return 'light';
    }
    return 'dark';
}

export const theme = signal(readInitial());

function apply(t) {
    if (typeof document === 'undefined') return;
    document.documentElement.setAttribute('data-theme', t);
}
apply(theme.value);

export function toggleTheme() {
    theme.value = theme.value === 'dark' ? 'light' : 'dark';
    try { localStorage.setItem(KEY, theme.value); } catch {}
    apply(theme.value);
}
