// P20 — generate raster PNG icons from the SVG source.
// iOS Safari and some Android launchers will not pick up SVG-only PWAs for
// "Add to Home Screen". This script runs as a prebuild step.

import { readFileSync, writeFileSync, mkdirSync } from 'node:fs';
import { dirname, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';
import sharp from 'sharp';

const here   = dirname(fileURLToPath(import.meta.url));
const root   = resolve(here, '..');
const svg    = readFileSync(resolve(root, 'public/icon.svg'));
const outDir = resolve(root, 'public');

mkdirSync(outDir, { recursive: true });

const sizes = [
  { size: 192, name: 'icon-192.png' },
  { size: 512, name: 'icon-512.png' },
  // Apple wants 180x180 specifically; iOS will scale, but cleaner is exact.
  { size: 180, name: 'apple-touch-icon.png' },
];

for (const { size, name } of sizes) {
  const buf = await sharp(svg, { density: 512 })
    .resize(size, size)
    .png()
    .toBuffer();
  writeFileSync(resolve(outDir, name), buf);
  console.log(`[icons] wrote ${name} (${size}×${size}, ${buf.length} bytes)`);
}
