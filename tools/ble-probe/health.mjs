#!/usr/bin/env node
// health.mjs — board health battery for the BrewPilot controller.
//
// Connects over real BLE, runs a structured set of probes against the
// firmware's req:* surface, and reports per-test pass/fail. Designed
// to answer "is this board healthy?" before / after flashing.
//
// Usage:
//   node tools/ble-probe/health.mjs              # safe defaults (read-only)
//   node tools/ble-probe/health.mjs --full       # adds heater/pump toggle tests
//   node tools/ble-probe/health.mjs --telemetry-secs 10
//   node tools/ble-probe/health.mjs --skip telemetry,chunked-notify
//
// Exit codes: 0 = every test passed, 1 = at least one failure,
// 2 = couldn't even reach the board (BLE off, scan timed out, etc.).
//
// macOS permission note: same as probe.mjs — Terminal needs Bluetooth
// access in System Settings → Privacy & Security.

import noble from '@abandonware/noble';

const NUS_SERVICE_UUID = '6e400001b5a3f393e0a9e50e24dcca9e';
const NUS_TX_CHAR_UUID = '6e400003b5a3f393e0a9e50e24dcca9e';
const NUS_RX_CHAR_UUID = '6e400002b5a3f393e0a9e50e24dcca9e';

// ── CLI ─────────────────────────────────────────────────────────
const args = process.argv.slice(2);
let scanTimeoutMs = 10_000;
let reqTimeoutMs  = 5_000;
let runFull       = false;
let telemetrySecs = 5;
let skip          = new Set();
for (let i = 0; i < args.length; i++) {
    if (args[i] === '--timeout')        { scanTimeoutMs = (+args[++i]) * 1000; continue; }
    if (args[i] === '--req-timeout')    { reqTimeoutMs  = (+args[++i]) * 1000; continue; }
    if (args[i] === '--telemetry-secs') { telemetrySecs =  +args[++i] || 5;    continue; }
    if (args[i] === '--full')           { runFull = true;                       continue; }
    if (args[i] === '--skip')           { skip = new Set((args[++i] || '').split(',').filter(Boolean)); continue; }
    if (args[i] === '--help' || args[i] === '-h') {
        console.log(`Usage: health.mjs [--full] [--telemetry-secs N] [--skip id1,id2] [--timeout S]`);
        process.exit(0);
    }
}

// ── Test catalog ────────────────────────────────────────────────
// Each test returns { pass, msg, details? }. Read-only tests run by
// default; write tests (`destructive: true`) require --full because
// they toggle physical outputs.
const TESTS = [
    { id: 'info',             desc: 'firmware identifies itself, heap healthy',     fn: testInfo },
    { id: 'rtc',              desc: 'RTC returns a plausible Unix timestamp',       fn: testRtc },
    { id: 'wifi-status',      desc: 'WiFi plugin responds to req:wifi:status',      fn: testWifiStatus },
    { id: 'settings-pid',     desc: 'PID settings readable',                        fn: testSettingsPid },
    { id: 'settings-thermal', desc: 'thermal params (P13) readable',                fn: testSettingsThermal },
    { id: 'settings-cal',     desc: 'temperature calibration readable',             fn: testSettingsCal },
    { id: 'sd-list',          desc: 'recipe list responds',                         fn: testSdList },
    { id: 'sd-roundtrip',     desc: 'recipe save → list → load → delete roundtrip', fn: testSdRoundtrip },
    { id: 'chunked-notify',   desc: 'large response survives chunked notify',       fn: testChunkedNotify },
    { id: 'telemetry',        desc: 'evt:status arriving + ct in 0..100 °C',        fn: testTelemetry },
    { id: 'heap-stable',      desc: 'no heap regression across suite',              fn: testHeapStable, runLast: true },
    { id: 'heater',           desc: 'heater toggle reflected in next status',       fn: testHeater, destructive: true },
    { id: 'pump',             desc: 'pump toggle reflected in next status',         fn: testPump,   destructive: true },
];

// ── BLE plumbing — copied from probe.mjs with frame extractor ───
let peripheral, rxChar, txChar;
let rxBuffer = '';
const pending = new Map();
let nextRid = 1;
const telemetryFrames = [];
let captureTelemetry = false;
// Generic "wait for the next event of type X" hook. WiFi status (and
// any other req:* that answers with a separate evt:*) uses this.
const evtWaiters = new Map();  // tp -> { resolve, timer }

function extractFrames(buffer) {
    const frames = [];
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
                try { frames.push(JSON.parse(frame)); } catch { /* dropped — bad json */ }
                consumedTo = i + 1;
                frameStart = -1;
            }
        }
    }
    return { frames, remaining: buffer.slice(consumedTo) };
}

async function send(type, extra = {}) {
    return new Promise((resolve, reject) => {
        const rid = `h${nextRid++}`;
        const timer = setTimeout(() => {
            pending.delete(rid);
            reject(new Error(`timeout ${reqTimeoutMs}ms`));
        }, reqTimeoutMs);
        pending.set(rid, { resolve, reject, timer });
        const payload = Buffer.from(JSON.stringify({ tp: type, rid, ...extra }), 'utf8');
        const chunkSize = 180;
        (async () => {
            for (let i = 0; i < payload.length; i += chunkSize) {
                await rxChar.writeAsync(payload.subarray(i, i + chunkSize), true);
            }
        })().catch(reject);
    });
}

function isNum(x)       { return typeof x === 'number' && Number.isFinite(x); }
function inRange(x, lo, hi) { return isNum(x) && x >= lo && x <= hi; }

// ── Test implementations ────────────────────────────────────────
let firstHeap = null;

async function testInfo() {
    const r = await send('req:info');
    if (r.tp !== 'res:info') return { pass: false, msg: `tp=${r.tp}` };
    // Set the heap baseline as long as we got a number, so a tight
    // free-heap reading doesn't poison the later heap-stable test.
    if (isNum(r.heap)) firstHeap = r.heap;
    // 15 KB is the "won't OOM on the next big BLE notify burst"
    // threshold. NimBLE TX mbufs + ArduinoJson scratch fit there;
    // anything under that is a red flag. Healthy idle ≈ 24 KB.
    const ok = typeof r.fw === 'string' && r.fw.length > 0
            && typeof r.build === 'string'
            && isNum(r.heap) && r.heap > 15_000;
    if (!ok) return { pass: false, msg: `fw=${r.fw} build=${r.build} heap=${r.heap}` };
    return { pass: true, msg: `fw=${r.fw} build=${r.build} heap=${(r.heap / 1024).toFixed(1)}KB` };
}

async function testRtc() {
    const r = await send('req:rtc:get');
    // Accept anything from 2024-01-01 onward. Below that the RTC
    // didn't sync.
    const min = 1_704_067_200;  // 2024-01-01 UTC
    if (!isNum(r.ts) || r.ts < min) {
        return { pass: false, msg: `ts=${r.ts ?? 'absent'} (rtc not synced?)` };
    }
    const iso = new Date(r.ts * 1000).toISOString();
    return { pass: true, msg: `ts=${r.ts} (${iso})` };
}

async function testWifiStatus() {
    // `req:wifi:status` returns res:ok and *separately* fires
    // evt:wifi:status with the actual fields (conn/ssid/ip/cfg). Set
    // up the waiter before sending so the event can't race the ack.
    const evtP = waitForEvent('evt:wifi:status', 2_000);
    const r    = await send('req:wifi:status');
    if (r.tp === 'res:error') return { pass: false, msg: r.err };
    let evt;
    try { evt = await evtP; }
    catch (e) { return { pass: false, msg: `ack ok but evt:wifi:status never arrived: ${e.message}` }; }
    if (typeof evt.conn !== 'boolean') {
        return { pass: false, msg: `evt:wifi:status missing 'conn' boolean (got ${JSON.stringify(evt)})` };
    }
    const ssid = evt.ssid || evt.cfg || '-';
    return { pass: true, msg: `conn=${evt.conn} ssid=${ssid}${evt.ip ? ' ip=' + evt.ip : ''}` };
}

function waitForEvent(tp, timeoutMs) {
    return new Promise((resolve, reject) => {
        const timer = setTimeout(() => {
            evtWaiters.delete(tp);
            reject(new Error(`timeout ${timeoutMs}ms`));
        }, timeoutMs);
        evtWaiters.set(tp, { resolve, timer });
    });
}

async function testSettingsPid() {
    const r = await send('req:settings:get');
    if (!isNum(r.kp) || !isNum(r.ki) || !isNum(r.kd)) {
        return { pass: false, msg: `kp=${r.kp} ki=${r.ki} kd=${r.kd}` };
    }
    if (r.kp < 0 || r.ki < 0 || r.kd < 0) {
        return { pass: false, msg: `negative gain — kp=${r.kp} ki=${r.ki} kd=${r.kd}` };
    }
    return { pass: true, msg: `kp=${r.kp} ki=${r.ki} kd=${r.kd}` };
}

async function testSettingsThermal() {
    const r = await send('req:settings:thermal:get');
    const fields = ['volumeL', 'powerW', 'ambientC', 'diameterM', 'lossCoeff'];
    for (const f of fields) {
        if (!isNum(r[f])) return { pass: false, msg: `missing/invalid ${f}` };
    }
    return { pass: true, msg: `vol=${r.volumeL}L pwr=${r.powerW}W amb=${r.ambientC}°C persisted=${!!r.persisted}` };
}

async function testSettingsCal() {
    const r = await send('req:settings:cal:get');
    if (!inRange(r.slope, 0.5, 1.5) || !inRange(r.offset, -20, 20)) {
        return { pass: false, msg: `out of sane range slope=${r.slope} offset=${r.offset}` };
    }
    return { pass: true, msg: `slope=${r.slope} offset=${r.offset}°C persisted=${!!r.persisted}` };
}

async function testSdList() {
    const r = await send('req:recipe:list');
    if (!Array.isArray(r.recipes)) return { pass: false, msg: `recipes=${JSON.stringify(r.recipes)}` };
    return { pass: true, msg: `${r.recipes.length} recipe(s) on SD` };
}

const TEST_FILE = '_health.txt';
async function testSdRoundtrip() {
    const content = '# health probe — safe to delete\nSET_TEMP 50\nWAIT_TEMP\n';
    const save = await send('req:recipe:save', { file: TEST_FILE, content });
    if (save.tp !== 'res:ok') return { pass: false, msg: `save ${save.tp}` };

    const list = await send('req:recipe:list');
    if (!Array.isArray(list.recipes) || !list.recipes.includes(TEST_FILE)) {
        return { pass: false, msg: `list missing test file (recipes=${JSON.stringify(list.recipes)})` };
    }

    const load = await send('req:recipe:load', { file: TEST_FILE });
    if (load.content !== content) {
        return { pass: false, msg: `content mismatch (got ${load.content?.length} bytes, expected ${content.length})` };
    }

    const del = await send('req:recipe:delete', { file: TEST_FILE });
    if (del.tp !== 'res:ok') return { pass: false, msg: `delete ${del.tp}` };

    return { pass: true, msg: `${content.length} bytes roundtrip + cleanup` };
}

async function testChunkedNotify() {
    // ~3 KB content — guaranteed to exceed even an MTU=185 single
    // frame, forcing the chunked-notify path.
    const filler = 'SET_TEMP 60\nWAIT_TEMP\nHOLD 5\n'.repeat(100);
    const file = '_health_big.txt';
    const t0 = Date.now();
    await send('req:recipe:save', { file, content: filler });
    const load = await send('req:recipe:load', { file });
    const ms = Date.now() - t0;
    await send('req:recipe:delete', { file });

    if (load.content !== filler) {
        return { pass: false, msg: `roundtrip corrupted at ${filler.length} bytes` };
    }
    return { pass: true, msg: `${filler.length} bytes roundtripped in ${ms}ms` };
}

async function testTelemetry() {
    telemetryFrames.length = 0;
    captureTelemetry = true;
    await new Promise((r) => setTimeout(r, telemetrySecs * 1000));
    captureTelemetry = false;

    const frames = telemetryFrames.slice();
    if (frames.length === 0) {
        return { pass: false, msg: 'no evt:status received' };
    }
    const withCt = frames.filter(f => isNum(f.ct));
    if (withCt.length === 0) {
        return { pass: false, msg: `${frames.length} frame(s) but none had ct` };
    }
    const cts = withCt.map(f => f.ct);
    const minC = Math.min(...cts), maxC = Math.max(...cts);
    const inSane = cts.every(c => c >= -10 && c <= 130);
    if (!inSane) {
        return { pass: false, msg: `ct out of -10..130°C (min=${minC} max=${maxC})` };
    }
    const rate = frames.length / telemetrySecs;
    if (rate < 0.5) {
        return { pass: false, msg: `rate=${rate.toFixed(2)} Hz (expected ≥0.5)` };
    }
    return { pass: true, msg: `${frames.length} frames in ${telemetrySecs}s · rate ${rate.toFixed(1)}Hz · ct ${minC.toFixed(2)}…${maxC.toFixed(2)}°C` };
}

async function testHeapStable() {
    if (firstHeap === null) return { pass: false, msg: 'baseline heap missing (testInfo skipped?)' };
    const r = await send('req:info');
    const delta = r.heap - firstHeap;
    // 4KB drift tolerance — covers JsonDocument churn and BLE
    // transient allocations. Anything beyond that is suspicious.
    if (delta < -4096) {
        return { pass: false, msg: `heap shrank by ${(-delta / 1024).toFixed(1)}KB during suite (leak?)` };
    }
    return { pass: true, msg: `Δ=${(delta / 1024).toFixed(1)}KB (start ${(firstHeap / 1024).toFixed(1)}KB → now ${(r.heap / 1024).toFixed(1)}KB)` };
}

async function awaitNextStatus(predicate, maxMs = 3000) {
    const start = Date.now();
    while (Date.now() - start < maxMs) {
        const last = telemetryFrames[telemetryFrames.length - 1];
        if (last && predicate(last)) return last;
        await new Promise(r => setTimeout(r, 100));
    }
    return null;
}

async function testHeater() {
    captureTelemetry = true;
    telemetryFrames.length = 0;
    try {
        await send('req:heater:on');
        const on = await awaitNextStatus(s => s.h === true);
        if (!on) return { pass: false, msg: 'heater never reported h=true in telemetry' };
        await send('req:heater:off');
        const off = await awaitNextStatus(s => s.h === false);
        if (!off) return { pass: false, msg: 'heater never reported h=false after off' };
        return { pass: true, msg: 'h=true → h=false confirmed via telemetry' };
    } finally {
        // Always send off, even on failure, so the heater can't be
        // left on by a half-run test.
        try { await send('req:heater:off'); } catch { /* noop */ }
        captureTelemetry = false;
    }
}

async function testPump() {
    captureTelemetry = true;
    telemetryFrames.length = 0;
    try {
        await send('req:pump:on');
        const on = await awaitNextStatus(s => s.p === true);
        if (!on) return { pass: false, msg: 'pump never reported p=true in telemetry' };
        await send('req:pump:off');
        const off = await awaitNextStatus(s => s.p === false);
        if (!off) return { pass: false, msg: 'pump never reported p=false after off' };
        return { pass: true, msg: 'p=true → p=false confirmed via telemetry' };
    } finally {
        try { await send('req:pump:off'); } catch { /* noop */ }
        captureTelemetry = false;
    }
}

// ── Suite driver ────────────────────────────────────────────────
function pad(s, n) { return (s + ' '.repeat(n)).slice(0, n); }

async function runSuite() {
    const RED = '\x1b[31m', GREEN = '\x1b[32m', DIM = '\x1b[2m', RESET = '\x1b[0m';
    const selected = TESTS.filter(t => {
        if (skip.has(t.id))               return false;
        if (t.destructive && !runFull)    return false;
        return true;
    });
    // testInfo must come first (baselines heap). testHeapStable last.
    selected.sort((a, b) => (a.id === 'info' ? -1 : b.id === 'info' ? 1 : 0)
                          + (a.runLast ? 1 : b.runLast ? -1 : 0));

    console.log(`[health] running ${selected.length} test(s)${runFull ? ' (--full)' : ''}\n`);
    let failed = 0;
    for (const t of selected) {
        process.stdout.write(`  ${pad(t.id, 18)} `);
        const t0 = Date.now();
        try {
            const r = await t.fn();
            const ms = Date.now() - t0;
            if (r.pass) {
                console.log(`${GREEN}✓${RESET} ${pad(t.desc, 50)} ${DIM}${ms}ms${RESET} ${DIM}${r.msg ?? ''}${RESET}`);
            } else {
                console.log(`${RED}✗${RESET} ${pad(t.desc, 50)} ${DIM}${ms}ms${RESET} ${RED}${r.msg}${RESET}`);
                failed++;
            }
        } catch (e) {
            console.log(`${RED}✗${RESET} ${pad(t.desc, 50)} ${RED}${e.message}${RESET}`);
            failed++;
        }
    }
    console.log('');
    if (failed === 0) {
        console.log(`${GREEN}[health] PASS${RESET} — ${selected.length} test(s) green`);
        process.exitCode = 0;
    } else {
        console.log(`${RED}[health] FAIL${RESET} — ${failed}/${selected.length} test(s) failed`);
        process.exitCode = 1;
    }
}

// ── BLE bootstrap ───────────────────────────────────────────────
noble.on('stateChange', async (state) => {
    console.log(`[health] Bluetooth: ${state}`);
    if (state !== 'poweredOn') {
        if (state === 'unauthorized') {
            console.error('[health] grant Bluetooth access in System Settings → Privacy.');
        }
        process.exit(2);
    }
    console.log(`[health] scanning for BrewPilot (NUS service, ${scanTimeoutMs / 1000}s)…`);
    await noble.startScanningAsync([NUS_SERVICE_UUID], false);
    setTimeout(async () => {
        if (!peripheral) {
            await noble.stopScanningAsync();
            console.error('[health] no BrewPilot device found.');
            process.exit(2);
        }
    }, scanTimeoutMs);
});

noble.on('discover', async (p) => {
    peripheral = p;
    await noble.stopScanningAsync();
    console.log(`[health] found ${p.advertisement.localName || p.id} rssi=${p.rssi}dBm`);

    p.once('disconnect', () => {
        console.log('[health] disconnected.');
        if (process.exitCode === undefined) process.exitCode = 0;
        process.exit(process.exitCode);
    });

    try {
        await p.connectAsync();
        const { characteristics } = await p.discoverSomeServicesAndCharacteristicsAsync(
            [NUS_SERVICE_UUID], [NUS_TX_CHAR_UUID, NUS_RX_CHAR_UUID]
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
                } else if (data.tp === 'evt:status' && captureTelemetry) {
                    telemetryFrames.push(data);
                } else if (data.tp && evtWaiters.has(data.tp)) {
                    const w = evtWaiters.get(data.tp);
                    evtWaiters.delete(data.tp);
                    clearTimeout(w.timer);
                    w.resolve(data);
                }
            }
        });
        await txChar.subscribeAsync();
        // Settle the notify subscription (iOS GATT registers a tick
        // after subscribeAsync resolves) before sending anything.
        await new Promise((r) => setTimeout(r, 300));

        await runSuite();
        await p.disconnectAsync();
    } catch (e) {
        console.error('[health] fatal:', e?.message ?? e);
        try { await p.disconnectAsync(); } catch { /* noop */ }
        process.exit(1);
    }
});

process.on('SIGINT', async () => {
    if (peripheral) { try { await peripheral.disconnectAsync(); } catch { /* noop */ } }
    process.exit(130);
});
