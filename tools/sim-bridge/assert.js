#!/usr/bin/env node
// assert.js — CI assertions over a brew JSONL stream.
//
// Usage:  node assert.js <events.jsonl>
//   --expect-completed    (default true)  Recipe must reach evt:recipe:completed
//                                          OR the telemetry mode must transition
//                                          from "recipe" back to "idle".
//   --expect-final-temp <c>   Final telemetry `ct` ≥ value
//   --expect-step-count <n>   Recipe must reach at least step N
//
// Exit 0 on success, 1 with details on failure.

import { readFileSync } from 'node:fs';

const argv = process.argv.slice(2);
if (argv.length === 0) {
    console.error('usage: node assert.js <events.jsonl> [--expect-final-temp N] [--expect-step-count N]');
    process.exit(2);
}

const path = argv[0];
const opts = { finalTempMin: null, stepMin: null, expectCompleted: true };
for (let i = 1; i < argv.length; i++) {
    if (argv[i] === '--expect-final-temp') opts.finalTempMin = parseFloat(argv[++i]);
    else if (argv[i] === '--expect-step-count') opts.stepMin = parseInt(argv[++i], 10);
    else if (argv[i] === '--no-expect-completed') opts.expectCompleted = false;
}

const lines = readFileSync(path, 'utf8').split('\n').filter(l => l.trim());
const events = [];
for (const line of lines) {
    try { events.push(JSON.parse(line)); } catch { /* skip */ }
}

const fails = [];
const tele  = events.filter(e => e.tp === 'evt:status');
const last  = tele[tele.length - 1] || {};
const maxStep = tele.reduce((m, e) => Math.max(m, e.rs || 0), 0);
const totalSteps = tele.reduce((m, e) => Math.max(m, e.rt || 0), 0);
const sawRecipeMode = tele.some(e => e.m === 'recipe');
const sawIdleAfterRecipe = sawRecipeMode && tele.slice(tele.findIndex(e => e.m === 'recipe'))
    .some(e => e.m === 'idle');

if (opts.expectCompleted) {
    if (!sawRecipeMode) {
        fails.push('telemetry never showed mode=recipe');
    } else {
        // Recipe completed if the engine transitioned back to idle, OR the
        // recipe ran out of steps (maxStep >= total). We accept maxStep ==
        // total-1 too because the final non-blocking step (HEATER_OFF) may
        // tick out of the telemetry frame.
        const completedByMode = sawIdleAfterRecipe;
        const completedByStep = totalSteps > 0 && maxStep >= totalSteps - 1;
        if (!completedByMode && !completedByStep) {
            fails.push(`recipe did not complete: step ${maxStep}/${totalSteps}, final mode=${last.m}`);
        }
    }
}
if (opts.finalTempMin !== null && (last.ct === undefined || last.ct < opts.finalTempMin))
    fails.push(`final temp ${last.ct}°C < expected ${opts.finalTempMin}°C`);
if (opts.stepMin !== null && maxStep < opts.stepMin)
    fails.push(`max step reached ${maxStep} < expected ${opts.stepMin}`);

const stats = {
    telemetryRows:  tele.length,
    maxStep,
    totalSteps,
    finalTempC:     last.ct,
    finalMode:      last.m,
    sawRecipeMode,
    sawIdleAfterRecipe,
};
console.log('[assert] stats:', JSON.stringify(stats));

if (fails.length) {
    console.error('[assert] FAIL:');
    for (const f of fails) console.error('  -', f);
    process.exit(1);
}
console.log('[assert] PASS');
