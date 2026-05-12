// babel.config.js — Expo + Tamagui + signals-react-transform.
//
// Order matters:
//
//   1. babel-preset-expo handles JSX, TS, RN-specific transforms.
//   2. @preact/signals-react-transform runs FIRST so it can see
//      original component bodies before Tamagui rewrites their JSX.
//      It auto-injects `import { useSignals } from
//      '@preact/signals-react/runtime'` into every component and
//      wraps the body with `useSignals()` so signal reads track.
//   3. @tamagui/babel-plugin runs Tamagui's optimizing compiler on
//      style props, mirroring what @tamagui/vite-plugin does on web.
//      Without it the runtime is fine but the bundle is heavier.
//   4. react-native-worklets/plugin MUST be last (its own docs
//      enforce this). In Reanimated 4 the plugin moved out of the
//      reanimated package into `react-native-worklets/plugin`; using
//      the old `react-native-reanimated/plugin` path silently misses
//      the worklet transform and the runtime throws "react-native-
//      reanimated is not installed" on first render.

module.exports = function (api) {
    api.cache(true);
    return {
        presets: [
            ['babel-preset-expo', { jsxRuntime: 'automatic' }],
        ],
        plugins: [
            [
                '@tamagui/babel-plugin',
                {
                    components: ['tamagui'],
                    config: './tamagui.config.ts',
                    logTimings: true,
                    disableExtraction: process.env.NODE_ENV === 'development',
                },
            ],
            'react-native-worklets/plugin',
        ],
        // signals-react-transform must only see our own code. If it runs
        // on node_modules, it sees Reanimated's worklets that read
        // `.value` on shared values and "helpfully" wraps them — the
        // injected `_useSignals` import then can't be serialised to the
        // UI thread runtime, so the worklet throws "_useSignals is not
        // a function" at first invocation. Same trap for any other
        // library that uses `.value` (Skia, victory-native chart-press
        // hooks, etc.).
        overrides: [
            {
                test: (filename) =>
                    typeof filename === 'string'
                    && !filename.includes('node_modules'),
                plugins: [
                    ['module:@preact/signals-react-transform'],
                ],
            },
        ],
    };
};
