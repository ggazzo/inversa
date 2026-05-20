# Inversa Display

Secondary BLE-central UI device for the Inversa brewing controller. Mirrors
the data the PWA shows and lets the operator confirm prompts, cancel
schedules, and reset the watchdog from a touch screen at the brewery.

## Hardware

- **Waveshare ESP32-S3 1.47" LCD Touch** — 172×320 IPS, JD9853 driver,
  AXS5106L capacitive touch over I2C, SD card slot, 16 MB flash, octal
  PSRAM.

The pin map under `src/lcd/LCD.h` matches the Waveshare reference design.
If you have a different board revision, override the `LcdPins` constants
in that header.

## Software stack

- **LovyanGFX** — display driver for JD9853 and the SPI/DMA wiring.
- **LVGL v9** — UI framework, widgets, touch input handling.
- **SquareLine Studio** — design tool. Drop your generated `ui/` folder
  into `src/ui/sq/` (you create it). The bridge layer in
  `src/bridge/LvglBridge.cpp` reads screens by name from there once they
  exist; until then the handwritten Scanning and Home placeholders in
  `src/screens/` are used.
- **NimBLE-Arduino** — BLE central role only (peripheral and broadcaster
  disabled in build flags to save flash).
- **ArduinoJson** — parses the `evt:status` / `evt:boil:*` / `evt:recipe:*`
  notifies coming back from the controller.

## Build & flash

```bash
cd firmware-display
pio run -e waveshare_s3_lcd147 -t upload
pio device monitor -e waveshare_s3_lcd147
```

The first build downloads LVGL v9 and LovyanGFX from PlatformIO, takes a
few minutes. Subsequent builds are quick.

## Wire format

Same as the PWA. The display connects to the brewing controller via
Nordic UART Service (UUID `6E40…`) and exchanges JSON-line frames:

- **Inbound**: `{"tp":"evt:status", ...}`, `{"tp":"evt:boil:addition", ...}`,
  `{"tp":"evt:recipe:state", ...}`, `{"tp":"evt:recipe:step", ...}`,
  `{"tp":"evt:recipe:wait", ...}`, `{"tp":"evt:watchdog:tripped", ...}`,
  etc. Parsed in `src/ble/BleClient.cpp::onNotify`.
- **Outbound**: `{"tp":"req:recipe:confirm"}`, `{"tp":"req:sched:stop"}`,
  `{"tp":"req:watchdog:reset"}`, `{"tp":"req:set-temp","temp":<C>}`, etc.
  See `BleClient.h` for the typed helpers.

## SquareLine workflow

1. Open SquareLine Studio, target LVGL v9, resolution 172×320.
2. Design your screens. Use `lv_obj_set_user_data` or named widgets so
   the bridge can look them up at runtime.
3. Export → `src/ui/sq/` (create the folder; gitignore the export if you
   prefer round-tripping rather than committing).
4. In `src/main.cpp`, include the SquareLine entry header
   (`ui/sq/ui.h`) and call `ui_init()` instead of `bridge_init()` — or
   keep both and let the bridge swap between Scanning and the SquareLine
   home.

The bridge in `src/bridge/LvglBridge.cpp` is intentionally minimal: it
shows how to write AppState fields into LVGL labels and bars. Replace
the hand-rolled Home with the SquareLine version once you have it.

## Layout / state machine

State transitions are driven by `AppState`, fed by `BleClient`:

| AppState condition | Screen |
|--------------------|--------|
| `!bleConnected` | Scanning |
| `wdTripped` | Watchdog overlay (TODO) |
| Recipe in `waiting_confirm` | Confirm overlay (TODO) |
| `schedActive && mode=idle` | Scheduled view (TODO) |
| `mode=recipe && boilActive` | Boil view (TODO) |
| `mode=recipe` | Recipe step view (TODO) |
| `mode=manual` | Manual view (TODO) |
| `mode=idle` (default) | Home |

Only **Scanning** and **Home (placeholder)** are wired today. The other
views are the next pass — they map 1:1 to the PWA's sub-states in
`packages/ui/src/views/states/`.

## Known limitations / TODO

- SquareLine integration is documented but not exercised yet. Drop your
  first export and shrink the bridge to match.
- Hop alert queue mirrors firmware notifies locally (`AppState::hops`).
  When the firmware-side queue lands (see `firmware/docs/hop-queue-plan.md`)
  the display should consume the firmware's authoritative `ph[]` array
  in `evt:status` instead and drop its own ring buffer.
- Watchdog overlay screen, Scheduled view, Recipe step views are stubs
  in the README only — the LvglBridge state machine just renders Home
  when connected today.
- SD card slot is unused. Could cache recipes locally or log telemetry
  for offline review.
- No persistent settings yet (theme, last device); add Preferences on
  the next iteration.
