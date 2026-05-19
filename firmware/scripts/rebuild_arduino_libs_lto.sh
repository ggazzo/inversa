#!/usr/bin/env bash
# rebuild_arduino_libs_lto.sh
#
# Rebuild the Espressif Arduino libs with LTO enabled and drop the
# resulting archives into the PlatformIO cache so this project can be
# built with `-flto`. See firmware/docs/LTO_CUSTOM_LIBS.md for the why,
# the gotchas, and the rationale for the CMake patch this script
# applies — it's not just "set a knob in sdkconfig" because that knob
# doesn't exist in ESP-IDF v5.4.
#
# Usage:
#   firmware/scripts/rebuild_arduino_libs_lto.sh [esp32s3,esp32c3,...]
#
# Defaults to esp32s3,esp32c3. IDF tag matches the pioarduino pin in
# firmware/platformio.ini (54.03.21-2 → arduino-esp32 v3.2.1 / IDF
# v5.4.2).
#
# Requires: Docker daemon running, ~6 GB free disk, ~90 min first run.
#
# Status: not validated end-to-end as of this commit. The first
# revision used CONFIG_COMPILER_OPTIMIZATION_LTO=y in
# configs/sdkconfig.defaults and produced archives without any
# `.gnu.lto_*` sections — the symbol simply isn't in ESP-IDF v5.4's
# Kconfig. This version patches the lib-builder CMakeLists.txt
# instead, which is the documented way to inject build flags into
# every IDF component, but the resulting archives still need
# verification before they're useful.

set -euo pipefail

# ── Config ──────────────────────────────────────────────────────────
LIB_BUILDER_REPO="https://github.com/espressif/esp32-arduino-lib-builder.git"
LIB_BUILDER_REF="release/v5.4"          # matches IDF v5.4.x
LIB_BUILDER_IMAGE="espressif/esp32-arduino-lib-builder:release-v5.4"
PIO_LIBS_DIR="${HOME}/.platformio/packages/framework-arduinoespressif32-libs"
WORK_DIR="${WORK_DIR:-${TMPDIR:-/tmp}/inversa-lto-libs}"
CHIPS_DEFAULT="esp32s3,esp32c3"
CHIPS="${1:-${CHIPS_DEFAULT}}"

# Color helpers — only when stdout is a TTY.
if [[ -t 1 ]]; then
    R=$'\033[0;31m'; G=$'\033[0;32m'; Y=$'\033[1;33m'; B=$'\033[0;34m'; N=$'\033[0m'
else
    R= G= Y= B= N=
fi

log()  { printf "%s==>%s %s\n" "$B" "$N" "$*"; }
warn() { printf "%s[!]%s %s\n"  "$Y" "$N" "$*" >&2; }
die()  { printf "%s[x]%s %s\n"  "$R" "$N" "$*" >&2; exit 1; }
ok()   { printf "%s✓%s %s\n"   "$G" "$N" "$*"; }

# ── Sanity checks ───────────────────────────────────────────────────
command -v docker >/dev/null || die "docker not found"
command -v rsync  >/dev/null || die "rsync not found"
docker info >/dev/null 2>&1 || die "docker daemon not running (start Docker Desktop first)"
[[ -d "$PIO_LIBS_DIR" ]] || die "PlatformIO arduino-esp32-libs not found at $PIO_LIBS_DIR (run \`pio run\` once first)"

log "Chips:           $CHIPS"
log "Lib builder ref: $LIB_BUILDER_REF"
log "Docker image:    $LIB_BUILDER_IMAGE"
log "Work dir:        $WORK_DIR"
log "PIO libs target: $PIO_LIBS_DIR"
echo

# ── Clone / update lib-builder ──────────────────────────────────────
mkdir -p "$WORK_DIR"
LIB_BUILDER_DIR="$WORK_DIR/esp32-arduino-lib-builder"
if [[ ! -d "$LIB_BUILDER_DIR/.git" ]]; then
    log "Cloning $LIB_BUILDER_REPO …"
    git clone "$LIB_BUILDER_REPO" "$LIB_BUILDER_DIR"
fi
log "Checking out $LIB_BUILDER_REF"
git -C "$LIB_BUILDER_DIR" fetch origin
git -C "$LIB_BUILDER_DIR" checkout "$LIB_BUILDER_REF"
git -C "$LIB_BUILDER_DIR" pull --ff-only origin "$LIB_BUILDER_REF" || true

# ── Patch CMakeLists.txt to inject LTO into every IDF component ─────
#
# The "obvious" approach — `CONFIG_COMPILER_OPTIMIZATION_LTO=y` in
# configs/sdkconfig.defaults — does NOT work on ESP-IDF v5.4. That
# symbol isn't in the Kconfig and the build silently ignores it,
# producing archives without `.gnu.lto_*` sections. Forcing the
# flags via `idf_build_set_property(... APPEND)` right after
# `project(...)` is the supported escape hatch.
#
# We append `-ffat-lto-objects` alongside `-flto` so each `.o`
# carries both GIMPLE bytecode AND native code; otherwise components
# that don't get LTO can't link against the resulting `.a`.
CMAKE_FILE="$LIB_BUILDER_DIR/CMakeLists.txt"
[[ -f "$CMAKE_FILE" ]] || die "Expected $CMAKE_FILE — lib-builder layout changed?"

if ! grep -q "Inversa: force LTO" "$CMAKE_FILE"; then
    log "Patching $CMAKE_FILE to enable LTO across all components"
    python3 - "$CMAKE_FILE" <<'PY'
import sys, re
path = sys.argv[1]
with open(path, 'r') as f:
    src = f.read()
patch = (
    '\n'
    '# Inversa: force LTO + fat-LTO across every IDF component so the\n'
    '# resulting archives carry both GIMPLE bytecode and native code.\n'
    '# CONFIG_COMPILER_OPTIMIZATION_LTO is NOT a real Kconfig symbol in\n'
    '# ESP-IDF v5.4, so the only way to flip LTO on is to inject the\n'
    '# flags here, after project() has registered the IDF build env.\n'
    'idf_build_set_property(COMPILE_OPTIONS "-flto;-ffat-lto-objects" APPEND)\n'
    'idf_build_set_property(LINK_OPTIONS    "-flto;-fuse-linker-plugin" APPEND)\n'
)
m = re.search(r'^project\([^\)]*\)\s*$', src, re.M)
if not m:
    sys.exit("could not find project(...) line to patch")
src2 = src[:m.end()] + patch + src[m.end():]
with open(path, 'w') as f:
    f.write(src2)
PY
    ok "CMakeLists.txt patched"
else
    ok "CMakeLists.txt already patched"
fi

# ── Pre-pull Docker image (so progress logs are inline) ─────────────
log "Pulling $LIB_BUILDER_IMAGE (one-time)"
docker pull "$LIB_BUILDER_IMAGE"

# ── Per-chip build inside container ─────────────────────────────────
# `-t a,b,c` (comma-separated) is the supported way to build multiple
# targets in one invocation. `-t a -t b` only honours the last one
# (getopts overwrites OPTARG).
log "Building libs for $CHIPS via Docker (this is the slow part — 30-60 min/chip)"
docker run --rm \
    -v "$LIB_BUILDER_DIR":/lib-builder \
    -w /lib-builder \
    -e LIBBUILDER_GIT_SAFE_DIR=/lib-builder \
    "$LIB_BUILDER_IMAGE" \
    ./build.sh -t "$CHIPS"

# ── Sanity check — confirm LTO sections are present ─────────────────
# This catches the case where the CMake patch silently failed and we
# ended up with archives identical to what pioarduino ships.
log "Verifying GIMPLE/LTO sections in produced archives"
IFS=',' read -r -a CHIP_ARR <<< "$CHIPS"
LTO_FAIL=0
for CHIP in "${CHIP_ARR[@]}"; do
    case "$CHIP" in
        esp32s3|esp32|esp32s2)
            OBJDUMP="$HOME/.platformio/packages/toolchain-xtensa-esp-elf/bin/xtensa-${CHIP}-elf-objdump"
            ;;
        *)
            OBJDUMP="$HOME/.platformio/packages/toolchain-riscv32-esp/bin/riscv32-esp-elf-objdump"
            ;;
    esac
    LIB_PATH="$LIB_BUILDER_DIR/out/tools/esp32-arduino-libs/$CHIP/lib/libfreertos.a"
    [[ -f "$LIB_PATH" ]] || LIB_PATH="$LIB_BUILDER_DIR/out/tools/esp32-arduino-libs/$CHIP/qio_qspi/libfreertos.a"
    if [[ ! -f "$LIB_PATH" ]]; then
        warn "$CHIP: libfreertos.a not found, skipping LTO verification"
        continue
    fi
    if [[ ! -x "$OBJDUMP" ]]; then
        warn "$CHIP: objdump $OBJDUMP not installed; skipping LTO verification"
        continue
    fi
    COUNT=$("$OBJDUMP" -h "$LIB_PATH" 2>/dev/null | grep -c "\.gnu\.lto" || true)
    if [[ "$COUNT" -gt 0 ]]; then
        ok "$CHIP libfreertos.a carries $COUNT GIMPLE/LTO sections"
    else
        warn "$CHIP libfreertos.a has 0 GIMPLE/LTO sections — LTO did NOT take effect"
        LTO_FAIL=1
    fi
done
if [[ "$LTO_FAIL" -ne 0 ]]; then
    die "LTO verification failed — see firmware/docs/LTO_CUSTOM_LIBS.md \"Solução de problemas\""
fi

# ── Install into PIO cache ──────────────────────────────────────────
BACKUP_TAG="backup-pre-lto"
BACKUP_DIR="$PIO_LIBS_DIR.$BACKUP_TAG-$(date +%Y%m%d-%H%M%S)"
if [[ ! -e "$PIO_LIBS_DIR.$BACKUP_TAG" ]]; then
    log "Backing up original libs to $BACKUP_DIR"
    cp -r "$PIO_LIBS_DIR" "$BACKUP_DIR"
    ln -snf "$(basename "$BACKUP_DIR")" "$PIO_LIBS_DIR.$BACKUP_TAG"
fi
for CHIP in "${CHIP_ARR[@]}"; do
    OUT_DIR="$LIB_BUILDER_DIR/out/tools/esp32-arduino-libs/$CHIP"
    [[ -d "$OUT_DIR" ]] || { warn "$OUT_DIR missing — skipping install"; continue; }
    log "Installing LTO-enabled libs for $CHIP into PIO cache"
    rsync -a --delete "$OUT_DIR/" "$PIO_LIBS_DIR/$CHIP/"
    ok "$CHIP installed"
done

echo
ok "Done. Next steps:"
cat <<'TIPS'

  1. Re-enable -flto in firmware/platformio.ini:
       [common_esp].build_flags += -flto -fuse-linker-plugin
     and drop the "LTO stays off" comment block, or leave a short
     breadcrumb mentioning the rebuild was applied.

  2. Clean rebuild + verify sizes:
       cd firmware
       pio run -e wemos_s3_mini -t clean && pio run -e wemos_s3_mini
       pio run -e wemos_c3_mini -t clean && pio run -e wemos_c3_mini

  3. Compare .flash.text / .iram0.text via objdump -h against the
     previous baseline. Use objdump, not `size` — `size` lies about
     PSRAM-mapped builds.

  4. Hardware smoke test (boot, BLE, recipe, OTA, free heap stability).

  5. If you ever change the pioarduino version pin in platformio.ini,
     PIO will overwrite the cache with upstream (non-LTO) archives.
     Re-run this script to re-apply.

  Restore the original libs at any time:
       rm -rf "$PIO_LIBS_DIR" && \
       cp -r "$PIO_LIBS_DIR.backup-pre-lto" "$PIO_LIBS_DIR"

TIPS
