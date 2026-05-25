#!/usr/bin/env python3
"""Inversa BLE debug CLI.

Single entrypoint with subcommands for scanning the brewer, querying its
state over the Nordic UART Service (NUS) characteristics, pushing WiFi
credentials, and live-monitoring sensor / watchdog transitions.

Examples:
  ./inversa_ble.py scan
  ./inversa_ble.py info
  ./inversa_ble.py temp
  ./inversa_ble.py wifi-status
  ./inversa_ble.py wifi-config MyNet hunter2
  ./inversa_ble.py wifi-reconnect
  ./inversa_ble.py watchdog-reset
  ./inversa_ble.py monitor --secs 90
  ./inversa_ble.py adc-monitor --secs 30
"""
from __future__ import annotations

import argparse
import asyncio
import json
import sys
import time
from typing import Any, Awaitable, Callable, Optional

from bleak import BleakClient, BleakScanner

# Nordic UART Service (matches firmware/src/core/constants.h)
SERVICE = "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
RX_UUID = "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"  # host → device (write)
TX_UUID = "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"  # device → host (notify)

DEFAULT_SCAN_SECS = 8.0
DEFAULT_NAME_HINTS = ("Inversa", "inversa")


# ─── Connection helpers ──────────────────────────────────────────────

async def find_device(scan_secs: float = DEFAULT_SCAN_SECS, hints=DEFAULT_NAME_HINTS):
    """Discover the first BLE peripheral whose name contains an Inversa hint."""
    devs = await BleakScanner.discover(timeout=scan_secs)
    for d in devs:
        name = (d.name or "").lower()
        if any(h.lower() in name for h in hints):
            return d
    return None


class Session:
    """Open BLE session: scan, connect, subscribe TX notify, queue JSON frames."""

    def __init__(self, client: BleakClient):
        self.client = client
        self.q: asyncio.Queue[dict] = asyncio.Queue()
        self._buf = bytearray()

    @classmethod
    async def open(cls, scan_secs: float = DEFAULT_SCAN_SECS) -> Optional["Session"]:
        dev = await find_device(scan_secs)
        if not dev:
            return None
        client = BleakClient(dev.address)
        await client.__aenter__()
        sess = cls(client)
        await client.start_notify(TX_UUID, sess._on_notify)
        return sess

    async def close(self):
        try:
            await self.client.stop_notify(TX_UUID)
        except Exception:
            pass
        await self.client.__aexit__(None, None, None)

    def _on_notify(self, _handle, data: bytearray):
        # NUS notifies can split across MTU boundaries; the firmware writes one
        # JSON per logical message but the controller may chunk it. Accumulate
        # until a parseable object emerges, then drop the buffer.
        self._buf.extend(data)
        try:
            obj = json.loads(self._buf.decode("utf-8", errors="replace"))
        except json.JSONDecodeError:
            return
        self._buf.clear()
        try:
            self.q.put_nowait(obj)
        except asyncio.QueueFull:
            pass

    async def send(self, payload: dict) -> None:
        data = json.dumps(payload).encode()
        await self.client.write_gatt_char(RX_UUID, data, response=False)

    async def request(self, payload: dict, wait_rid: Optional[str] = None,
                      timeout: float = 6.0,
                      stream: bool = False,
                      until: Optional[Callable[[dict], bool]] = None) -> Optional[dict]:
        """Send and optionally wait for a correlated reply (`rid`) or any event
        that matches the `until(msg) -> bool` predicate. With stream=False
        returns the first matching message; with stream=True yields them via
        the caller's predicate side effects until timeout.
        """
        await self.send(payload)
        if wait_rid is None and until is None:
            return None
        deadline = asyncio.get_event_loop().time() + timeout
        while True:
            remaining = deadline - asyncio.get_event_loop().time()
            if remaining <= 0:
                return None
            try:
                msg = await asyncio.wait_for(self.q.get(), timeout=remaining)
            except asyncio.TimeoutError:
                return None
            if wait_rid is not None and msg.get("rid") == wait_rid:
                return msg
            if until is not None and until(msg):
                return msg

    async def drain(self, secs: float, on_msg: Callable[[dict, float], None]):
        """Pump messages for `secs` seconds, calling `on_msg(msg, t)` for each."""
        t0 = time.monotonic()
        end = t0 + secs
        while time.monotonic() < end:
            try:
                msg = await asyncio.wait_for(
                    self.q.get(),
                    timeout=max(0.05, end - time.monotonic()),
                )
            except asyncio.TimeoutError:
                continue
            on_msg(msg, time.monotonic() - t0)


# ─── Subcommands ─────────────────────────────────────────────────────

async def cmd_scan(args):
    devs = await BleakScanner.discover(timeout=args.secs)
    for d in devs:
        print(f"{d.address}  {d.name!r}")
    matches = [d for d in devs if any(h.lower() in (d.name or "").lower()
                                       for h in DEFAULT_NAME_HINTS)]
    if not matches:
        print("[!] No Inversa device found.")
        return 1
    return 0


async def with_session(coro):
    sess = await Session.open()
    if not sess:
        print("[!] No Inversa device advertising.")
        return 1
    try:
        return await coro(sess)
    finally:
        await sess.close()


async def cmd_info(args):
    async def run(s: Session):
        r = await s.request({"tp": "req:info", "rid": "i"}, wait_rid="i", timeout=6.0)
        if not r:
            print("[!] No reply.")
            return 1
        print(json.dumps(r, indent=2))
        return 0
    return await with_session(run)


async def cmd_temp(args):
    async def run(s: Session):
        r = await s.request({"tp": "req:status", "rid": "s"},
                            until=lambda m: m.get("tp") == "evt:status",
                            timeout=6.0)
        if not r:
            print("[!] No status received.")
            return 1
        wd = r.get("wd", {}) or {}
        print(f"current   = {r.get('ct')} °C")
        print(f"setpoint  = {r.get('tt')} °C")
        print(f"adc_raw   = {r.get('adc')}")
        print(f"tempSenOk = {r.get('tso')}")
        print(f"heater    = {r.get('h')}")
        print(f"mode      = {r.get('m')}")
        print(f"watchdog  = tripped={wd.get('t')} cause={wd.get('lc')} count={wd.get('c')}")
        return 0
    return await with_session(run)


async def cmd_wifi_status(args):
    async def run(s: Session):
        r = await s.request({"tp": "req:wifi:status", "rid": "w"},
                            until=lambda m: m.get("tp") in ("evt:wifi:status", "res:wifi:status"),
                            timeout=6.0)
        if not r:
            print("[!] No wifi status received.")
            return 1
        print(json.dumps(r, indent=2))
        return 0
    return await with_session(run)


async def cmd_wifi_config(args):
    async def run(s: Session):
        payload = {"tp": "req:wifi:config", "ssid": args.ssid,
                   "pwd": args.password, "rid": "cfg"}
        r = await s.request(payload, wait_rid="cfg", timeout=6.0)
        if not r or r.get("tp") != "res:ok":
            print(f"[!] Rejected: {r}")
            return 2
        print(f"[+] Credentials saved. Triggering connect...")
        r2 = await s.request({"tp": "req:wifi:connect", "rid": "conn"},
                             wait_rid="conn", timeout=6.0)
        if not r2 or r2.get("tp") != "res:ok":
            print(f"[!] Connect failed: {r2}")
            return 3
        end = asyncio.get_event_loop().time() + 25
        while asyncio.get_event_loop().time() < end:
            try:
                msg = await asyncio.wait_for(s.q.get(),
                                             timeout=end - asyncio.get_event_loop().time())
            except asyncio.TimeoutError:
                break
            if msg.get("tp") == "evt:wifi:status" and msg.get("ip"):
                print(f"[✓] Connected: ssid={msg.get('ssid')!r} ip={msg.get('ip')}")
                return 0
        print("[!] Timed out waiting for IP.")
        return 4
    return await with_session(run)


async def cmd_wifi_reconnect(args):
    async def run(s: Session):
        r = await s.request({"tp": "req:wifi:connect", "rid": "c"},
                            wait_rid="c", timeout=6.0)
        if not r or r.get("tp") != "res:ok":
            print(f"[!] Reconnect rejected: {r}")
            return 2
        end = asyncio.get_event_loop().time() + 25
        while asyncio.get_event_loop().time() < end:
            try:
                msg = await asyncio.wait_for(s.q.get(),
                                             timeout=end - asyncio.get_event_loop().time())
            except asyncio.TimeoutError:
                break
            if msg.get("tp") == "evt:wifi:status" and msg.get("ip"):
                print(f"[✓] {msg}")
                return 0
        print("[!] No IP within window.")
        return 3
    return await with_session(run)


async def cmd_watchdog_reset(args):
    async def run(s: Session):
        r = await s.request({"tp": "req:watchdog:reset", "rid": "r"},
                            wait_rid="r", timeout=6.0)
        if not r:
            print("[!] No reply.")
            return 1
        print(json.dumps(r))
        return 0 if r.get("tp") == "res:ok" else 2
    return await with_session(run)


async def cmd_monitor(args):
    """Stream evt:status and log transitions (sensor flap, watchdog flip,
    heater toggle, watchdog count). Issues a reset on entry so the first
    transition isn't drowned out by the pre-existing latch.
    """
    async def run(s: Session):
        await s.send({"tp": "req:watchdog:reset", "rid": "r"})
        state = {"adc": None, "tso": None, "ct": None,
                 "wd_t": None, "wd_c": None, "wd_lc": None, "heater": None}
        print("# t        ct     adc   tso    wd.t  wd.c  heater  wd.lc")

        def on(msg, t):
            tp = msg.get("tp", "")
            if tp == "evt:watchdog:tripped":
                print(f"  {t:6.2f}s  🔥 TRIPPED cause={msg.get('cause')} temp={msg.get('temp')}")
                return
            if tp == "evt:watchdog:reset":
                print(f"  {t:6.2f}s  ⚙ reset auto={msg.get('auto')}")
                return
            if tp != "evt:status":
                return
            ct = msg.get("ct"); adc = msg.get("adc"); tso = msg.get("tso")
            heater = msg.get("h")
            wd = msg.get("wd", {}) or {}
            wd_t = wd.get("t"); wd_c = wd.get("c"); wd_lc = wd.get("lc")
            curr = {"adc": adc, "tso": tso, "ct": ct, "wd_t": wd_t,
                    "wd_c": wd_c, "wd_lc": wd_lc, "heater": heater}
            if any(state[k] != curr[k] for k in ("adc", "tso", "wd_t", "wd_c", "heater")):
                print(f"  {t:6.2f}s  {ct!s:>5}  {adc!s:>4}  {tso!s:>5}  "
                      f"{wd_t!s:>5}  {wd_c!s:>3}  {heater!s:>5}   {wd_lc}")
                state.update(curr)

        await s.drain(args.secs, on)
        return 0
    return await with_session(run)


async def cmd_adc_monitor(args):
    """Lower-level loop targeting NTC sensor diagnostics: only the ADC
    raw value, tempSensorOk flag and the watchdog SENSOR_FAULT path. Use
    while physically prodding the NTC connector — the ADC value updates
    on every status sample (~1 Hz).
    """
    async def run(s: Session):
        await s.send({"tp": "req:watchdog:reset", "rid": "r"})
        last = {"adc": None, "tso": None, "wd_t": None, "wd_c": None}
        print("# t        ct     adc   tso    wd.t  wd.c  wd.lc")

        def on(msg, t):
            tp = msg.get("tp", "")
            if tp == "evt:watchdog:tripped":
                print(f"  {t:6.2f}s  🔥 TRIPPED cause={msg.get('cause')}")
                return
            if tp == "evt:watchdog:reset":
                print(f"  {t:6.2f}s  ⚙ reset")
                return
            if tp != "evt:status":
                return
            ct = msg.get("ct"); adc = msg.get("adc"); tso = msg.get("tso")
            wd = msg.get("wd", {}) or {}
            wd_t = wd.get("t"); wd_c = wd.get("c"); wd_lc = wd.get("lc")
            curr = {"adc": adc, "tso": tso, "wd_t": wd_t, "wd_c": wd_c}
            if any(last[k] != curr[k] for k in last):
                print(f"  {t:6.2f}s  {ct!s:>5}  {adc!s:>4}  {tso!s:>5}  "
                      f"{wd_t!s:>5}  {wd_c!s:>3}   {wd_lc}")
                last.update(curr)

        await s.drain(args.secs, on)
        return 0
    return await with_session(run)


# ─── argparse wiring ─────────────────────────────────────────────────

def build_parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(description="Inversa BLE debug CLI")
    sub = p.add_subparsers(dest="cmd", required=True)

    s_scan = sub.add_parser("scan", help="BLE scan, list peripherals")
    s_scan.add_argument("--secs", type=float, default=DEFAULT_SCAN_SECS)
    s_scan.set_defaults(func=cmd_scan)

    sub.add_parser("info", help="Firmware version, build, heap").set_defaults(func=cmd_info)
    sub.add_parser("temp", help="Current temperature + ADC + sensor + watchdog snapshot").set_defaults(func=cmd_temp)
    sub.add_parser("wifi-status", help="WiFi connection state").set_defaults(func=cmd_wifi_status)

    s_wifi = sub.add_parser("wifi-config", help="Push SSID + password and connect")
    s_wifi.add_argument("ssid")
    s_wifi.add_argument("password")
    s_wifi.set_defaults(func=cmd_wifi_config)

    sub.add_parser("wifi-reconnect", help="Re-trigger WiFi connect").set_defaults(func=cmd_wifi_reconnect)
    sub.add_parser("watchdog-reset", help="Clear watchdog latch").set_defaults(func=cmd_watchdog_reset)

    s_mon = sub.add_parser("monitor", help="Stream + log transitions")
    s_mon.add_argument("--secs", type=int, default=60)
    s_mon.set_defaults(func=cmd_monitor)

    s_adc = sub.add_parser("adc-monitor", help="ADC + sensor fault diagnostics")
    s_adc.add_argument("--secs", type=int, default=30)
    s_adc.set_defaults(func=cmd_adc_monitor)

    return p


def main() -> int:
    args = build_parser().parse_args()
    return asyncio.run(args.func(args))


if __name__ == "__main__":
    sys.exit(main() or 0)
