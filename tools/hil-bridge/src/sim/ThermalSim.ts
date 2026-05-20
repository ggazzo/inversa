// ThermalSim — TypeScript port of firmware/sim/ThermalSim.h.
//
// Closed loop:
//   1. Subscribe to SSR events (or poll heaterOn from state telemetry).
//   2. Integrate the temperature ODE forward each tick:
//        m * c_water * dT/dt = P_in * eff  -  h * A * (T - T_amb)
//   3. Push the resulting °C back to firmware as `set ntc.c`.
//
// Integration is done in virtual time. We use ctx.clock.now() as the
// timebase; on each tick we compute dt vs the last integration. The host
// CLI is responsible for advancing the virtual clock (otherwise this sim
// will produce a stuck reading, which is also a useful test fixture).

import type {
  HilPlugin,
  HilPluginContext,
  Report,
  TelemetryMsg,
} from "../core/types.js";

export type ThermalSimParams = {
  volumeLiters: number;
  heaterPowerW: number;
  efficiency: number;
  ambientTempC: number;
  diameterM: number;
  heatLossCoeffLidOn: number;
  heatLossCoeffLidOff: number;
  lidOn: boolean;
  initialTempC: number;
};

const DEFAULTS: ThermalSimParams = {
  volumeLiters: 25,
  heaterPowerW: 3000,
  efficiency: 0.9,
  ambientTempC: 22,
  diameterM: 0.35,
  heatLossCoeffLidOn: 8,
  heatLossCoeffLidOff: 12,
  lidOn: true,
  initialTempC: 22,
};

const C_WATER_J_PER_KG_K = 4186;

export class ThermalSim implements HilPlugin {
  readonly name = "ThermalSim";
  readonly kind = "sim" as const;

  private params: ThermalSimParams;
  private tempC: number;
  private heaterOn = false;
  private lastIntegrationVMs: number | null = null;
  private radius: number;
  private surfaceAreaM2: number;
  private samplesPushed = 0;

  constructor(params: Partial<ThermalSimParams> = {}) {
    this.params = { ...DEFAULTS, ...params };
    this.tempC = this.params.initialTempC;
    this.radius = this.params.diameterM / 2;
    const heightM =
      (this.params.volumeLiters * 0.001) /
      (Math.PI * this.radius * this.radius);
    this.surfaceAreaM2 =
      2 * Math.PI * this.radius * heightM + Math.PI * this.radius * this.radius;
  }

  setup(ctx: HilPluginContext): void {
    // Push initial NTC so the firmware sees a valid temperature from t=0.
    ctx.send({ cmd: "set", path: "ntc.c", value: this.tempC });
    ctx.send({ cmd: "set", path: "ambient.c", value: this.params.ambientTempC });
  }

  onTelemetry(msg: TelemetryMsg, _ctx: HilPluginContext): void {
    if (msg.event === "ssr") {
      this.heaterOn = msg.level === 1;
      return;
    }
    if (msg.event === "state") {
      // Trust the explicit SSR pin; only fall back to heaterOn if no SSR
      // event has been observed yet.
      if (this.lastIntegrationVMs === null) {
        this.heaterOn = msg.heaterOn;
      }
      return;
    }
  }

  onTick(virtualMs: number, ctx: HilPluginContext): void {
    if (this.lastIntegrationVMs === null) {
      this.lastIntegrationVMs = virtualMs;
      return;
    }
    const dtMs = virtualMs - this.lastIntegrationVMs;
    if (dtMs <= 0) return;
    this.lastIntegrationVMs = virtualMs;

    const dt = dtMs / 1000;
    const pIn = this.heaterOn
      ? this.params.heaterPowerW * this.params.efficiency
      : 0;
    const h = this.params.lidOn
      ? this.params.heatLossCoeffLidOn
      : this.params.heatLossCoeffLidOff;
    const pOut = h * this.surfaceAreaM2 * (this.tempC - this.params.ambientTempC);
    const massKg = this.params.volumeLiters; // water density 1 kg/L
    const dT = ((pIn - pOut) / (massKg * C_WATER_J_PER_KG_K)) * dt;
    this.tempC += dT;

    // Push back to firmware. We bypass Kalman for tight closed-loop fidelity
    // — the sim already integrates a smooth signal.
    ctx.send({ cmd: "set", path: "ntc.c", value: this.tempC });
    this.samplesPushed++;
  }

  onShutdown(_ctx: HilPluginContext): Report {
    return {
      plugin: this.name,
      ok: true,
      failures: [],
      metrics: {
        finalTempC: this.tempC,
        samplesPushed: this.samplesPushed,
      },
      notes: [
        `final temp=${this.tempC.toFixed(2)}°C, samples=${this.samplesPushed}`,
      ],
    };
  }

  // Test access
  currentTemp(): number {
    return this.tempC;
  }
}
