// frameExtractor.ts — pulls complete top-level JSON objects out of a
// streaming buffer. The firmware's BLE TX path concatenates frames
// with no delimiter (no newline, no length prefix), so the receiver
// can't rely on `JSON.parse` on the whole buffer — that fails the
// moment frame N+1 starts arriving (`{...}{...}` is not valid JSON).
//
// We walk the buffer counting `{`/`}` while staying aware of string
// literals (so braces inside `"..."` don't shift depth) and slice
// each complete object as soon as its closing brace lands. Pure
// function; testable in isolation.

export interface FrameExtractResult {
    /** Complete top-level JSON objects parsed from the buffer. */
    frames: any[];
    /** Bytes that didn't form a complete frame yet. Caller keeps
     *  this as the new buffer head for the next chunk. */
    remaining: string;
    /** Frames whose JSON.parse failed (corrupt stream). Caller can
     *  log these. Empty under normal operation. */
    parseErrors: { frame: string; error: string }[];
}

export function extractFrames(buffer: string): FrameExtractResult {
    const frames: any[] = [];
    const parseErrors: FrameExtractResult['parseErrors'] = [];

    let depth      = 0;
    let inString   = false;
    let escape     = false;
    let frameStart = -1;
    let consumedTo = 0;     // bytes safely consumed from buffer head

    for (let i = 0; i < buffer.length; i++) {
        const ch = buffer[i];
        if (escape) { escape = false; continue; }
        if (inString) {
            if (ch === '\\') escape = true;
            else if (ch === '"') inString = false;
            continue;
        }
        if (ch === '"') { inString = true; continue; }
        if (ch === '{') {
            if (depth === 0) frameStart = i;
            depth++;
        } else if (ch === '}') {
            depth--;
            if (depth === 0 && frameStart >= 0) {
                const frame = buffer.slice(frameStart, i + 1);
                try {
                    frames.push(JSON.parse(frame));
                } catch (e: any) {
                    parseErrors.push({ frame, error: e?.message ?? String(e) });
                }
                consumedTo = i + 1;
                frameStart = -1;
            }
        }
    }

    return {
        frames,
        remaining: buffer.slice(consumedTo),
        parseErrors,
    };
}
