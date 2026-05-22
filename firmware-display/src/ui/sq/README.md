# Inversa Display — SquareLine Studio integration

This directory is the drop point for the SquareLine Studio export. While
empty (just this README + `sq_ui.h`), the firmware uses the handwritten
LVGL screens under `src/screens/*.cpp` as a fallback. Once you drop a real
export here and flip `USE_SQUARELINE_UI` on, those handwritten files
become inert (they still link in, but `screen_create()` short-circuits to
the SquareLine-generated objects).

## TL;DR workflow

1. Open SquareLine Studio (LVGL v9 target — Arduino with TFT_eSPI, or any
   "Pure LVGL" target works).
2. Set the project resolution to **172 × 320, portrait**.
3. For each screen / overlay in the table below, create the screen and
   name every widget exactly as documented. SquareLine emits one global
   per widget named `ui_<Screen>_<widget>`.
4. **Export → UI Files**: drop the entire output into this directory
   (overwriting `sq_ui.h` is fine — the export should regenerate it via
   `ui.h` declarations).
5. Edit `platformio.ini` and uncomment `-D USE_SQUARELINE_UI`.
6. `pio run -e waveshare_s3_lcd147 -t upload`.

## Build flag

```
[env:waveshare_s3_lcd147]
build_flags =
    ...
    -D USE_SQUARELINE_UI
    -I src/ui/sq
```

With the flag on, each `<name>_screen_create()` calls `ui_init()` once
(global no-op if already done) and binds the local static pointers to
the `ui_<Screen>_<widget>` externs declared in `sq_ui.h`. With the flag
off, the handwritten LVGL widgets are used.

## Required widget naming

The C bindings reach into widgets by **fixed names**. If you rename
`lblTemp` to `tempLabel` in SquareLine, the build link-errors. Keep names
verbatim.

### Home screen — `ui_Home`
| Name        | Type     | Purpose                              |
|-------------|----------|--------------------------------------|
| `lblTemp`   | Label    | Big current temp, e.g. "65.4°"       |
| `lblTarget` | Label    | "alvo 67.0°"                         |
| `lblMode`   | Label    | "Idle" / "Receita" / "Manual"        |
| `lblClock`  | Label    | HH:MM (RTC, asterisk if unsynced)    |
| `arc`       | Arc      | Ramp progress, 0–100                 |
| `dotHeater` | Object   | 10 × 10 pill, red when heater on     |
| `dotPump`   | Object   | 10 × 10 pill, blue when pump on      |

### Recipe screen — `ui_Recipe`
| Name           | Type   | Purpose                                  |
|----------------|--------|------------------------------------------|
| `lblName`      | Label  | Recipe name (e.g. "American Pale Ale")   |
| `lblStepIdx`   | Label  | "Passo 3/7"                              |
| `lblStep`      | Label  | Step name (e.g. "Mash – Sacarificação")  |
| `lblTemp`      | Label  | Current temp                             |
| `lblTarget`    | Label  | Target temp                              |
| `bar`          | Bar    | Step progress 0–100 (hidden during boil) |
| `lblBoil`      | Label  | "Fervura 45:12" (visible only on boil)   |
| `btnRow`       | Object | Container with the three buttons         |
| `btnConfirm`   | Button | Sends recipe:confirm (shown on WAIT_CONFIRM) |
| `btnPause`     | Button | Sends recipe:pause or :resume            |
| `lblBtnPause`  | Label  | Inside btnPause; text toggles Play/Pause |
| `btnStop`      | Button | Sends recipe:stop                        |

### Manual screen — `ui_Manual`
| Name         | Type   | Purpose                              |
|--------------|--------|--------------------------------------|
| `lblTemp`    | Label  | Current temp                         |
| `lblTarget`  | Label  | Target temp readout                  |
| `slider`     | Slider | Range 22..100; release → set_temp    |
| `switch`     | Switch | Heater toggle                        |

### Scheduled screen — `ui_Scheduled`
| Name           | Type   | Purpose                              |
|----------------|--------|--------------------------------------|
| `lblTarget`    | Label  | "07:30"                              |
| `lblCountdown` | Label  | "em 2h05m"                           |
| `lblTemp`      | Label  | "alvo 65°"                           |
| `lblVol`       | Label  | "vol 20L"                            |
| `lblStatus`    | Label  | Free text from controller            |
| `btnStop`      | Button | Sends sched:stop                     |

### HopAlert overlay — `ui_HopAlert`
| Name          | Type   | Purpose                              |
|---------------|--------|--------------------------------------|
| `lblName`     | Label  | Hop name                             |
| `lblMin`      | Label  | "marca: 60 min"                      |
| `btnConfirm`  | Button | Pops the alert from the queue        |

### Watchdog overlay — `ui_Watchdog`
| Name        | Type   | Purpose                              |
|-------------|--------|--------------------------------------|
| `lblCause`  | Label  | Cause string from controller         |
| `lblHard`   | Label  | "hard-stop 105°"                     |
| `btnReset`  | Button | Sends watchdog:reset                 |

### Scanning screen — `ui_Scanning` (optional)
This one is fine to leave handwritten — the export is optional. If you do
export, name the root `Scanning` so `ui_Scanning` resolves.

## Notes about the SquareLine export

- The handwritten code never touches widget styling, layout, fonts or
  colors — those are 100% yours in SquareLine. The C code only reads
  state from `AppState` and writes values + show/hide flags.
- Button event callbacks are attached **here** in the C++ code, not in
  SquareLine. Do not assign event handlers in SquareLine; we use
  `lv_obj_add_event_cb` after binding. If SquareLine adds its own
  handlers, the firmware ones still run on top, so behavior is OK, but
  the duplicate is wasted RAM.
- Fonts must be Montserrat 14 / 24 / 48 (or whatever you pick in
  SquareLine — labels resize themselves, but check truncation on 172 px
  width).
- For modal overlays (`ui_HopAlert`, `ui_Watchdog`) the bridge calls
  `lv_obj_move_foreground()` after `lv_obj_set_parent(lv_layer_top())`.
  Don't fight that in SquareLine — just design them as regular screens.

## Falling back

If something breaks after a SquareLine export, remove `-D USE_SQUARELINE_UI`
from `platformio.ini` and you immediately revert to the handwritten
screens. No code edits needed.
