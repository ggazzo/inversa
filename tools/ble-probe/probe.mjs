#!/usr/bin/env node
// ble-probe — real-BLE roundtrip check against the Inversa controller.
//
// Uses @abandonware/noble to drive macOS CoreBluetooth from Node. The
// firmware exposes a Nordic UART Service; we scan filtered by that
// UUID, connect to the first match, subscribe to the TX characteristic
// (notify: device→app) and write to the RX (app→device). Then runs a
// list of req:* commands and waits for matching res:* by rid.
//
// Usage:
//   cd tools/ble-probe && npm install      # first time
//   node probe.mjs                          # default suite
//   node probe.mjs req:info req:settings:get
//   node probe.mjs --timeout 15            # extend scan timeout
//
// macOS permissions: Terminal (or whichever shell you're in) needs
// "Bluetooth" access in System Settings → Privacy & Security. The
// first run pops the prompt; deny it and you'll see "Bluetooth state:
// unauthorized" until you grant it from settings and restart the
// shell.

import noble from '@abandonware/noble';

const NUS_SERVICE_UUID = '6e400001b5a3f393e0a9e50e24dcca9e';
const NUS_TX_CHAR_UUID = '6e400003b5a3f393e0a9e50e24dcca9e'; // notify: device → app
const NUS_RX_CHAR_UUID = '6e400002b5a3f393e0a9e50e24dcca9e'; // write:  app → device

// ── CLI args ─────────────────────────────────────────────────────
const args = process.argv.slice(2);
let scanTimeoutMs = 10_000;
let reqTimeoutMs  = 5_000;
const cmds = [];
for (let i = 0; i < args.length; i++) {
    if (args[i] === '--timeout') { scanTimeoutMs = (+args[++i]) * 1000; continue; }
    if (args[i] === '--req-timeout') { reqTimeoutMs = (+args[++i]) * 1000; continue; }
    cmds.push(args[i]);
}
if (cmds.length === 0) cmds.push('req:info', 'req:settings:get', 'req:recipe:list');

function pad(s, n) { return (s + ' '.repeat(n)).slice(0, n); }

// ── Brace-depth frame extractor (same algorithm as the RN adapter) ─
function extractFrames(buffer) {
    const frames = [];
    const errors = [];
    let depth = 0, inString = false, escape = false, frameStart = -1, consumedTo = 0;
    for (let i = 0; i < buffer.length; i++) {
        const ch = buffer[i];
        if (escape) { escape = false; continue; }
        if (inString) {
            if (ch === '\\') escape = true;
            else if (ch === '"') inString = false;
            continue;
        }
        if (ch === '"') { inString = true; continue; }
        if (ch === '{') { if (depth === 0) frameStart = i; depth++; }
        else if (ch === '}') {
            depth--;
            if (depth === 0 && frameStart >= 0) {
                const frame = buffer.slice(frameStart, i + 1);
                try { frames.push(JSON.parse(frame)); }
                catch (e) { errors.push({ frame, error: e.message }); }
                consumedTo = i + 1;
                frameStart = -1;
            }
        }
    }
    return { frames, remaining: buffer.slice(consumedTo), errors };
}

// ── Pairing + connect ───────────────────────────────────────────
let peripheral, rxChar, txChar;
let rxBuffer = '';
const pending = new Map();
let nextRid = 1;
let sawStatus = false;

noble.on('stateChange', async (state) => {
    console.log(`[ble-probe] Bluetooth state: ${state}`);
    if (state !== 'poweredOn') {
        if (state === 'unauthorized') {
            console.error('[ble-probe] grant Terminal Bluetooth access in System Settings → Privacy.');
        }
        process.exit(2);
    }
    console.log(`[ble-probe] scanning (filter: NUS service, ${scanTimeoutMs / 1000}s timeout)`);
    await noble.startScanningAsync([NUS_SERVICE_UUID], false);

    setTimeout(async () => {
        if (!peripheral) {
            await noble.stopScanningAsync();
            console.error('[ble-probe] no Inversa device found.');
            process.exit(2);
        }
    }, scanTimeoutMs);
});

noble.on('discover', async (p) => {
    peripheral = p;
    await noble.stopScanningAsync();
    console.log(`[ble-probe] found ${p.advertisement.localName || p.id} rssi=${p.rssi}dBm`);

    p.once('disconnect', () => {
        console.log('[ble-probe] disconnected.');
        process.exit(0);
    });

    try {
        await p.connectAsync();
        console.log('[ble-probe] connected, discovering services…');

        const { characteristics } = await p.discoverSomeServicesAndCharacteristicsAsync(
            [NUS_SERVICE_UUID],
            [NUS_TX_CHAR_UUID, NUS_RX_CHAR_UUID],
        );
        for (const c of characteristics) {
            if (c.uuid === NUS_TX_CHAR_UUID) txChar = c;
            else if (c.uuid === NUS_RX_CHAR_UUID) rxChar = c;
        }
        if (!txChar || !rxChar) throw new Error('NUS characteristics missing');

        txChar.on('data', (buf) => {
            rxBuffer += buf.toString('utf8');
            const { frames, remaining } = extractFrames(rxBuffer);
            rxBuffer = remaining;
            for (const data of frames) {
                if (data.rid && pending.has(data.rid)) {
                    const slot = pending.get(data.rid);
                    pending.delete(data.rid);
                    clearTimeout(slot.timer);
                    if (data.tp === 'res:error') slot.reject(new Error(data.err || 'res:error'));
                    else                          slot.resolve(data);
                } else if (data.tp === 'evt:status') {
                    if (!sawStatus) {
                        sawStatus = true;
                        console.log(`[ble-probe] · evt:status (further hidden) ct=${data.ct} tt=${data.tt}`);
                    }
                } else {
                    console.log(`[ble-probe] · ${data.tp ?? '(unknown)'}`);
                }
            }
        });
        await txChar.subscribeAsync();
        console.log('[ble-probe] subscribed to TX (notify)');

        await runSuite();
        await p.disconnectAsync();
    } catch (e) {
        console.error('[ble-probe] error:', e?.message ?? e);
        try { await p.disconnectAsync(); } catch { /* noop */ }
        process.exit(1);
    }
});

async function send(type) {
    return new Promise((resolve, reject) => {
        const rid = `b${nextRid++}`;
        const timer = setTimeout(() => {
            pending.delete(rid);
            reject(new Error(`timeout (${reqTimeoutMs}ms)`));
        }, reqTimeoutMs);
        pending.set(rid, { resolve, reject, timer });

        const payload = Buffer.from(JSON.stringify({ tp: type, rid }), 'utf8');
        // MTU on macOS CoreBluetooth defaults around 185 bytes; firmware
        // expects chunks under that. Chunk at 180 to match the RN adapter.
        const chunkSize = 180;
        (async () => {
            for (let i = 0; i < payload.length; i += chunkSize) {
                await rxChar.writeAsync(payload.subarray(i, i + chunkSize), true /* withoutResponse */);
            }
        })().catch(reject);
    });
}

async function runSuite() {
    let failed = 0;
    for (const cmd of cmds) {
        const t0 = Date.now();
        try {
            const res = await send(cmd);
            const ms = Date.now() - t0;
            const preview = JSON.stringify(res).slice(0, 80);
            console.log(`[ble-probe] ✓ ${pad(cmd, 24)} ${pad(ms + 'ms', 7)} ${preview}`);
        } catch (e) {
            failed++;
            console.error(`[ble-probe] ✗ ${pad(cmd, 24)}  ${e.message}`);
        }
    }
    process.exitCode = failed === 0 ? 0 : 1;
}

process.on('SIGINT', async () => {
    if (peripheral) { try { await peripheral.disconnectAsync(); } catch { /* noop */ } }
    process.exit(130);
});
