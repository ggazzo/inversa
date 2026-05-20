import type { HilPlugin } from "../core/types.js";
import { overtempScenario } from "./overtemp.js";

export type ScenarioFactory = () => HilPlugin[];

const SCENARIOS: Record<string, ScenarioFactory> = {
  overtemp: () => overtempScenario(),
};

export function knownScenarios(): string[] {
  return Object.keys(SCENARIOS);
}

export function loadScenario(name: string): HilPlugin[] {
  const factory = SCENARIOS[name];
  if (!factory) {
    throw new Error(
      `Unknown scenario '${name}'. Known: ${knownScenarios().join(", ")}`,
    );
  }
  return factory();
}
