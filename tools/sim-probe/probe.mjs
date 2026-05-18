#!/usr/bin/env node
// sim-probe — end-to-end protocol roundtrip check against the sim bridge.
//
// Connects to ws://localhost:8765 (or --url), spawns the firmware_sim
// behind the bridge, sends a list of req:* commands, waits for each
// matching res:* (by rid), and prints pass/fail per command. Exits
// non-zero on first timeout / error.
//
// Usage:
//   node tools/sim-probe/probe.mjs                    # default suite
//   node tools/sim-probe/probe.mjs req:info req:settings:get
//   node tools/sim-probe/probe.mjs --url ws://localhost:8765 req:info
//
// The bridge must be running first:
//   cd tools/sim-bridge && npm start

const args = process.argv.slice(2);
let url = 'ws://localhost:8765';
const cmds = [];
const reqTimeoutMs = 5_000;

for (let i = 0; i < args.length; i++) {
    if (args[i] === '--url') { url = args[++i]; continue; }
    cmds.push(args[i]);
}
if (cmds.length === 0) {
    // Default suite: cheap reads only, no state mutation.
    cmds.push('req:info', 'req:settings:get', 'req:recipe:list');
}

function pad(s, n) { return (s + ' '.repeat(n)).slice(0, n); }

const ws = new WebSocket(url);
const pending = new Map();   // rid -> { resolve, reject, timer, type }
let nextRid = 1;

function send(type) {
    return new Promise((resolve, reject) => {
        const rid = `p${nextRid++}`;
        const timer = setTimeout(() => {
            pending.delete(rid);
            reject(new Error(`timeout (${reqTimeoutMs}ms)`));
        }, reqTimeoutMs);
        pending.set(rid, { resolve, reject, timer, type });
        ws.send(JSON.stringify({ tp: type, rid }));
    });
}

ws.addEventListener('open', async () => {
    console.log(`[probe] connected ${url}`);
    let failed = 0;
    for (const cmd of cmds) {
        const t0 = Date.now();
        try {
            const res = await send(cmd);
            const ms = Date.now() - t0;
            const preview = JSON.stringify(res).slice(0, 80);
            console.log(`[probe] ✓ ${pad(cmd, 24)} ${pad(ms + 'ms', 7)} ${preview}`);
        } catch (e) {
            failed++;
            console.error(`[probe] ✗ ${pad(cmd, 24)}  ${e.message}`);
        }
    }
    ws.close();
    process.exit(failed === 0 ? 0 : 1);
});

ws.addEventListener('message', (e) => {
    let data;
    try { data = JSON.parse(e.data); }
    catch { console.warn('[probe] bad JSON line:', e.data); return; }
    if (data.rid && pending.has(data.rid)) {
        const p = pending.get(data.rid);
        pending.delete(data.rid);
        clearTimeout(p.timer);
        if (data.tp === 'res:error') p.reject(new Error(data.err || 'res:error'));
        else                          p.resolve(data);
    } else if (data.tp === 'evt:status') {
        // Drown the telemetry firehose; print only first one as a sanity check.
        if (!ws._sawStatus) {
            ws._sawStatus = true;
            console.log(`[probe] · evt:status (further hidden) ct=${data.ct} tt=${data.tt}`);
        }
    } else {
        console.log(`[probe] · ${data.tp ?? '(unknown)'}`);
    }
});

ws.addEventListener('error', (e) => {
    console.error('[probe] WebSocket error:', e?.message ?? e);
    process.exit(2);
});

ws.addEventListener('close', () => {
    if (pending.size > 0) {
        for (const [rid, { type }] of pending) {
            console.error(`[probe] ✗ ${type} (rid=${rid}) connection closed`);
        }
        process.exit(1);
    }
});
