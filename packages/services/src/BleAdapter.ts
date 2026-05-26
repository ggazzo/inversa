// BleAdapter.ts — platform-agnostic transport interface.
//
// Two implementations resolve at bundle time:
//
//   * BleAdapter.web.ts     — Web Bluetooth (real device) + WebSocket
//                             (sim bridge, selected by `?sim=ws://...`).
//                             Picked up by Vite when `resolve.extensions`
//                             puts `.web.ts` before `.ts`.
//
//   * BleAdapter.native.ts  — react-native-ble-plx + WebSocket sim path
//                             (selected by `EXPO_PUBLIC_BREWPILOT_SIM_URL`).
//                             Picked up by Metro automatically.
//
// Each adapter is responsible for the wire protocol (chunk re-assembly
// for real BLE, line-framed JSON for the sim WebSocket). It hands
// already-parsed objects up to BleClient, which adds rid-correlated
// request/response on top — that layer is portable and lives in
// BleClient.ts.

/** A device discovered during a scan, before any connection. */
export interface BleScanDevice {
    /** Stable identifier used to connect — the GATT id on ble-plx,
     *  arbitrary on sim/web. */
    id: string;
    /** Best-effort advertised name. May be null if the device hasn't
     *  set one or the advertisement didn't include it. */
    name: string | null;
    /** Received Signal Strength Indicator in dBm (negative; closer to
     *  zero is stronger). Optional — sim doesn't expose it. */
    rssi?: number | null;
}

export interface BleAdapter {
    /** Whether the runtime can talk to a device at all (Web Bluetooth
     *  present, sim URL set, or ble-plx available). UI uses this to
     *  gate the Connect button. */
    isSupported(): boolean;

    /** True when this adapter needs the app to host its own device-
     *  picker UI. False on web (the browser shows its native picker
     *  inside `requestDevice`) and on RN in sim mode (the WebSocket
     *  url is fixed so there's nothing to pick). True on RN with real
     *  BLE — the UI should call `startScan` + `connectToDevice` rather
     *  than `connect`. */
    needsPicker(): boolean;

    /** Open a connection without prompting the UI for a device.
     *  - Web: pops the browser's BLE picker, returns when connected.
     *  - Sim: opens the WebSocket immediately.
     *  - RN real BLE: scans and auto-connects to the first match.
     *    The picker flow (`startScan` + `connectToDevice`) is preferred
     *    when multiple devices may be in range. */
    connect(): Promise<void>;

    /** Start scanning for nearby BrewPilot devices. Each discovery fires
     *  `onDevice`; transient scan failures fire `onError`. Returns a
     *  function that stops the scan. Safe to call multiple times — the
     *  adapter coalesces. */
    startScan(
        onDevice: (d: BleScanDevice) => void,
        onError?: (e: Error) => void,
    ): () => void;

    /** Connect to a specific device id (from a `startScan` result).
     *  Use this together with `startScan` when `needsPicker()` is true. */
    connectToDevice(id: string): Promise<void>;

    /** Close the connection. Idempotent; safe to call on a dead link. */
    disconnect(): void;

    /** Send one JSON object to the device. The adapter handles framing
     *  (chunk by MTU on real BLE, one WS frame per message on sim). */
    send(message: object): Promise<void>;

    /** Subscribe to every parsed message coming back from the device.
     *  Called per message, not per chunk. */
    onMessage(cb: (data: any) => void): void;

    /** Fired once after a successful connect. */
    onConnect(cb: () => void): void;

    /** Fired when the link drops, whether by `disconnect()` or by the
     *  device going away. */
    onDisconnect(cb: () => void): void;

    /** Best-effort label for the connected device. `null` before
     *  connect or when the transport doesn't expose a name (sim). */
    getDeviceName(): string | null;

    /** Subscribe to RSSI samples (dBm, negative; closer to 0 = stronger).
     *  Native adapter polls every few seconds while connected. Web and
     *  sim never fire. */
    onRssi(cb: (rssi: number | null) => void): void;
}

/** Throws on every operation. Replaced at bundle time by
 *  BleAdapter.web.ts (Vite) or BleAdapter.native.ts (Metro). */
class UnsupportedAdapter implements BleAdapter {
    isSupported(): boolean { return false; }
    needsPicker(): boolean { return false; }
    async connect(): Promise<void> { throw new Error('No BleAdapter for this platform'); }
    startScan(_onDevice: (d: BleScanDevice) => void, _onError?: (e: Error) => void): () => void {
        return () => { /* noop */ };
    }
    async connectToDevice(_id: string): Promise<void> { throw new Error('No BleAdapter for this platform'); }
    disconnect(): void { /* noop */ }
    async send(_message: object): Promise<void> { throw new Error('No BleAdapter for this platform'); }
    onMessage(_cb: (data: any) => void): void { /* noop */ }
    onConnect(_cb: () => void): void { /* noop */ }
    onDisconnect(_cb: () => void): void { /* noop */ }
    onRssi(_cb: (rssi: number | null) => void): void { /* noop */ }
    getDeviceName(): string | null { return null; }
}

/** Factory used by BleClient. Bundler-resolved overrides export the
 *  real implementation under the same name. */
export function createBleAdapter(): BleAdapter {
    return new UnsupportedAdapter();
}
