// tamagui.config.ts — same tokens as apps/web. Duplicated for now;
// promote to a shared package once both platforms have proven the
// config resolves through Metro and Vite identically. The babel
// plugin reads this file at compile time, the runtime reads it via
// `<TamaguiProvider>`, so any centralisation needs to keep both paths
// working.

import { config } from '@tamagui/config/v3';
import { createTamagui } from 'tamagui';

const inversa = createTamagui({
    ...config,
    themes: {
        ...config.themes,
        dark: {
            ...config.themes.dark,
            primary:        '#f97316',
            primaryHover:   '#fb923c',
            primaryPress:   '#ea580c',
            heating:        '#f97316',
            holding:        '#22c55e',
            cooling:        '#3b82f6',
            error:          '#ef4444',
            paused:         '#facc15',
            background:     '#1d232a',
            backgroundHover:'#272d36',
            backgroundPress:'#1a1f25',
        },
        light: {
            ...config.themes.light,
            primary:        '#f97316',
            primaryHover:   '#fb923c',
            primaryPress:   '#ea580c',
            heating:        '#f97316',
            holding:        '#16a34a',
            cooling:        '#2563eb',
            error:          '#dc2626',
            paused:         '#ca8a04',
            background:     '#ffffff',
            backgroundHover:'#f3f4f6',
            backgroundPress:'#e5e7eb',
        },
    },
});

type InversaConf = typeof inversa;

declare module 'tamagui' {
    interface TamaguiCustomConfig extends InversaConf {}
}

export default inversa;
