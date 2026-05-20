// Plugin loader by name. Keeps registration explicit (no dynamic imports
// in production code) so the CLI surface is enumerable for --help.

import type { HilPlugin } from "./types.js";

import { ThermalSim } from "../sim/ThermalSim.js";
import { SafetyInvariants } from "../analyzer/SafetyInvariants.js";
import { PIDQuality } from "../analyzer/PIDQuality.js";

export type PluginFactory = () => HilPlugin;

const REGISTRY: Record<string, PluginFactory> = {
  thermal: () => new ThermalSim(),
  safety: () => new SafetyInvariants(),
  pid: () => new PIDQuality(),
};

export function knownPlugins(): string[] {
  return Object.keys(REGISTRY);
}

export function loadPlugin(name: string): HilPlugin {
  const factory = REGISTRY[name];
  if (!factory) {
    throw new Error(
      `Unknown plugin '${name}'. Known: ${knownPlugins().join(", ")}`,
    );
  }
  return factory();
}

export function loadPlugins(names: string[]): HilPlugin[] {
  return names.map(loadPlugin);
}
