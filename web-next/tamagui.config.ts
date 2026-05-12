// tamagui.config.ts — Inversa design tokens + themes.
//
// We layer on top of `@tamagui/config/v3` (Tamagui's default preset:
// spacing scale, font tokens, animation drivers) and override the
// colour ramps to match the brand palette already established by the
// DaisyUI version of the UI. The accent stays the orange #f97316; the
// status palette (heating/holding/cooling/error/paused) is preserved
// so the cross-fade between the two versions doesn't visually drift.
//
// `themes` exports `dark` and `light` so the theme toggle in TopBar
// can switch them with `useThemeName`. The default is `dark`.

import { config } from '@tamagui/config/v3';
import { createTamagui } from 'tamagui';

const inversa = createTamagui({
    ...config,
    themes: {
        ...config.themes,
        dark: {
            ...config.themes.dark,
            // brand
            primary:        '#f97316',
            primaryHover:   '#fb923c',
            primaryPress:   '#ea580c',

            // status (used by TempInstrument and pills)
            heating:        '#f97316',
            holding:        '#22c55e',
            cooling:        '#3b82f6',
            error:          '#ef4444',
            paused:         '#facc15',

            // backdrops tuned to the legacy DaisyUI dark theme
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
