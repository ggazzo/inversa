#!/usr/bin/env bash
# serial-capture.sh — capture firmware serial output to a timestamped
# log file while highlighting known disconnect-related lines.
#
# Used to triage "BLE disconnects when opening a recipe". Run it BEFORE
# reproducing the bug. Press Ctrl+C to stop; the log path is printed
# at exit.
#
#   ./tools/serial-capture.sh                       # auto-pick /dev/cu.usbmodem*
#   ./tools/serial-capture.sh /dev/cu.usbmodem2101  # explicit port
#
# Implementation note: we deliberately do NOT use `pio device monitor`
# here. Miniterm (which pio shells out to) requires a controlling TTY
# (`termios.tcgetattr`), so it fails when launched headless from a
# background runner. Plain `stty + cat` doesn't need a TTY, gives us
# the same raw stream, and composes with awk for annotation + tee for
# logging.

set -euo pipefail

PORT="${1:-}"
if [[ -z "${PORT}" ]]; then
    # Pick the first /dev/cu.usbmodem* present. Bluetooth-Incoming-Port
    # and debug-console always exist and aren't us, so they're skipped
    # by the glob.
    for candidate in /dev/cu.usbmodem*; do
        if [[ -e "${candidate}" ]]; then PORT="${candidate}"; break; fi
    done
fi

if [[ -z "${PORT}" || ! -e "${PORT}" ]]; then
    echo "[serial-capture] no /dev/cu.usbmodem* device found — plug it in" >&2
    exit 1
fi

LOG_DIR="${INVERSA_LOG_DIR:-/tmp}"
TS="$(date +%Y%m%d-%H%M%S)"
LOG="${LOG_DIR}/inversa-serial-${TS}.log"

echo "[serial-capture] port : ${PORT}"
echo "[serial-capture] log  : ${LOG}"
echo "[serial-capture] hint : reproduce the disconnect, then Ctrl+C"
echo "[serial-capture]        suspicious lines re-printed in red"
echo

# Configure the port: 115200 baud, raw mode, no local echo. On macOS
# `stty -f` is the right syntax (Linux uses `stty -F`).
stty -f "${PORT}" 115200 cs8 -cstopb -parenb raw -echo -echoe -echok -icanon -icrnl -ixon -ixoff

# `cat` opens the device for read. The line-buffered awk filter
# timestamps each line and highlights interesting ones; tee mirrors
# everything raw into the logfile.
cat "${PORT}" \
    | awk -v start="$(perl -MTime::HiRes=time -e 'printf "%.3f", time')" '
        BEGIN {
            red    = "\033[31m";
            yellow = "\033[33m";
            reset  = "\033[0m";
        }
        {
            cmd = "perl -MTime::HiRes=time -e \"printf \\\"%.3f\\\", time\""
            cmd | getline now; close(cmd);
            ms = int((now - start) * 1000);
            printf "[%6d ms] ", ms;
            if ($0 ~ /notify dropped chunk|command dropped|disconnect|reason=|supervision|stack trace|abort|Guru Meditation|WDT/) {
                print red $0 reset;
            } else if ($0 ~ /\[BLE\]/) {
                print yellow $0 reset;
            } else {
                print $0;
            }
            fflush();
        }
    ' \
    | tee "${LOG}"
