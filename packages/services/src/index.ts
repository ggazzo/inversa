// The BleAdapter abstraction will land when the native app is scaffolded
// (so we design the interface against a second, real implementation).
// Until then BLEService is the only impl and we re-export it directly.

export * from './BLEService';
export * from './ConnectionManager';
