#!/usr/bin/env node
// BrewPilot simulator ↔ WebSocket bridge.
//
// Spawns one `firmware_sim` child per connected WebSocket client and pipes
// stdin/stdout between them. The PWA (with `?sim=ws://host:port`) speaks
// the same JSON protocol it would over BLE, so no UI changes are needed
// beyond a single conditional in BLEService.js.

import { spawn } from 'node:child_process';
import { existsSync } from 'node:fs';
import { resolve, join } from 'node:path';
import { fileURLToPath } from 'node:url';
import { WebSocketServer } from 'ws';

const here = fileURLToPath(new URL('.', import.meta.url));
const repoRoot = resolve(here, '..', '..');

const args = process.argv.slice(2);
const port = parseInt(envOrFlag('--port', 'PORT', '8765'), 10);
const simBin = process.env.SIM_BIN || join(repoRoot, 'firmware', '.pio', 'build', 'sim', 'program');
// Strip "--port N" (both the flag AND its value) from the list we pass on
// to the sim. Other args are forwarded verbatim.
const simArgs = (() => {
    const out = [];
    for (let i = 0; i < args.length; i++) {
        if (args[i] === '--port') { i++; continue; }
        out.push(args[i]);
    }
    return out;
})();

if (!existsSync(simBin)) {
    console.error(`[bridge] sim binary not found at ${simBin}`);
    console.error(`[bridge] build it first:  cd firmware && pio run -e sim`);
    process.exit(1);
}

const wss = new WebSocketServer({ port });
console.log(`[bridge] listening on ws://localhost:${port}`);
console.log(`[bridge] each client spawns: ${simBin} ${simArgs.join(' ')}`);

wss.on('connection', (client, req) => {
    const ip = req.socket.remoteAddress;
    console.log(`[bridge] +client ${ip}`);
    const sim = spawn(simBin, simArgs, { cwd: join(repoRoot, 'firmware') });

    let stdoutBuf = '';
    sim.stdout.on('data', chunk => {
        stdoutBuf += chunk.toString();
        let nl;
        while ((nl = stdoutBuf.indexOf('\n')) >= 0) {
            const line = stdoutBuf.slice(0, nl).trim();
            stdoutBuf = stdoutBuf.slice(nl + 1);
            if (line && client.readyState === 1) client.send(line);
        }
    });
    sim.stderr.on('data', d => process.stderr.write(`[sim:${sim.pid}] ${d}`));
    sim.on('exit', (code) => {
        console.log(`[bridge] sim ${sim.pid} exited (${code})`);
        try { client.close(); } catch {}
    });

    client.on('message', m => {
        try { sim.stdin.write(m.toString().trim() + '\n'); } catch {}
    });
    client.on('close', () => {
        console.log(`[bridge] -client ${ip}`);
        try { sim.kill('SIGINT'); } catch {}
    });
});

function envOrFlag(flag, env, def) {
    const idx = args.indexOf(flag);
    if (idx >= 0 && args[idx + 1]) return args[idx + 1];
    return process.env[env] || def;
}
