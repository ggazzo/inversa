// metro.config.js — Expo Metro tuned for the monorepo.
//
// Two non-default tweaks:
//
//   1. `watchFolders` points at the monorepo root so Metro indexes the
//      sibling packages (`@inversa/ui`, `@inversa/stores`, ...). Without
//      this Metro only watches `apps/native/` and errors on resolve.
//
//   2. `disableHierarchicalLookup: true` + explicit `nodeModulesPaths`
//      keeps Metro from walking up the filesystem past the monorepo
//      root, so a globally-installed `react-native` (or a parent
//      `node_modules/`) can't shadow our pinned version.

const { getDefaultConfig } = require('expo/metro-config');
const path = require('path');

const projectRoot = __dirname;
const workspaceRoot = path.resolve(projectRoot, '../..');

const config = getDefaultConfig(projectRoot);

config.watchFolders = [workspaceRoot];

config.resolver.nodeModulesPaths = [
    path.resolve(projectRoot, 'node_modules'),
    path.resolve(workspaceRoot, 'node_modules'),
];
config.resolver.disableHierarchicalLookup = true;

// Make sure Tamagui's `tamagui.config.ts` and friends resolve. Tamagui
// ships its own CJS/ESM bifurcation so we leave Metro's defaults alone
// for sourceExts/assetExts.

module.exports = config;
