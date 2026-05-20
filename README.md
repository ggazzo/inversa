# Inversa

**ESP32-based temperature controller for homebrewing**

Inversa is a complete system for brewing automation, featuring ESP32 firmware and a web app that connects via Bluetooth. Control temperature, run automated recipes, schedule your brew sessions, and monitor everything from your phone — no app installation required.

## Features

### Temperature Control

- **PID with heat loss compensation** — advanced algorithm that accounts for equipment heat loss
- **Auto-Tune** — automatic PID parameter calibration using Ziegler-Nichols relay method
- **Ramp Mode** — gradual heating (°C/min) for delicate mash profiles
- **Kalman Filter** — stable and accurate temperature readings

### Recipe Automation

- **Recipe language** — simple text files with intuitive commands
- **22 available commands** — full control over temperature, timers, pump, heater
- **Boil timer** — with hop addition alerts at configured times
- **Automatic mash-out** — raises to inactivation temperature at the end of mashing

### Smart Scheduling

- **"Ready at HH:MM"** — set when you want the water ready and the system calculates when to start heating
- **Thermal calculations** — considers volume, ambient temperature, and heater power
- **RTC with NTP sync** — keeps time even without WiFi

### Monitoring & Logging

- **Real-time telemetry** — temperature, setpoint, PID output, actuator states
- **Temperature chart** — visualize your brew session history
- **Log export** — CSV or JSON for later analysis
- **Power failure recovery** — resumes recipe from where it stopped

### Connectivity

- **Web Bluetooth** — connects directly from the browser, no native app needed
- **OTA via GitHub** — update firmware over the air, straight from releases
- **PWA** — install the web app on your phone's home screen

## Supported Hardware

| Board                 | Status      |
| --------------------- | ----------- |
| ESP32-S3 Mini (Lolin) | Recommended |
| ESP32-C3 Mini (Lolin) | Supported   |

### Required Components

- ESP32-S3 Mini or ESP32-C3 Mini
- NTC 10K temperature sensor with 10K pull-up resistor
- SSR relay for heater (e.g., SSR-25DA)
- Relay module for pump (optional)
- DS1307 RTC module (optional, for scheduling)
- SD Card module (optional, for recipes and logs)

### Wiring Diagram

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                           INVERSA WIRING DIAGRAM                            │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│   ESP32-S3 Mini                          ESP32-C3 Mini                      │
│   ─────────────                          ─────────────                      │
│                                                                             │
│   ┌─────────────┐                        ┌─────────────┐                    │
│   │  S3 MINI    │                        │  C3 MINI    │                    │
│   │             │                        │             │                    │
│   │  GPIO1  ────┼── NTC Sensor           │  GPIO3  ────┼── NTC Sensor       │
│   │  GPIO4  ────┼── SSR (Heater)         │  GPIO2  ────┼── SSR (Heater)     │
│   │  GPIO16 ────┼── Relay (Pump)         │  GPIO6  ────┼── Relay (Pump)     │
│   │  GPIO47 ────┼── NeoPixel (opt)       │  GPIO7  ────┼── NeoPixel (opt)   │
│   │             │                        │             │                    │
│   │  GPIO35 ────┼── I2C SDA (RTC)        │  GPIO8  ────┼── I2C SDA (RTC)    │
│   │  GPIO36 ────┼── I2C SCL (RTC)        │  GPIO10 ────┼── I2C SCL (RTC)    │
│   │             │                        │             │                    │
│   │  SS     ────┼── SD CS                │  GPIO5  ────┼── SD CS            │
│   │  SCK    ────┼── SD SCK               │  GPIO1  ────┼── SD SCK           │
│   │  MISO   ────┼── SD MISO              │  GPIO0  ────┼── SD MISO          │
│   │  MOSI   ────┼── SD MOSI              │  GPIO4  ────┼── SD MOSI          │
│   │             │                        │             │                    │
│   │  3V3    ────┼── VCC (sensors)        │  3V3    ────┼── VCC (sensors)    │
│   │  GND    ────┼── GND (common)         │  GND    ────┼── GND (common)     │
│   │  5V     ────┼── VCC (relays)         │  5V     ────┼── VCC (relays)     │
│   └─────────────┘                        └─────────────┘                    │
│                                                                             │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│   NTC TEMPERATURE SENSOR                 SSR RELAY (HEATER)                 │
│   ──────────────────────                 ─────────────────                  │
│                                                                             │
│        3V3                                    ESP32                         │
│         │                                       │                           │
│         ├──┤ 10K ├──┬── NTC Pin                GPIO ──── SSR Input (+)      │
│         │          │                           GND ───── SSR Input (-)      │
│        NTC         │                                                        │
│         │          │                      SSR Output ── Heater (AC Live)    │
│        GND        GND                     AC Neutral ── Heater (AC Neutral) │
│                                                                             │
│   PUMP RELAY MODULE                      DS1307 RTC MODULE                  │
│   ─────────────────                      ────────────────                   │
│                                                                             │
│   VCC ───── 5V                           VCC ───── 3V3                      │
│   GND ───── GND                          GND ───── GND                      │
│   IN  ───── Pump GPIO                    SDA ───── I2C SDA                  │
│   NO  ───── Pump (+)                     SCL ───── I2C SCL                  │
│   COM ───── Power Supply (+)                                                │
│                                                                             │
│   SD CARD MODULE                         NEOPIXEL (OPTIONAL)                │
│   ──────────────                         ───────────────────                │
│                                                                             │
│   VCC ───── 3V3                          VCC ───── 5V                       │
│   GND ───── GND                          GND ───── GND                      │
│   CS  ───── SD CS                        DIN ───── NeoPixel GPIO            │
│   SCK ───── SD SCK                                                          │
│   MISO ──── SD MISO                                                         │
│   MOSI ──── SD MOSI                                                         │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

### Pin Reference Table

| Function     | ESP32-S3 Mini | ESP32-C3 Mini | Notes                    |
| ------------ | ------------- | ------------- | ------------------------ |
| NTC Sensor   | GPIO1 (A1)    | GPIO3 (A3)    | Analog input + 10K pull-up |
| Heater SSR   | GPIO4         | GPIO2         | 3.3V logic, active HIGH  |
| Pump Relay   | GPIO16        | GPIO6         | 5V relay module          |
| NeoPixel     | GPIO47        | GPIO7         | WS2812B compatible       |
| I2C SDA      | GPIO35        | GPIO8         | For RTC (DS1307)         |
| I2C SCL      | GPIO36        | GPIO10        | For RTC (DS1307)         |
| SD Card CS   | SS (default)  | GPIO5         | SPI chip select          |
| SD Card SCK  | SCK (default) | GPIO1         | SPI clock                |
| SD Card MISO | MISO (default)| GPIO0         | SPI data in              |
| SD Card MOSI | MOSI (default)| GPIO4         | SPI data out             |

## Installation

### Firmware

```bash
# Clone the repository
git clone https://github.com/ggazzo/inversa.git
cd inversa/firmware

# Build and upload (requires PlatformIO)
pio run -e wemos_s3_mini -t upload
```

### Web App

```bash
cd web

# Install dependencies
npm install

# Run in development
npm run dev

# Or build for production
npm run build
```

The compiled web app can be hosted on any static server (GitHub Pages, Vercel, Netlify) or opened locally.

## Usage

### Connecting

1. Open the web app in your browser (Chrome, Edge, or any browser with Web Bluetooth support)
2. Click "Connect"
3. Select the "Inversa" device from the list

### Manual Control

On the **Control** page:

- Set your desired temperature
- Use quick presets (50°, 60°, 65°, 68°, 72°, 78°, 100°C)
- Manually toggle heater and pump

### Running Recipes

1. Go to **Recipes**
2. Select a recipe from the SD card or create a new one
3. Click **Start**
4. Monitor progress on the **Dashboard**

### Scheduling

On the **Control** page, in the "Scheduler" card:

1. Set the water volume (liters)
2. Set the desired temperature
3. Set the time you want the water ready
4. Click "Schedule"

The system will automatically calculate when to turn on the heater.

## Recipe Language

Recipes are text files with one command per line:

```
# Classic Pilsner - 20L

STEP PRE_HEATING
SET_TEMP 52
WAIT_TEMP

STEP MASHING
WAIT_TIMER 10
WAIT_CONFIRM "Add grains and stir well"

SET_TEMP 62
WAIT_TEMP
WAIT_TIMER 20

SET_TEMP 68
WAIT_TEMP
WAIT_TIMER 40

STEP MASH_OUT
MASH_OUT 76

STEP BOILING
SET_TEMP 100
WAIT_TEMP

ADD_HOP 60 "Hallertau - bittering"
ADD_HOP 15 "Saaz - flavor"
ADD_HOP 5 "Saaz - aroma"
BOIL 60
WAIT_BOIL

STEP DONE
HEATER_OFF
PUMP_OFF
```

### Available Commands

| Command                    | Description                                     |
| -------------------------- | ----------------------------------------------- |
| `SET_TEMP <value>`         | Set target temperature                          |
| `WAIT_TEMP [tolerance]`    | Wait until temperature reached (default: 0.5°C) |
| `WAIT_TIMER <minutes>`     | Internal recipe timer                           |
| `TIMER <minutes>`          | Timer with app notification                     |
| `ALARM <HH:MM>`            | Alarm at specific time                          |
| `MASH_OUT [temp]`          | Mash-out (default: 76°C)                        |
| `BOIL <minutes>`           | Start boil timer                                |
| `ADD_HOP <minutes> "name"` | Add hop addition (minutes remaining)            |
| `WAIT_BOIL`                | Wait for boil to complete                       |
| `RAMP <rate>`              | Enable ramp mode (°C/min)                       |
| `RAMP_OFF`                 | Disable ramp mode                               |
| `PUMP_ON` / `PUMP_OFF`     | Turn pump on/off                                |
| `HEATER_ON` / `HEATER_OFF` | Turn heater on/off                              |
| `WAIT_CONFIRM "msg"`       | Wait for user confirmation                      |
| `STEP <stage>`             | Set current stage (displayed in app)            |
| `# comment`                | Ignored line                                    |

### Brewing Stages

The `STEP` command accepts:

- `PRE_HEATING`
- `MASHING`
- `MASH_OUT`
- `SPARGE`
- `BOILING`
- `HOPPING`
- `COOLING`
- `DONE`

## Configuration

### PID Auto-Tune

1. Fill your equipment with the usual water volume
2. Go to **Settings** > **Auto-Tune**
3. Set a target temperature (~65°C is ideal)
4. Start the process (takes 15-30 minutes)
5. Parameters are saved automatically

### Heat-Loss Auto-Tune (LossTune)

Measures the pot's heat-transfer coefficient `h` (W/m²·K) by fitting a
Newton-cooling decay. Two coefficients are stored — one for lid-on and
one for lid-off — and feed the PID feed-forward + scheduler.

Open **Equipment wizard** → "Auto-tune do coef. de perda" and pick the
lid mode. Takes ~10-15 min per mode. See [docs/losstune.md](docs/losstune.md)
for the physics, acceptance gates, failure modes, BLE protocol, and
bench-validation checklist.

### OTA (Over-the-Air Updates)

1. Connect the ESP32 to WiFi in **Settings**
2. Click "Check for Updates"
3. If a new version is available, click "Install"

## Architecture

```
inversa/
├── firmware/           # ESP32 code (PlatformIO + Arduino)
│   ├── src/
│   │   ├── core/       # EventBus, NVS, Recovery, ThermalCalc
│   │   ├── models/     # MachineState
│   │   ├── plugins/    # 17 plugins (PID, BLE, Recipe, etc.)
│   │   └── protocol/   # BLE protocol definitions
│   └── test/           # Unit tests (Unity)
└── web/                # Web App (Preact + Vite + DaisyUI)
    └── src/
        ├── pages/      # Dashboard, Control, Boil, Recipes, Settings
        ├── stores/     # Global state (Zustand)
        └── services/   # BLE ConnectionManager
```

### Firmware Plugins

| Plugin                | Function                                           |
| --------------------- | -------------------------------------------------- |
| TemperaturePlugin     | NTC reading with Kalman filter                     |
| PIDPlugin             | PID control with feed-forward                      |
| HeaterPlugin          | Soft PWM for SSR                                   |
| ThermalWatchdogPlugin | Independent safety layer (overtemp / fault / loop-stuck / gradient) |
| PumpPlugin            | Pump control                                       |
| RecipePlugin          | Recipe execution                                   |
| BoilTimerPlugin       | Boil timer with hop additions                      |
| TimerPlugin           | Generic timer                                      |
| SchedulerPlugin       | "Ready at" scheduling                              |
| RampPlugin            | Gradual heating                                    |
| BLEPlugin             | Bluetooth communication                            |
| WiFiPlugin            | WiFi management                                    |
| OTAPlugin             | OTA updates                                        |
| SDCardPlugin          | SD storage                                         |
| RTCPlugin             | Real-time clock                                    |
| AutoTunePlugin        | PID calibration                                    |
| BrewLogPlugin         | Brew session logging                               |
| CommandHandler        | Command routing                                    |

### Thermal Watchdog

`ThermalWatchdogPlugin` is a redundant safety layer that monitors the
control loop and forces the SSR off through a direct GPIO write when any
of four conditions trip:

- **Overtemp** — `currentTemp > T_HARDSTOP` (default 105 °C)
- **Sensor fault** — `tempSensorOk == false` for ≥ `T_SENSOR_FAULT_MS` (default 10 s)
- **Loop stuck** — main `loop()` has not called `kick()` for ≥ `T_LOOP_STUCK_MS` (default 5 s; checked by an `esp_timer` task every 100 ms)
- **Gradient** — `|dT|` exceeds `GRAD_FACTOR ×` running median of the last `GRAD_WINDOW` samples (defaults 5 × median, 20 samples / 4 s history)

On trip the SSR is forced LOW, the latch is persisted in NVS (so it
survives reboot), `HeaterPlugin` and `PIDPlugin` suppress further heat
commands, and an `evt:watchdog:tripped` event is sent over BLE. The
latch is cleared in one of two ways:

- The user issues `req:watchdog:reset` while the temperature is safely below `T_HARDSTOP − 5 °C`
- Cooldown auto-reset (default enabled): `currentTemp < T_SAFE_AUTORESET` (40 °C) for `T_COOL_MIN_MS` (5 minutes) with no other condition active

Configurable via `req:watchdog:config`; all limits are validated and
persisted in NVS under the `wd_*` keys.

A future redundant hardware cut path can be plugged in via the
`IExternalCut` interface (declared in `firmware/src/plugins/IExternalCut.h`),
without changing the watchdog core. No concrete external implementation
ships in this release.

See `_reversa_forward/001-thermal-watchdog/` (requirements, roadmap,
data-delta, interfaces, onboarding) for the full specification.

## License

MIT
