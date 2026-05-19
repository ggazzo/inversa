// @ts-nocheck — Web Bluetooth + WebSocket. The Web Bluetooth API is
// browser-only and untyped in our setup; the legacy BLEService.ts that
// this file replaces already lived under @ts-nocheck. Keeping it here
// avoids manufacturing types we'd only use in one place.
//
// BleAdapter.web.ts — implementation for the browser. Vite picks this
// up over BleAdapter.ts because `resolve.extensions` is configured to
// prefer `.web.ts`. Two transports live behind one switch:
//
//   * Real BLE (`navigator.bluetooth.requestDevice`) — Nordic UART
//     Service, chunked writes, accumulator on the notify path because
//     a single JSON message can span several characteristic frames.
//
//   * Sim bridge (WebSocket, selected by `?sim=ws://localhost:8765`)
//     — line-framed JSON, one frame per message. No re-assembly.
//
// The rid/request-correlation layer used to live here too; it now
// belongs to BleClient (portable, runs above any transport).

import type { BleAdapter, BleScanDevice } from './BleAdapter';

const NUS_SERVICE_UUID = '6e400001-b5a3-f393-e0a9-e50e24dcca9e';
const NUS_TX_CHAR_UUID = '6e400003-b5a3-f393-e0a9-e50e24dcca9e'; // notify: device → app
const NUS_RX_CHAR_UUID = '6e400002-b5a3-f393-e0a9-e50e24dcca9e'; // write:  app → device

class WebBleAdapter implements BleAdapter {
    device:        any = null;
    server:        any = null;
    txChar:        any = null;
    rxChar:        any = null;
    connected:     boolean = false;
    _onMessage:    ((data: any) => void) | null = null;
    _onConnect:    (() => void) | null = null;
    _onDisconnect: (() => void) | null = null;
    _rxBuffer:     string = '';
    _simSocket:    WebSocket | null = null;

    isSupported(): boolean {
        return !!navigator.bluetooth || this._simUrl() !== null;
    }

    /** Web's BLE picker lives inside `navigator.bluetooth.requestDevice`
     *  — the browser draws it. So we never need an in-app picker. */
    needsPicker(): boolean { return false; }

    /** No-op on web: the browser's `requestDevice` picker doesn't
     *  expose a scan stream to us. Kept so the BleAdapter interface
     *  is satisfied; UI calls connect() directly on web. */
    startScan(_onDevice: (d: BleScanDevice) => void, _onError?: (e: Error) => void): () => void {
        return () => { /* noop */ };
    }

    async connectToDevice(_id: string): Promise<void> {
        // Web doesn't expose a scan-then-connect-by-id flow — connect()
        // pops the browser picker which already lets the user choose.
        return this.connect();
    }

    _simUrl(): string | null {
        if (typeof location === 'undefined') return null;
        return new URLSearchParams(location.search).get('sim');
    }

    async connect(): Promise<void> {
        const simUrl = this._simUrl();
        if (simUrl) return this._connectSim(simUrl);

        if (!this.isSupported()) {
            throw new Error('Web Bluetooth is not supported in this browser');
        }

        try {
            this.device = await navigator.bluetooth.requestDevice({
                filters: [{ services: [NUS_SERVICE_UUID] }],
                optionalServices: [NUS_SERVICE_UUID],
            });

            this.device.addEventListener('gattserverdisconnected', () => {
                this.connected = false;
                this._onDisconnect?.();
            });

            this.server  = await this.device.gatt.connect();
            const service = await this.server.getPrimaryService(NUS_SERVICE_UUID);
            this.txChar = await service.getCharacteristic(NUS_TX_CHAR_UUID);
            this.rxChar = await service.getCharacteristic(NUS_RX_CHAR_UUID);

            await this.txChar.startNotifications();
            this.txChar.addEventListener('characteristicvaluechanged', (e: any) => {
                this._handleNotification(e.target.value);
            });

            this.connected = true;
            this._onConnect?.();
        } catch (err) {
            console.error('[BLE] Connection failed:', err);
            throw err;
        }
    }

    disconnect(): void {
        if (this._simSocket) {
            try { this._simSocket.close(); } catch { /* noop */ }
            this._simSocket = null;
        } else if (this.device?.gatt?.connected) {
            this.device.gatt.disconnect();
        }
        this.connected = false;
        this._onDisconnect?.();
    }

    _connectSim(url: string): Promise<void> {
        return new Promise((resolve, reject) => {
            const ws = new WebSocket(url);
            const timer = setTimeout(() => reject(new Error('Sim connection timeout')), 5000);

            ws.addEventListener('open', () => {
                clearTimeout(timer);
                this._simSocket = ws;
                this.connected = true;
                this._onConnect?.();
                resolve();
            });
            ws.addEventListener('message', (e: any) => {
                // Each WS frame is one complete JSON line; no accumulation.
                try {
                    const data = JSON.parse(e.data);
                    this._onMessage?.(data);
                } catch {
                    console.warn('[SIM] bad JSON line:', e.data);
                }
            });
            ws.addEventListener('close', () => {
                clearTimeout(timer);
                this._simSocket = null;
                this.connected = false;
                this._onDisconnect?.();
            });
            ws.addEventListener('error', (err: any) => {
                clearTimeout(timer);
                reject(err);
            });
        });
    }

    async send(message: object): Promise<void> {
        if (this._simSocket) {
            this._simSocket.send(JSON.stringify(message));
            return;
        }
        if (!this.connected || !this.rxChar) {
            throw new Error('Not connected');
        }

        const json    = JSON.stringify(message);
        const encoded = new TextEncoder().encode(json);

        // BLE MTU caps each write at ~500 bytes on this firmware. Split
        // large frames; the device's UART side reassembles by JSON.
        const chunkSize = 500;
        for (let i = 0; i < encoded.length; i += chunkSize) {
            const chunk = encoded.slice(i, i + chunkSize);
            await this.rxChar.writeValueWithoutResponse(chunk);
        }
    }

    onMessage(cb: (data: any) => void): void { this._onMessage = cb; }
    onConnect(cb: () => void): void { this._onConnect = cb; }
    onDisconnect(cb: () => void): void { this._onDisconnect = cb; }
    // Web Bluetooth doesn't expose RSSI; signal stays null on web.
    onRssi(_cb: (rssi: number | null) => void): void { /* noop */ }

    getDeviceName(): string | null {
        return this.device?.name ?? null;
    }

    _handleNotification(dataView: DataView): void {
        const chunk = new TextDecoder().decode(dataView);
        this._rxBuffer += chunk;

        // Try to parse; if the JSON is incomplete, keep accumulating.
        try {
            const data = JSON.parse(this._rxBuffer);
            this._rxBuffer = '';
            this._onMessage?.(data);
        } catch {
            // Sanity guard: drop the buffer if a malformed stream piles up.
            if (this._rxBuffer.length > 10_000) {
                console.warn('[BLE] Buffer overflow, resetting');
                this._rxBuffer = '';
            }
        }
    }
}

export function createBleAdapter(): BleAdapter {
    return new WebBleAdapter();
}
