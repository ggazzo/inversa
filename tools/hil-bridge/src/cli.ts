#!/usr/bin/env node
// hil-bridge CLI.
//
// Usage:
//   hil-bridge run scenario <name> [--port /dev/tty…] [--duration 30s]
//   hil-bridge run --sim thermal --analyzer safety,pid [--duration 30s]
//   hil-bridge list

import { runScenario } from "./core/runner.js";
import { knownScenarios, loadScenario } from "./scenarios/index.js";
import { knownPlugins, loadPlugins } from "./core/registry.js";

function parseDuration(s: string | undefined, fallbackMs: number): number {
  if (!s) return fallbackMs;
  const m = s.match(/^(\d+)(ms|s|m)?$/);
  if (!m) throw new Error(`bad duration: ${s}`);
  const n = parseInt(m[1] ?? "0", 10);
  switch (m[2] ?? "ms") {
    case "ms": return n;
    case "s":  return n * 1000;
    case "m":  return n * 60_000;
    default:   return n;
  }
}

type Args = {
  command?: string;
  scenario?: string;
  port: string;
  sims: string[];
  analyzers: string[];
  durationMs: number;
  stateHz: number;
  list?: boolean;
};

function parseArgs(argv: string[]): Args {
  // Very small parser — no dependency, deterministic.
  const args: Args = {
    port: process.env.HIL_PORT ?? "-",
    sims: [],
    analyzers: [],
    durationMs: 30_000,
    stateHz: 20,
  };
  let i = 0;
  while (i < argv.length) {
    const a = argv[i++]!;
    if (a === "run" || a === "list") {
      args.command = a;
    } else if (a === "scenario") {
      args.scenario = argv[i++];
    } else if (a === "--port") {
      args.port = argv[i++] ?? "-";
    } else if (a === "--sim") {
      args.sims.push(...(argv[i++]?.split(",") ?? []));
    } else if (a === "--analyzer") {
      args.analyzers.push(...(argv[i++]?.split(",") ?? []));
    } else if (a === "--duration") {
      args.durationMs = parseDuration(argv[i++], 30_000);
    } else if (a === "--state-hz") {
      args.stateHz = parseInt(argv[i++] ?? "20", 10);
    } else if (a === "--list") {
      args.list = true;
    } else if (a === "-h" || a === "--help") {
      printHelp();
      process.exit(0);
    }
  }
  return args;
}

function printHelp(): void {
  process.stdout.write(
    [
      "hil-bridge — host bridge for Inversa HIL builds",
      "",
      "Usage:",
      "  hil-bridge run scenario <name> [--port <serial>] [--duration 30s]",
      "  hil-bridge run --sim <names> --analyzer <names> [--port <serial>] [--duration 30s]",
      "  hil-bridge list",
      "",
      `Scenarios: ${knownScenarios().join(", ")}`,
      `Plugins:   ${knownPlugins().join(", ")}`,
      "",
      "Use --port - for stdio (testing).",
      "",
    ].join("\n"),
  );
}

async function main(): Promise<void> {
  const args = parseArgs(process.argv.slice(2));

  if (args.command === "list" || args.list) {
    process.stdout.write(
      JSON.stringify(
        { scenarios: knownScenarios(), plugins: knownPlugins() },
        null,
        2,
      ) + "\n",
    );
    return;
  }

  if (args.command !== "run") {
    printHelp();
    process.exit(args.command ? 2 : 0);
  }

  const plugins = args.scenario
    ? loadScenario(args.scenario)
    : loadPlugins([...args.sims, ...args.analyzers]);

  if (plugins.length === 0) {
    process.stderr.write("[bridge] no plugins selected — nothing to do\n");
    printHelp();
    process.exit(2);
  }

  const result = await runScenario({
    port: args.port,
    plugins,
    durationMs: args.durationMs,
    stateHz: args.stateHz,
  });

  process.stdout.write(JSON.stringify(result, null, 2) + "\n");
  process.exit(result.ok ? 0 : 1);
}

main().catch((err) => {
  // eslint-disable-next-line no-console
  console.error("[bridge] fatal:", err);
  process.exit(1);
});
