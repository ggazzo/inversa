// BleClient.ts — platform-agnostic wrapper around BleAdapter.
//
// Adds the bits that aren't transport-specific:
//
//   1. rid-correlated request/response (`request(type, data)` resolves
//      when the device echoes back the same rid, or rejects on timeout
//      / `res:error`).
//   2. A single subscription point for ConnectionManager to wire up;
//      the adapter underneath stays swappable.
//
// The adapter is created via `createBleAdapter()` from `./BleAdapter`,
// which is the typed fallback by default but gets replaced at bundle
// time with BleAdapter.web.ts (Vite) or BleAdapter.native.ts (Metro)
// via extension resolution. TypeScript only ever sees the interface
// declared in BleAdapter.ts, which keeps the type-checker happy
// without forcing per-platform .d.ts shims.

import { createBleAdapter, type BleAdapter, type BleScanDevice } from './BleAdapter';

export type { BleScanDevice };

class BleClientClass {
    private adapter: BleAdapter;
    private requestMap = new Map<string, (response: any) => void>();
    private requestId = 0;
    private _onMessage: ((data: any) => void) | null = null;
    private _onConnect: (() => void) | null = null;
    private _onDisconnect: (() => void) | null = null;

    constructor() {
        this.adapter = createBleAdapter();

        // The adapter delivers parsed messages; we peel off responses
        // that match a pending rid before forwarding to the consumer.
        this.adapter.onMessage((data) => {
            if (data.rid && this.requestMap.has(data.rid)) {
                this.requestMap.get(data.rid)!(data);
            }
            this._onMessage?.(data);
        });

        this.adapter.onConnect(() => this._onConnect?.());
        this.adapter.onDisconnect(() => this._onDisconnect?.());
    }

    isSupported(): boolean { return this.adapter.isSupported(); }
    needsPicker(): boolean { return this.adapter.needsPicker(); }
    async connect(): Promise<void> { return this.adapter.connect(); }
    startScan(onDevice: (d: BleScanDevice) => void, onError?: (e: Error) => void): () => void {
        return this.adapter.startScan(onDevice, onError);
    }
    async connectToDevice(id: string): Promise<void> { return this.adapter.connectToDevice(id); }
    disconnect(): void { this.adapter.disconnect(); }
    async send(message: object): Promise<void> { return this.adapter.send(message); }

    /** Send a request and wait for the matching response. Rejects on
     *  timeout or when the device replies with `res:error`. */
    async request(type: string, data: object = {}, timeoutMs: number = 5000): Promise<any> {
        const rid = `r${++this.requestId}`;
        const message = { tp: type, rid, ...data };

        return new Promise((resolve, reject) => {
            const timer = setTimeout(() => {
                this.requestMap.delete(rid);
                reject(new Error(`Request timeout: ${type}`));
            }, timeoutMs);

            this.requestMap.set(rid, (response) => {
                clearTimeout(timer);
                this.requestMap.delete(rid);
                if (response.tp === 'res:error') {
                    reject(new Error(response.err || 'Unknown error'));
                } else {
                    resolve(response);
                }
            });

            this.adapter.send(message).catch((err) => {
                clearTimeout(timer);
                this.requestMap.delete(rid);
                reject(err);
            });
        });
    }

    onMessage(cb: (data: any) => void): void { this._onMessage = cb; }
    onConnect(cb: () => void): void { this._onConnect = cb; }
    onDisconnect(cb: () => void): void { this._onDisconnect = cb; }

    getDeviceName(): string | null { return this.adapter.getDeviceName(); }
}

export const BleClient = new BleClientClass();
