# Inversa Display — SquareLine Studio widget blueprint

Per-screen widget recipe. Add each row in SquareLine Studio with the
**Object Name** verbatim, set the listed properties, leave everything
else default. All measurements are for a 172 × 320 portrait canvas.

**Global settings** (apply once before placing widgets):
- Project → Display: 172 × 320, color depth 16-bit (swap), portrait
- LVGL Version: 9.x
- Fonts (add in Project Settings → Fonts):
  - `Montserrat 14`
  - `Montserrat 24`
  - `Montserrat 48`
- Screen background border width: 0 (every screen)
- Screen scrollable: **OFF** (every screen — inspector "Flag → Scrollable")
- Screen padding: 8 px all sides (default in code), 10 px for Manual/Scheduled, 12 px for HopAlert

**General rules**:
- **Object Name** column is the inspector "Object Name" field. SquareLine
  emits `ui_<ScreenName>_<ObjectName>`. Match exactly — typos
  link-error at build.
- **Align** column maps to the inspector "Align" dropdown.
- **Offset** column is the X/Y offset relative to the alignment anchor
  (positive Y = down, negative = up).
- **No event handlers** in the Studio. The C++ code attaches them after
  binding the externs.

---

## Screen: `Home`

Screen background: solid `#0B0B0F`, opa cover.

| Object Name | Type    | Align       | Offset (X, Y) | Size (W, H) | Font  | Text color | Notes |
|-------------|---------|-------------|---------------|-------------|-------|------------|-------|
| `lblClock`  | Label   | TOP_MID     | 0, 0          | content     | M-14  | `#6B7280`  | text `"--:--"` |
| `arc`       | Arc     | TOP_MID     | 0, 22         | 150, 150    | —     | —          | see arc spec below |
| `lblTemp`   | Label   | TOP_MID     | 0, 72         | content     | M-48  | `#FFFFFF`  | text `"--"` |
| `lblTarget` | Label   | TOP_MID     | 0, 134        | content     | M-14  | `#6B7280`  | text `"alvo --"` |
| `lblMode`   | Label   | BOTTOM_MID  | 0, -50        | content     | M-24  | `#FFFFFF`  | text `"Idle"` |
| `dotHeater` | Object  | BOTTOM_MID  | -30, -16      | 10, 10      | —     | —          | bg `#1A1A22`, radius `LV_RADIUS_CIRCLE`, opa cover, border 0 |
| `dotPump`   | Object  | BOTTOM_MID  | 30, -16       | 10, 10      | —     | —          | bg `#1A1A22`, radius `LV_RADIUS_CIRCLE`, opa cover, border 0 |

### `arc` properties
- Range: 0 to 100
- Value: 0
- Background angles: start 135, end 45 (so opening faces down)
- Mode: NORMAL
- Knob: remove style (Studio: Knob → Style → remove); or set knob opa 0
- Clickable flag: **off**
- Main arc: width 10, color `#1A1A22`
- Indicator arc: width 10, color `#10B981`

---

## Screen: `Recipe`

Screen bg `#0B0B0F`. **Layout mode: Flex** (Studio: screen inspector
"Layout → Flex Flow: Column, Main align: Start, Cross align: Center,
Item align: Center; row pad 4 px"). Add the widgets in the order below
so flex stacks them top-to-bottom.

| Object Name      | Type    | Size (W, H) | Font  | Color     | Notes |
|------------------|---------|-------------|-------|-----------|-------|
| `lblStepIdx`     | Label   | content     | M-14  | `#6B7280` | text `"Passo --/--"` |
| `lblName`        | Label   | 156, content| M-14  | `#6B7280` | text `""`, long mode DOT |
| `lblStep`        | Label   | 156, content| M-24  | `#FFFFFF` | text `"Passo"`, long mode DOT |
| `lblTemp`        | Label   | content     | M-48  | `#FFFFFF` | text `"--"` |
| `lblTarget`      | Label   | content     | M-14  | `#6B7280` | text `"alvo --"` |
| `bar`            | Bar     | 156, 6      | —     | —         | range 0-100, value 0; main bg `#1A1A22`, indicator `#10B981` |
| `lblBoil`        | Label   | content     | M-24  | `#F59E0B` | text `""`, flag **Hidden ON** |
| `btnRow`         | Object  | 100% W, content H | — | — | Flex Row, main align SPACE_BETWEEN, items center; bg opa 0, border 0, pad 0; flag **Hidden ON** |
| ↳ `btnConfirm`   | Button (child of `btnRow`) | 50, 36 | — | bg `#10B981`, radius 6 | label child below |
|   ↳ `lblConfirm` | Label (child of `btnConfirm`) | content | M-14 | `#FFFFFF` | text `"OK"`, centered |
| ↳ `btnPause`     | Button (child of `btnRow`) | 50, 36 | — | bg `#F59E0B`, radius 6 | label child below |
|   ↳ `lblBtnPause`| Label (child of `btnPause`)   | content | M-14 | `#FFFFFF` | text `"Pause"`, centered |
| ↳ `btnStop`      | Button (child of `btnRow`) | 50, 36 | — | bg `#EF4444`, radius 6 | label child below |
|   ↳ `lblStop`    | Label (child of `btnStop`) | content | M-14 | `#FFFFFF` | text `"Stop"`, centered |

**Important**: the labels inside `btnConfirm` / `btnStop` can have any
name (`lblConfirm`, `lblStop`). Only `lblBtnPause` is referenced from
code (the Pause text toggles to "Play" at runtime). Other button labels
are static so their internal names do not matter.

---

## Screen: `Manual`

Screen bg `#0B0B0F`, padding 10 px. Layout: Flex Column, main align
SPACE_EVENLY, cross center, item center.

| Object Name | Type    | Size (W, H) | Font  | Color     | Notes |
|-------------|---------|-------------|-------|-----------|-------|
| `lblTitle`  | Label   | content     | M-14  | `#6B7280` | text `"Manual"` (name does not matter) |
| `lblTemp`   | Label   | content     | M-48  | `#FFFFFF` | text `"--"` |
| `lblTarget` | Label   | content     | M-24  | `#10B981` | text `"→ 65°"` |
| `slider`    | Slider  | 150, 22     | —     | —         | range 22-100, value 65; main bg `#1A1A22`, indicator `#10B981`, knob `#FFFFFF` |
| (row)       | Object  | content     | —     | —         | Flex Row, col pad 8 px, opa 0, border 0 |
| ↳ `lblResist` | Label (child of row) | content | M-14 | `#6B7280` | text `"Resistencia"` (name does not matter) |
| ↳ `switch`  | Switch (child of row) | 50, 26 | — | — | default off |

---

## Screen: `Scheduled`

Screen bg `#0B0B0F`, padding 10 px. Flex Column, main SPACE_EVENLY,
cross center, item center, row pad 6.

| Object Name      | Type    | Size (W, H) | Font  | Color     | Notes |
|------------------|---------|-------------|-------|-----------|-------|
| `lblTitle`       | Label   | content     | M-14  | `#3B82F6` | text `"🔔 Agendado"` (name not referenced) |
| `lblTarget`      | Label   | content     | M-48  | `#FFFFFF` | text `"--:--"` |
| `lblCountdown`   | Label   | content     | M-24  | `#10B981` | text `"--"` |
| `lblTemp`        | Label   | content     | M-14  | `#6B7280` | text `"alvo --°"` |
| `lblVol`         | Label   | content     | M-14  | `#6B7280` | text `"vol --L"` |
| `lblStatus`      | Label   | 150, content| M-14  | `#F59E0B` | text `""`, long mode DOT, text align center |
| `btnStop`        | Button  | 140, 44     | —     | —         | bg `#EF4444`, radius 6; label child `"Cancelar"`, M-14, `#FFFFFF`, centered |

---

## Screen: `HopAlert` (modal overlay)

Treat as a full-screen modal. Screen bg `#F59E0B`, opa cover, scrollable
off, border 0, padding 12 px. Flex Column, main center, cross center,
item center, row pad 10.

| Object Name        | Type    | Size (W, H) | Font  | Color     | Notes |
|--------------------|---------|-------------|-------|-----------|-------|
| `lblTitle`         | Label   | content     | M-24  | `#000000` | text `"ADICIONAR\nLUPULO"`, text align center (name not referenced) |
| `lblName`          | Label   | 140, content| M-24  | `#000000` | text `""`, long mode DOT, text align center |
| `lblMin`           | Label   | content     | M-14  | `#000000` | text `""` |
| `btnConfirm`       | Button  | 130, 56     | —     | —         | bg `#000000`, radius 8; label child `"CONFIRMAR"`, M-14, `#F59E0B`, centered |

The bridge will re-parent this screen to `lv_layer_top()` on first show,
so its on-canvas position in SquareLine does not matter — only sizes and
contents do.

---

## Screen: `Watchdog` (modal overlay)

Screen bg `#EF4444`, opa cover, scrollable off, border 0, padding 10 px.
Flex Column, main center, cross center, item center, row pad 8.

| Object Name      | Type    | Size (W, H) | Font  | Color     | Notes |
|------------------|---------|-------------|-------|-----------|-------|
| `lblIcon`        | Label   | content     | M-48  | `#FFFFFF` | text `LV_SYMBOL_WARNING` — in Studio paste `\xef\x80\x97` or pick "Warning" symbol from the LVGL icon picker (name not referenced) |
| `lblTitle`       | Label   | content     | M-24  | `#FFFFFF` | text `"WATCHDOG"` (name not referenced) |
| `lblCause`       | Label   | 150, content| M-14  | `#FFFFFF` | text `""`, long mode WRAP, text align center |
| `lblHard`        | Label   | content     | M-14  | `#FFFFFF` | text `""` |
| `btnReset`       | Button  | 130, 44     | —     | —         | bg `#000000`, radius 6; label child `"RESET"`, M-14, `#EF4444`, centered |

---

## After exporting

1. Studio → **Export → Export UI Files**. Confirm the path lands in
   `firmware-display/src/ui/sq/`.
2. The export should overwrite my hand-written `ui.c` with its own.
   `sq_ui.h` is **not** something SquareLine generates with that exact
   name — keep my `sq_ui.h` (it just re-declares the same externs that
   SquareLine's `ui.h` declares; the screens .cpp can keep including
   `sq_ui.h` — both headers will declare the same symbols, no conflict.
   If you prefer, swap `#include "../ui/sq/sq_ui.h"` to `"ui.h"` in
   each screen .cpp).
3. Keep `-D USE_SQUARELINE_UI` in `platformio.ini` ON.
4. `pio run -e waveshare_s3_lcd147 -t upload`.

### Sanity checks

- `pio run` succeeds — all link-required externs resolved.
- Boot serial shows the RGB smoke test, then transitions through
  Scanning → Home as soon as the controller connects.
- Tapping the screen in HopAlert/Watchdog state triggers the buttons
  (they end up as bridges to the same `ble_send_*` calls the
  handwritten path used).
