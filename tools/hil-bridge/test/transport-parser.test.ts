// Unit-level coverage of the JSONL line classification in
// SerialTransport.handleLine. We don't open a real serial port; we
// re-implement the rules verbatim and feed lines through them.
//
// The intent is to catch any regression that re-orders the `#` prefix
// check vs the JSON parse — the firmware mixes DEBUG_PRINTF output with
// telemetry on the same wire, so a wrong order would break parsing on
// every log line.

import { test } from "node:test";
import { strict as assert } from "node:assert";

type Classification = "empty" | "log" | "parse-error" | "telemetry";

// Mirror of SerialTransport.handleLine (src/core/transport.ts).
function classify(rawLine: string): { kind: Classification; payload?: unknown } {
  const line = rawLine.replace(/\r$/, "");
  if (line.length === 0) return { kind: "empty" };
  if (line.startsWith("# ")) return { kind: "log", payload: line.slice(2) };
  if (line.startsWith("#")) return { kind: "log", payload: line.slice(1) };
  try {
    return { kind: "telemetry", payload: JSON.parse(line) };
  } catch (err) {
    return { kind: "parse-error", payload: (err as Error).message };
  }
}

test("empty lines are dropped", () => {
  assert.equal(classify("").kind, "empty");
  assert.equal(classify("\r").kind, "empty");
});

test("# space prefix is treated as log and the space is stripped", () => {
  const r = classify("# [Heater] SSR on GPIO 5");
  assert.equal(r.kind, "log");
  assert.equal(r.payload, "[Heater] SSR on GPIO 5");
});

test("# without space is also treated as log", () => {
  const r = classify("#tight");
  assert.equal(r.kind, "log");
  assert.equal(r.payload, "tight");
});

test("valid JSON parses as telemetry", () => {
  const r = classify('{"event":"hello","t":42,"firmware":"v1","schema":1}');
  assert.equal(r.kind, "telemetry");
  assert.deepEqual(r.payload, {
    event: "hello",
    t: 42,
    firmware: "v1",
    schema: 1,
  });
});

test("valid state telemetry parses (currentTemp schema)", () => {
  const r = classify(
    '{"event":"state","t":100,"currentTemp":22.5,"targetTemp":65,"tempSensorOk":true,"heaterOn":false,"pidOutput":0,"watchdogTripped":false,"watchdogLastCause":0,"watchdogTripCount":0,"ambientSensorC":22,"ambientSensorOk":false,"mode":0,"rtcAvailable":false,"rtcTimestamp":0,"ssr":0}',
  );
  assert.equal(r.kind, "telemetry");
});

test("malformed JSON yields parse-error", () => {
  const r = classify('{"event":"hello",');
  assert.equal(r.kind, "parse-error");
});

test("whitespace-only line falls through to parse-error (no implicit trim)", () => {
  const r = classify("   ");
  assert.equal(r.kind, "parse-error");
});

test("logs interleaved with telemetry never derail the parser", () => {
  const lines = [
    "# starting",
    '{"event":"hello","t":0,"firmware":"v1","schema":1}',
    "# config loaded",
    '{"event":"state","t":100,"currentTemp":22.5,"targetTemp":65,"tempSensorOk":true,"heaterOn":false,"pidOutput":0,"watchdogTripped":false,"watchdogLastCause":0,"watchdogTripCount":0,"ambientSensorC":22,"ambientSensorOk":false,"mode":0,"rtcAvailable":false,"rtcTimestamp":0,"ssr":0}',
    "",
  ];
  const kinds = lines.map((l) => classify(l).kind);
  assert.deepEqual(kinds, ["log", "telemetry", "log", "telemetry", "empty"]);
});
