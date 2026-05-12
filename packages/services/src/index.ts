// BleClient is the cross-platform entry point. The transport beneath
// it (BleAdapter.web.ts or BleAdapter.native.ts) is selected at bundle
// time via extension resolution — Vite is configured to prefer
// `.web.ts`, Metro prefers `.native.ts` by default.

export * from './BleAdapter';
export * from './BleClient';
export * from './ConnectionManager';
