// BleAdapter.ts — platform-agnostic transport interface.
//
// Two implementations resolve at bundle time:
//
//   * BleAdapter.web.ts     — Web Bluetooth (real device) + WebSocket
//                             (sim bridge, selected by `?sim=ws://...`).
//                             Picked up by Vite when `resolve.extensions`
//                             puts `.web.ts` before `.ts`.
//
//   * BleAdapter.native.ts  — react-native-ble-plx (Phase E). Picked up
//                             by Metro automatically (`.native.ts` wins
//                             over `.ts` by default).
//
// Each adapter is responsible for the wire protocol (chunk re-assembly
// for real BLE, line-framed JSON for the sim WebSocket). It hands
// already-parsed objects up to BleClient, which adds rid-correlated
// request/response on top — that layer is portable and lives in
// BleClient.ts.
//
// Importing `./BleAdapter` from BleClient.ts gives you whichever impl
// the bundler resolved. The plain `.ts` file below is a typed fallback
// that throws on any operation; it exists so TypeScript has something
// to type-check against and so Jest / Node test runners (which don't
// know about platform-resolved extensions) get a clear error instead
// of a confusing "module not found".

export interface BleAdapter {
    /** Whether the runtime can talk to a device at all (Web Bluetooth
     *  present, or `?sim=` flag set). UI uses this to gate the Connect
     *  button. */
    isSupported(): boolean;

    /** Open a connection. Rejects if the user cancels the picker, the
     *  GATT handshake fails, or the sim bridge is unreachable. */
    connect(): Promise<void>;

    /** Close the connection. Idempotent; safe to call on a dead link. */
    disconnect(): void;

    /** Send one JSON object to the device. The adapter handles framing
     *  (chunk by MTU on real BLE, one WS frame per message on sim). */
    send(message: object): Promise<void>;

    /** Subscribe to every parsed message coming back from the device.
     *  Called per message, not per chunk. */
    onMessage(cb: (data: any) => void): void;

    /** Fired once after `connect()` resolves successfully. */
    onConnect(cb: () => void): void;

    /** Fired when the link drops, whether by `disconnect()` or by the
     *  device going away. */
    onDisconnect(cb: () => void): void;

    /** Best-effort label for the connected device. `null` before
     *  connect or when the transport doesn't expose a name (sim). */
    getDeviceName(): string | null;
}

/** Throws on every operation. Replaced at bundle time by
 *  BleAdapter.web.ts (Vite) or BleAdapter.native.ts (Metro). */
class UnsupportedAdapter implements BleAdapter {
    isSupported(): boolean { return false; }
    async connect(): Promise<void> { throw new Error('No BleAdapter for this platform'); }
    disconnect(): void { /* noop */ }
    async send(_message: object): Promise<void> { throw new Error('No BleAdapter for this platform'); }
    onMessage(_cb: (data: any) => void): void { /* noop */ }
    onConnect(_cb: () => void): void { /* noop */ }
    onDisconnect(_cb: () => void): void { /* noop */ }
    getDeviceName(): string | null { return null; }
}

/** Factory used by BleClient. Bundler-resolved overrides export the
 *  real implementation under the same name. */
export function createBleAdapter(): BleAdapter {
    return new UnsupportedAdapter();
}
