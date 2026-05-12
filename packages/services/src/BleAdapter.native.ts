// BleAdapter.native.ts — stub for Phase E.
//
// Metro picks this up over BleAdapter.ts because `.native.ts` wins in
// Metro's default resolution order. Today it just yells; Phase E
// fills it with:
//
//   * react-native-ble-plx for real BLE (NUS service, like web)
//   * a WebSocket transport for the sim bridge (RN has WebSocket
//     globally, no shim required), gated by the same `?sim=` URL
//     fragment but read from the deep-link / launch URL instead of
//     `location.search`.

import type { BleAdapter } from './BleAdapter';

class NativeBleAdapterStub implements BleAdapter {
    isSupported(): boolean { return false; }
    async connect(): Promise<void> {
        throw new Error('BleAdapter.native is not implemented yet (Phase E)');
    }
    disconnect(): void { /* noop */ }
    async send(_message: object): Promise<void> {
        throw new Error('BleAdapter.native is not implemented yet (Phase E)');
    }
    onMessage(_cb: (data: any) => void): void { /* noop */ }
    onConnect(_cb: () => void): void { /* noop */ }
    onDisconnect(_cb: () => void): void { /* noop */ }
    getDeviceName(): string | null { return null; }
}

export function createBleAdapter(): BleAdapter {
    return new NativeBleAdapterStub();
}
