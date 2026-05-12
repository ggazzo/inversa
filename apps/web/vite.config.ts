// Vite configuration for the Tamagui-based PWA.
//
// Two non-trivial plugins beyond the React preset:
//
//  - `@tamagui/vite-plugin` runs Tamagui's optimizing compiler on
//    JSX, hoisting style props into CSS at build time. Without it the
//    runtime is fine but the bundle includes the unused style engine
//    for every component.
//
//  - `@preact/signals-react-transform` wires React's reconciler to
//    signal subscriptions so reading `signal.value` inside a component
//    automatically re-renders it on change — the same DX we have today
//    with Preact + signals, but with React 18.

import { defineConfig } from 'vite';
import react from '@vitejs/plugin-react';
import { tamaguiPlugin } from '@tamagui/vite-plugin';

export default defineConfig({
    plugins: [
        react({
            babel: {
                plugins: [['module:@preact/signals-react-transform']],
            },
        }),
        tamaguiPlugin({
            config: './tamagui.config.ts',
            components: ['tamagui'],
            optimize: true,
        }) as any,
    ],
    // `.web.ts` ahead of `.ts` so cross-platform packages can ship
    // browser-specific shims (BleAdapter.web.ts, etc.) without
    // polluting the native bundle. Metro mirrors this with its built-in
    // `.native.ts` priority.
    resolve: {
        extensions: ['.web.tsx', '.web.ts', '.web.jsx', '.web.js',
                     '.tsx', '.ts', '.jsx', '.js', '.json'],
    },
    build: {
        outDir: 'dist',
        // Target the ESP32 SPIFFS host (any modern static server). We
        // keep `assetsInlineLimit` low so the chart code stays as a
        // separate JS chunk that the SW can cache independently.
        assetsInlineLimit: 4096,
        minify: 'esbuild',
        rollupOptions: {
            output: {
                manualChunks: undefined,
            },
        },
    },
    server: {
        port: 5174,         // 5173 is the legacy web/ project
        host: '127.0.0.1',
    },
});
