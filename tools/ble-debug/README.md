# ble-debug

Python BLE CLI for poking the Inversa firmware over the Nordic UART Service
(NUS) characteristics it already exposes (same UUIDs the web/mobile app uses).
Useful when:

- The display is glitching or you want a second opinion on what the device
  reports.
- The brewer is far enough that the LCD is hard to read but BLE still works.
- You need to capture sensor / watchdog state changes during a bug repro.
- WiFi credentials need to change without the mobile app handy.

This is a **diagnostic tool**, not part of the production wire. It speaks
the same JSON protocol as `firmware/src/protocol/protocol.h`.

## Install

macOS / Linux (Python 3.11+):

```bash
python3 -m venv .venv
source .venv/bin/activate
pip install -r requirements.txt
```

macOS only: grant the terminal **Bluetooth** access in
`System Settings → Privacy & Security → Bluetooth` (the OS will prompt on the
first scan).

## Usage

All commands auto-scan for the first peripheral whose name contains
`Inversa`. Run `--help` after any subcommand for flags.

| Command | What it does |
|---|---|
| `scan` | BLE scan, list every advertiser. Confirms the brewer is visible. |
| `info` | `req:info` → firmware version, build timestamp, env name (`name`), free heap. |
| `temp` | One-shot `req:status` → current temp, setpoint, ADC raw, `tempSensorOk`, heater, mode, watchdog state. |
| `wifi-status` | `req:wifi:status` → connected, SSID, IP, configured SSID. |
| `wifi-config SSID PWD` | Saves credentials and triggers connect, prints the resulting IP. |
| `wifi-reconnect` | Re-issues `req:wifi:connect` (handy after the brewer drops the AP). |
| `watchdog-reset` | Clears the latched trip if temperature is below `hardStop − margin`. |
| `monitor --secs N` | Streams `evt:status` for N seconds, logs every transition (sensorOk, watchdog count/cause, heater toggle). Resets the latch on entry so the first transition isn't drowned out. |
| `adc-monitor --secs N` | Same idea but tighter: only ADC raw + `tempSensorOk` + watchdog. Use while wiggling the NTC connector to see live ADC. |

Examples:

```bash
./inversa_ble.py scan
./inversa_ble.py info
./inversa_ble.py temp
./inversa_ble.py wifi-config 'GZ_' 'hunter2'
./inversa_ble.py wifi-reconnect
./inversa_ble.py watchdog-reset
./inversa_ble.py monitor --secs 90
./inversa_ble.py adc-monitor --secs 30
```

## What the ADC value means

`temp` and `adc-monitor` print `adc` raw (0–4095) and `tso`
(`gState.tempSensorOk`). Quick lookup table for the high-side topology
(`NTC_HIGH_SIDE=1`):

| ADC | What's likely going on |
|---|---|
| `0` | NTC shorted to GND, or `R_ref` open between ADC and GND. |
| `4095` | NTC fully disconnected (open circuit). |
| ~150–250 stable | Pin floating or contact-resistance way too high — typical loose connector. |
| ~1500–2500 | Healthy mid-range (R_ntc roughly in the 5–20 kΩ band). |
| oscillating wildly | Intermittent contact in the NTC cable. |

If `tso == false` but the displayed `ct` looks plausible (e.g. 32 °C),
that temperature is **cached** — the firmware keeps the last valid reading
in `gState.currentTemp` and only overwrites it on a successful read. Trust
`tso` and `adc`, not `ct`, when diagnosing sensor problems.

## Limitations

- NimBLE in the firmware accepts **one** central at a time
  (`CONFIG_BT_NIMBLE_MAX_CONNECTIONS=1`). If the display (or the mobile app)
  is already connected, the brewer stops advertising and `scan` finds
  nothing. Disconnect the other client first.
- The CoreBluetooth backend on macOS occasionally races on the first
  characteristic-discovery after a fresh ad. Re-running the command usually
  succeeds.
- macOS sandboxing means the script cannot speak BLE if the terminal app
  hasn't been granted Bluetooth permission — the scan will just return
  zero peripherals.

## Files

- `inversa_ble.py` — the CLI (single file, no project layout).
- `requirements.txt` — pinned to `bleak`.
- `README.md` — you are here.
