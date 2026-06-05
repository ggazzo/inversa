// Run with: node --test --experimental-strip-types packages/utils/src/parseRecipe.test.ts
//
// Conformance test for the recipe DSL. The firmware (RecipePlugin.h) and this
// app parser are independent implementations that MUST agree on the parsed
// step sequence — the timeline highlights the firmware's `recipeStep` index,
// so a divergence shows the wrong row or silently drops a step.
//
// This pins the app parser's output for the shipped sample recipes. The C++
// side has a mirror test (firmware/test/test_recipe) asserting the SAME
// {type, value, message} sequence on the SAME files. If either parser drifts,
// its conformance test fails — that's the contract these two tests enforce.
//
// Only the cross-parser fields are asserted: verb `type`, numeric `value`,
// and `message`, plus count + order. Display-only fields (label/kind) and the
// firmware-internal STEP→BrewingStep value mapping are out of contract.

import { test } from 'node:test';
import { strict as assert } from 'node:assert';
import { readFileSync } from 'node:fs';
import { fileURLToPath } from 'node:url';
import { dirname, join } from 'node:path';
import { parseRecipe } from './parseRecipe.ts';

const here = dirname(fileURLToPath(import.meta.url));
const SAMPLES = join(here, '../../../firmware/samples');

function seq(content: string) {
    return parseRecipe(content).map((s) => ({
        type: s.type,
        ...(s.value !== undefined && Number.isFinite(s.value) ? { value: s.value } : {}),
        ...(s.message !== undefined ? { message: s.message } : {}),
    }));
}

test('IPA.txt parses to the canonical step sequence', () => {
    const content = readFileSync(join(SAMPLES, 'IPA.txt'), 'utf8');
    assert.deepEqual(seq(content), [
        { type: 'STEP', message: 'Pre-aquecimento' },
        { type: 'SET_TEMP', value: 67 },
        { type: 'WAIT_TEMP', value: 0.5 },
        { type: 'STEP', message: 'Mostura' },
        { type: 'WAIT_TIMER', value: 60 },
        { type: 'MASH_OUT', value: 76 },
        { type: 'WAIT_TEMP', value: 0.5 },
        { type: 'STEP', message: 'Lavagem' },
        { type: 'WAIT_CONFIRM', message: 'Iniciar lavagem com 10L' },
        { type: 'STEP', message: 'Fervura' },
        { type: 'SET_TEMP', value: 100 },
        { type: 'WAIT_TEMP', value: 0.5 },
        { type: 'ADD_HOP', value: 60, message: 'Magnum 30g' },
        { type: 'ADD_HOP', value: 15, message: 'Cascade 40g' },
        { type: 'ADD_HOP', value: 5, message: 'Citra 30g' },
        { type: 'BOIL', value: 60 },
        { type: 'WAIT_BOIL' },
        { type: 'STEP', message: 'Resfriamento' },
        { type: 'HEATER_OFF' },
    ]);
});

test('quick.txt parses to the canonical step sequence', () => {
    const content = readFileSync(join(SAMPLES, 'quick.txt'), 'utf8');
    assert.deepEqual(seq(content), [
        { type: 'STEP', message: 'Pre-aquecimento' },
        { type: 'SET_TEMP', value: 60 },
        { type: 'WAIT_TEMP', value: 1.0 },
        { type: 'STEP', message: 'Mostura curta' },
        { type: 'WAIT_TIMER', value: 1 },
        { type: 'STEP', message: 'Resfriamento' },
        { type: 'HEATER_OFF' },
    ]);
});

test('blank lines and # comments are skipped', () => {
    assert.deepEqual(seq('# header\n\nSET_TEMP 50\n  # indented comment\nHEATER_OFF\n'), [
        { type: 'SET_TEMP', value: 50 },
        { type: 'HEATER_OFF' },
    ]);
});
