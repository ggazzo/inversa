// babel.config.js — Expo + Tamagui + signals-react-transform.
//
// Order matters:
//
//   1. babel-preset-expo handles JSX, TS, RN-specific transforms.
//   2. @tamagui/babel-plugin runs Tamagui's optimizing compiler on
//      style props, mirroring what @tamagui/vite-plugin does on web.
//      Without it the runtime is fine but the bundle is heavier.
//   3. @preact/signals-react-transform wires React's reconciler to
//      signal subscriptions so `signal.value` reads trigger re-renders
//      automatically — same DX we have on web.
//   4. react-native-reanimated/plugin MUST be last (its own docs
//      enforce this). Tamagui's RN animation driver uses Reanimated.

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
            ['module:@preact/signals-react-transform'],
            'react-native-reanimated/plugin',
        ],
    };
};
