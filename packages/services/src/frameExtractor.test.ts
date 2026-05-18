// Run with: node --test --experimental-strip-types packages/services/src/frameExtractor.test.ts
//
// Simulates the firmware's BLE TX behaviour: 20-byte MTU chunks, no
// delimiter between JSON frames, multiple frames sent back-to-back.
// Each test feeds the extractor one or more chunks and asserts the
// frame stream + leftover buffer match.

import { test } from 'node:test';
import { strict as assert } from 'node:assert';
import { extractFrames } from './frameExtractor.ts';

// Helper: feed chunks one at a time, accumulate emitted frames.
function runStream(chunks: string[]): { frames: any[]; remaining: string } {
    let buffer = '';
    const all: any[] = [];
    for (const c of chunks) {
        buffer += c;
        const { frames, remaining } = extractFrames(buffer);
        all.push(...frames);
        buffer = remaining;
    }
    return { frames: all, remaining: buffer };
}

// Split a string into N-byte chunks like a BLE notify stream.
function chunkify(s: string, size: number): string[] {
    const out: string[] = [];
    for (let i = 0; i < s.length; i += size) out.push(s.slice(i, i + size));
    return out;
}

test('single complete frame in one chunk', () => {
    const { frames, remaining } = runStream(['{"tp":"evt:status","ct":65.4}']);
    assert.deepEqual(frames, [{ tp: 'evt:status', ct: 65.4 }]);
    assert.equal(remaining, '');
});

test('one frame split into 20-byte chunks', () => {
    const json = '{"tp":"evt:status","ct":65.4,"tt":67.0,"out":128,"h":true,"p":false}';
    const { frames, remaining } = runStream(chunkify(json, 20));
    assert.equal(frames.length, 1);
    assert.equal(frames[0].tp, 'evt:status');
    assert.equal(frames[0].out, 128);
    assert.equal(remaining, '');
});

test('two frames back-to-back, no delimiter', () => {
    const a = '{"tp":"evt:status","ct":65.0}';
    const b = '{"tp":"evt:status","ct":66.0}';
    const { frames, remaining } = runStream(chunkify(a + b, 20));
    assert.equal(frames.length, 2);
    assert.equal(frames[0].ct, 65.0);
    assert.equal(frames[1].ct, 66.0);
    assert.equal(remaining, '');
});

test('multiple frames split across many small chunks', () => {
    // Roughly matches the failure mode in the live log: 20-byte
    // chunks, buffer grew to 1700+ bytes. Build a stream of ~10
    // frames of varied size; verify every one parses.
    const frames = Array.from({ length: 10 }, (_, i) => ({
        tp:    'evt:status',
        ct:    60 + i * 0.5,
        tt:    67,
        out:   100 + i,
        seq:   i,
        rn:    i % 2 === 0 ? 'IPA' : 'Pilsen',
    }));
    const concat = frames.map((f) => JSON.stringify(f)).join('');
    const result = runStream(chunkify(concat, 20));
    assert.equal(result.frames.length, frames.length);
    for (let i = 0; i < frames.length; i++) {
        assert.equal(result.frames[i].seq, i);
    }
    assert.equal(result.remaining, '');
});

test('partial trailing frame stays in buffer', () => {
    const complete = '{"tp":"evt:status","ct":65.0}';
    const partial  = '{"tp":"evt:status","ct":';
    const { frames, remaining } = runStream(chunkify(complete + partial, 7));
    assert.equal(frames.length, 1);
    assert.equal(remaining, partial);
});

test('braces inside string literals do not shift depth', () => {
    const json = '{"tp":"evt:log","msg":"{not a frame}","ok":true}';
    const { frames, remaining } = runStream(chunkify(json, 5));
    assert.equal(frames.length, 1);
    assert.equal(frames[0].msg, '{not a frame}');
    assert.equal(remaining, '');
});

test('escaped quotes inside string literals', () => {
    const json = '{"tp":"evt:log","msg":"he said \\"hi\\"","ok":1}';
    const { frames } = runStream(chunkify(json, 4));
    assert.equal(frames.length, 1);
    assert.equal(frames[0].msg, 'he said "hi"');
});

test('nested objects', () => {
    const json = '{"tp":"evt:status","nested":{"a":1,"b":{"c":2}},"end":true}';
    const { frames } = runStream(chunkify(json, 3));
    assert.equal(frames.length, 1);
    assert.equal(frames[0].nested.b.c, 2);
    assert.equal(frames[0].end, true);
});

test('corrupt frame surfaces in parseErrors, stream continues', () => {
    // Manually craft a buffer with a bad frame followed by a good one.
    // depth-tracking would balance braces, but the JSON is malformed.
    const bad  = '{"tp":,,,}';
    const good = '{"tp":"ok"}';
    const buffer = bad + good;
    const { frames, remaining, parseErrors } = extractFrames(buffer);
    assert.equal(parseErrors.length, 1);
    assert.equal(frames.length, 1);
    assert.equal(frames[0].tp, 'ok');
    assert.equal(remaining, '');
});
