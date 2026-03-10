import { defineConfig } from 'vite';
import preact from '@preact/preset-vite';
import tailwindcss from '@tailwindcss/vite';

export default defineConfig({
  plugins: [
    preact(),
    tailwindcss(),
  ],
  build: {
    // Optimize for ESP32 SPIFFS (small output)
    outDir: 'dist',
    assetsInlineLimit: 4096,
    rollupOptions: {
      output: {
        manualChunks: undefined, // Single chunk for smaller size
      },
    },
    minify: 'esbuild',
  },
  server: {
    port: 3000,
  },
});
