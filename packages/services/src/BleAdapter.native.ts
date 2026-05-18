// BleAdapter.native.ts — react-native-ble-plx implementation.
//
// Mirrors the web adapter's behaviour against the same Nordic UART
// Service the firmware exposes:
//
//   * Scan filtered by NUS service UUID (avoids paging through every
//     advertisement on a crowded RF environment).
//   * Connect → discover services/characteristics → subscribe to the
//     TX characteristic (`6e400003`) for notifications.
//   * Writes go to the RX characteristic (`6e400002`) without
//     response, chunked to fit the negotiated MTU.
//
// Transport layer notes:
//
//   * ble-plx hands us base64 strings on both directions. We pivot
//     through utf-8 with the tiny base64 helpers below so the JSON
//     framing logic stays the same as on web.
//
//   * Android 12+ requires the user to grant BLUETOOTH_SCAN and
//     BLUETOOTH_CONNECT at runtime even though the manifest declares
//     them. We request both before scanning. iOS 13+ asks for the
//     Always-Bluetooth string the moment we instantiate BleManager,
//     so no extra plumbing on that side.
//
//   * The simulator path (`?sim=ws://...`) is web-only. On RN the
//     dev workflow uses a real device against the firmware; a future
//     enhancement could read a deep-link URL parameter and short-
//     circuit to WebSocket, but for now sim simply isn't reachable
//     from the native app.

import { NativeModules, PermissionsAndroid, Platform } from 'react-native';
import type { BleAdapter, BleScanDevice } from './BleAdapter';
import { extractFrames } from './frameExtractor';

const NUS_SERVICE_UUID = '6e400001-b5a3-f393-e0a9-e50e24dcca9e';
const NUS_TX_CHAR_UUID = '6e400003-b5a3-f393-e0a9-e50e24dcca9e'; // notify: device → app
const NUS_RX_CHAR_UUID = '6e400002-b5a3-f393-e0a9-e50e24dcca9e'; // write:  app → device

// ble-plx is a real native module; in the Expo dev-client it's
// available, but during `expo export` in CI the JS bundle is built
// without the native binary being linked. Importing it eagerly at
// module load makes Metro happy (JS-only resolution) and the
// runtime require throws only if the module isn't linked — which is
// the same failure mode as any other native module.

// We require lazily so the Metro bundle in CI doesn't fail if
// react-native-ble-plx ever pulls in a host-binding it can't satisfy
// during dry-export. The cost is one require() at first connect().
type BleManagerCtor = new () => any;
let _BleManager: BleManagerCtor | null = null;
function getBleManagerCtor(): BleManagerCtor {
    if (_BleManager) return _BleManager;
    // eslint-disable-next-line @typescript-eslint/no-var-requires
    const mod = require('react-native-ble-plx');
    _BleManager = mod.BleManager as BleManagerCtor;
    return _BleManager;
}

// Base64 helpers. RN ships `global.btoa`/`atob` polyfills in recent
// versions, but they're not guaranteed everywhere — and our payload
// is UTF-8 JSON, which the browser atob doesn't handle directly
// (it's binary-safe only). We do the JSON → bytes → base64 trip via
// a small inline routine that uses Buffer when available (Hermes
// 0.79+) and falls back to a manual encoder.
function encodeBase64(input: string): string {
    if (typeof (globalThis as any).Buffer !== 'undefined') {
        return (globalThis as any).Buffer.from(input, 'utf8').toString('base64');
    }
    return btoaUtf8(input);
}
function decodeBase64(input: string): string {
    if (typeof (globalThis as any).Buffer !== 'undefined') {
        return (globalThis as any).Buffer.from(input, 'base64').toString('utf8');
    }
    return atobUtf8(input);
}

const B64 = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/';
function btoaUtf8(str: string): string {
    const bytes = unescape(encodeURIComponent(str));
    let out = '';
    for (let i = 0; i < bytes.length; i += 3) {
        const b1 = bytes.charCodeAt(i);
        const b2 = i + 1 < bytes.length ? bytes.charCodeAt(i + 1) : NaN;
        const b3 = i + 2 < bytes.length ? bytes.charCodeAt(i + 2) : NaN;
        out += B64[b1 >> 2];
        out += B64[((b1 & 3) << 4) | ((Number.isNaN(b2) ? 0 : b2) >> 4)];
        out += Number.isNaN(b2) ? '=' : B64[((b2 & 15) << 2) | ((Number.isNaN(b3) ? 0 : b3) >> 6)];
        out += Number.isNaN(b3) ? '=' : B64[b3 & 63];
    }
    return out;
}
function atobUtf8(b64: string): string {
    const lookup = new Map(B64.split('').map((c, i) => [c, i]));
    let bytes = '';
    for (let i = 0; i < b64.length; i += 4) {
        const c1 = lookup.get(b64[i])!;
        const c2 = lookup.get(b64[i + 1])!;
        const c3 = b64[i + 2] === '=' ? 0 : (lookup.get(b64[i + 2]) ?? 0);
        const c4 = b64[i + 3] === '=' ? 0 : (lookup.get(b64[i + 3]) ?? 0);
        bytes += String.fromCharCode((c1 << 2) | (c2 >> 4));
        if (b64[i + 2] !== '=') bytes += String.fromCharCode(((c2 & 15) << 4) | (c3 >> 2));
        if (b64[i + 3] !== '=') bytes += String.fromCharCode(((c3 & 3) << 6) | c4);
    }
    return decodeURIComponent(escape(bytes));
}

// Runtime permission grant for Android. iOS doesn't need this.
async function ensureAndroidPermissions(): Promise<boolean> {
    if (Platform.OS !== 'android') return true;
    const perms = [
        PermissionsAndroid.PERMISSIONS.BLUETOOTH_SCAN,
        PermissionsAndroid.PERMISSIONS.BLUETOOTH_CONNECT,
        // Some older OEM stacks (pre-Android 12) still gate scans on location.
        PermissionsAndroid.PERMISSIONS.ACCESS_FINE_LOCATION,
    ].filter(Boolean) as string[];
    const granted = await PermissionsAndroid.requestMultiple(perms);
    return perms.every((p) => granted[p] === PermissionsAndroid.RESULTS.GRANTED);
}

// Sim bridge URL. Set via `EXPO_PUBLIC_INVERSA_SIM_URL` when starting
// the dev server (e.g. `EXPO_PUBLIC_INVERSA_SIM_URL=ws://192.168.1.10:8765
// npx expo start`). When the variable is set, connect() opens a
// WebSocket to that URL instead of scanning for BLE — the only way to
// exercise the app on iOS Simulator (no Bluetooth radio) or test
// against the brewing simulator from a physical device on the same
// LAN. Leave unset for real BLE.
function getSimUrl(): string | null {
    const url = process.env.EXPO_PUBLIC_INVERSA_SIM_URL;
    return typeof url === 'string' && url.length > 0 ? url : null;
}

class NativeBleAdapter implements BleAdapter {
    private manager: any | null = null;
    private device:  any | null = null;
    private simSocket: WebSocket | null = null;
    private connected = false;
    private rxBuffer  = '';
    private monitorSub: any | null = null;
    private connectSub: any | null = null;
    private scanning  = false;

    private _onMessage:    ((data: any) => void) | null = null;
    private _onConnect:    (() => void) | null = null;
    private _onDisconnect: (() => void) | null = null;
    private _onRssi:       ((rssi: number | null) => void) | null = null;
    private rssiTimer:     ReturnType<typeof setInterval> | null = null;

    private ensureManager(): any {
        if (!this.manager) {
            const Ctor = getBleManagerCtor();
            this.manager = new Ctor();
        }
        return this.manager;
    }

    isSupported(): boolean {
        // Sim mode is always supported (WebSocket is built into RN).
        if (getSimUrl()) return true;
        // The native module ships with the dev-client (or any custom
        // build). Bare Expo Go can't host BLE, but our default flow is
        // the dev-client so we report true. If the require fails at
        // first connect, the user sees a clear error.
        return !!NativeModules.BleClientManager
            || Platform.OS === 'ios' || Platform.OS === 'android';
    }

    /** Sim mode hard-wires the URL — no device to pick. Real BLE needs
     *  the UI to drive a scan-and-select flow. */
    needsPicker(): boolean {
        return getSimUrl() === null;
    }

    /** Convenience: in sim mode opens the WebSocket directly; in real
     *  BLE mode, scans for 10s and auto-connects to the first match.
     *  UI prefers the explicit picker flow (`startScan` +
     *  `connectToDevice`) on real BLE so users can choose between
     *  multiple devices in range. */
    async connect(): Promise<void> {
        const simUrl = getSimUrl();
        if (simUrl) return this.connectSim(simUrl);

        const ok = await ensureAndroidPermissions();
        if (!ok) throw new Error('Bluetooth: permissões negadas');

        const manager = this.ensureManager();

        const device = await new Promise<any>((resolve, reject) => {
            const timer = setTimeout(() => {
                manager.stopDeviceScan();
                reject(new Error('Nenhum Inversa encontrado em 10s'));
            }, 10_000);

            manager.startDeviceScan([NUS_SERVICE_UUID], null, (error: any, d: any) => {
                if (error) {
                    clearTimeout(timer);
                    manager.stopDeviceScan();
                    reject(error);
                    return;
                }
                if (d) {
                    clearTimeout(timer);
                    manager.stopDeviceScan();
                    resolve(d);
                }
            });
        });

        const connected = await device.connect();
        await this.attachToDevice(connected);
    }

    startScan(
        onDevice: (d: BleScanDevice) => void,
        onError?: (e: Error) => void,
    ): () => void {
        // Sim mode: yield a single pseudo-device immediately. The UI
        // picker then "connects" by routing through connectToDevice,
        // which opens the WebSocket like the bare `connect()` does.
        const simUrl = getSimUrl();
        if (simUrl) {
            queueMicrotask(() => onDevice({ id: '__sim__', name: `Simulador (${simUrl})` }));
            return () => { /* noop */ };
        }

        let active = true;

        const begin = async () => {
            const ok = await ensureAndroidPermissions();
            if (!active) return;
            if (!ok) {
                onError?.(new Error('Bluetooth: permissões negadas'));
                return;
            }
            try {
                const manager = this.ensureManager();
                this.scanning = true;
                manager.startDeviceScan([NUS_SERVICE_UUID], null, (error: any, d: any) => {
                    if (!active) return;
                    if (error) {
                        onError?.(error);
                        return;
                    }
                    if (d) {
                        onDevice({
                            id:   d.id,
                            name: d.name ?? d.localName ?? null,
                            rssi: d.rssi ?? null,
                        });
                    }
                });
            } catch (e: any) {
                onError?.(e);
            }
        };

        begin();

        return () => {
            active = false;
            if (this.scanning && this.manager) {
                try { this.manager.stopDeviceScan(); } catch { /* noop */ }
                this.scanning = false;
            }
        };
    }

    async connectToDevice(id: string): Promise<void> {
        const simUrl = getSimUrl();
        if (simUrl) return this.connectSim(simUrl);

        const manager = this.ensureManager();
        // ble-plx requires stopping any active scan before connecting,
        // otherwise iOS rejects with a generic GATT error.
        if (this.scanning) {
            try { manager.stopDeviceScan(); } catch { /* noop */ }
            this.scanning = false;
        }
        // ble-plx's connectToDevice() does the GATT handshake; we still
        // need our own service/characteristic discovery + monitor setup
        // on top, which attachToDevice handles.
        const device = await manager.connectToDevice(id);
        await this.attachToDevice(device);
    }

    /** Shared post-scan wiring: discover services, subscribe to the TX
     *  characteristic, register disconnect handler. Caller hands us an
     *  already-connected Device (either via manager.connectToDevice or
     *  device.connect()). */
    private async attachToDevice(connected: any): Promise<void> {
        await connected.discoverAllServicesAndCharacteristics();
        console.log('[BLE] services discovered, id=', connected.id);

        // Track disconnects so the UI updates. ble-plx fires this for
        // every reason (BLE link drop, user-initiated, OS reset).
        this.connectSub = connected.onDisconnected((err: any) => {
            console.log('[BLE] onDisconnected', err?.message ?? '');
            this.stopRssiPolling();
            this.connected = false;
            this.device = null;
            this._onDisconnect?.();
        });

        // Subscribe to the TX characteristic. Each notification is a
        // chunk of the device's JSON response; we accumulate until
        // JSON.parse succeeds, matching the web adapter.
        //
        // ble-plx occasionally fires the callback with "Operation was
        // cancelled" right after monitorCharacteristicForService
        // resolves — iOS GATT race where the registration isn't fully
        // settled. We retry the subscribe once on that specific error
        // before the settle window. Cancellations after the settle
        // (user disconnect, link drop) are normal and silent.
        const subscribe = (): void => {
            this.monitorSub = connected.monitorCharacteristicForService(
                NUS_SERVICE_UUID,
                NUS_TX_CHAR_UUID,
                (error: any, char: any) => {
                    if (error) {
                        const msg = String(error?.message ?? '');
                        if (msg.toLowerCase().includes('cancel')) {
                            // Suppress: either the retry below or the
                            // user-initiated disconnect cleared us.
                            return;
                        }
                        console.warn('[BLE] monitor error', msg);
                        return;
                    }
                    if (!char?.value) return;
                    this.handleChunk(decodeBase64(char.value));
                },
            );
        };
        subscribe();
        console.log('[BLE] monitor subscribed');

        // iOS GATT registers the notify subscription asynchronously on
        // the peripheral side; ble-plx resolves monitorCharacteristic*
        // before the firmware has actually started routing notifies to
        // us. The first request sent in that window drops its response.
        // 300ms is enough to settle on every device we tested; tune
        // higher only if first-request timeouts come back.
        await new Promise((r) => setTimeout(r, 300));

        // If the iOS stack cancelled the subscription during the
        // settle window, the user wouldn't notice — frames just never
        // arrive. Retry the subscribe once before signaling onConnect
        // so the app starts in a healthy state.
        if (!this.monitorSub?.isCancelled?.()) {
            // ble-plx Subscription has no public "isCancelled" — we
            // just re-subscribe unconditionally, cheap. The previous
            // sub's callback ignores "cancel" so the new one wins.
            try { this.monitorSub?.remove(); } catch { /* noop */ }
            subscribe();
            console.log('[BLE] monitor re-subscribed after settle');
        }

        this.device = connected;
        this.connected = true;
        this.startRssiPolling();
        this._onConnect?.();
    }

    private startRssiPolling(): void {
        this.stopRssiPolling();
        // Immediate read so the chip shows bars without the 3s delay.
        this.pollRssi();
        this.rssiTimer = setInterval(() => this.pollRssi(), 3_000);
    }

    private stopRssiPolling(): void {
        if (this.rssiTimer) { clearInterval(this.rssiTimer); this.rssiTimer = null; }
        this._onRssi?.(null);
    }

    private async pollRssi(): Promise<void> {
        if (!this.device || !this.connected) return;
        try {
            const d = await this.device.readRSSI();
            const rssi = typeof d?.rssi === 'number' ? d.rssi : null;
            this._onRssi?.(rssi);
        } catch {
            // Read failure is non-fatal; transient on iOS during link
            // power-save. Skip until next tick.
        }
    }

    /** Sim bridge path — line-framed JSON over a WebSocket, mirroring
     *  the web adapter's behaviour. The dev sim sends one complete
     *  JSON object per frame, so no chunk reassembly is needed. */
    private connectSim(url: string): Promise<void> {
        return new Promise((resolve, reject) => {
            const ws = new WebSocket(url);
            const timer = setTimeout(() => {
                try { ws.close(); } catch { /* noop */ }
                reject(new Error(`Sim timeout (${url})`));
            }, 5_000);

            ws.onopen = () => {
                clearTimeout(timer);
                this.simSocket = ws;
                this.connected = true;
                this._onConnect?.();
                resolve();
            };
            ws.onmessage = (e: any) => {
                try {
                    const data = JSON.parse(e.data);
                    this._onMessage?.(data);
                } catch {
                    console.warn('[SIM] bad JSON line:', e.data);
                }
            };
            ws.onclose = () => {
                clearTimeout(timer);
                this.simSocket = null;
                this.connected = false;
                this._onDisconnect?.();
            };
            ws.onerror = (_err: any) => {
                clearTimeout(timer);
                reject(new Error(`Sim unreachable (${url})`));
            };
        });
    }

    disconnect(): void {
        this.stopRssiPolling();
        if (this.simSocket) {
            try { this.simSocket.close(); } catch { /* noop */ }
            this.simSocket = null;
            this.connected = false;
            // onclose handler fires _onDisconnect; nothing else to do.
            return;
        }
        if (this.monitorSub) { try { this.monitorSub.remove(); } catch { /* noop */ } this.monitorSub = null; }
        if (this.connectSub) { try { this.connectSub.remove(); } catch { /* noop */ } this.connectSub = null; }
        if (this.device) {
            // `cancelConnection` returns a promise — fire and forget;
            // the onDisconnected callback above will clear our state.
            this.device.cancelConnection().catch(() => { /* noop */ });
        } else {
            // No active device yet: still signal disconnect so the UI
            // can return to the "Conectar" CTA.
            this._onDisconnect?.();
        }
        this.connected = false;
    }

    async send(message: object): Promise<void> {
        if (this.simSocket) {
            this.simSocket.send(JSON.stringify(message));
            return;
        }
        if (!this.connected || !this.device) {
            throw new Error('Not connected');
        }
        const json = JSON.stringify(message);
        const tp   = (message as any)?.tp ?? '(no tp)';
        console.log(`[BLE] tx ${tp} bytes=${json.length}`);

        // BLE MTU on Android negotiates around 185 bytes by default
        // (517 max); iOS sits at 185 too. We stay at 180 to leave
        // headroom — matches the firmware's per-chunk expectation and
        // is well under the 500 we use on web (chrome negotiates up).
        const chunkSize = 180;
        let chunkIdx = 0;
        for (let i = 0; i < json.length; i += chunkSize) {
            const chunk = json.slice(i, i + chunkSize);
            try {
                await this.device.writeCharacteristicWithoutResponseForService(
                    NUS_SERVICE_UUID,
                    NUS_RX_CHAR_UUID,
                    encodeBase64(chunk),
                );
                console.log(`[BLE] tx chunk ${chunkIdx} (${chunk.length}b) ok`);
            } catch (e: any) {
                console.warn(`[BLE] tx chunk ${chunkIdx} FAILED: ${e?.message ?? e}`);
                throw e;
            }
            chunkIdx++;
        }
    }

    onMessage(cb: (data: any) => void): void { this._onMessage = cb; }
    onConnect(cb: () => void): void { this._onConnect = cb; }
    onDisconnect(cb: () => void): void { this._onDisconnect = cb; }
    onRssi(cb: (rssi: number | null) => void): void { this._onRssi = cb; }

    getDeviceName(): string | null {
        if (this.simSocket) return 'Sim';
        return this.device?.name ?? this.device?.localName ?? null;
    }

    private handleChunk(text: string): void {
        this.rxBuffer += text;
        const { frames, remaining, parseErrors } = extractFrames(this.rxBuffer);
        this.rxBuffer = remaining;
        for (const data of frames) {
            console.log('[BLE] rx', data?.tp);
            this._onMessage?.(data);
        }
        for (const { error, frame } of parseErrors) {
            console.warn('[BLE] frame parse error', error, frame.slice(0, 60));
        }
        // Sanity guard against runaway buffers from a corrupted stream.
        if (this.rxBuffer.length > 10_000) {
            console.warn('[BLE] buffer overflow, resetting');
            this.rxBuffer = '';
        }
    }
}

export function createBleAdapter(): BleAdapter {
    return new NativeBleAdapter();
}
