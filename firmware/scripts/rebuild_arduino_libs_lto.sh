#!/usr/bin/env bash
# rebuild_arduino_libs_lto.sh
#
# Rebuild the Espressif Arduino libs with LTO enabled and drop the
# resulting archives into the PlatformIO cache so this project can be
# built with `-flto`. See firmware/docs/LTO_CUSTOM_LIBS.md for the why
# and the surrounding context — this script is just the mechanical
# part of that doc.
#
# Usage:
#   firmware/scripts/rebuild_arduino_libs_lto.sh [esp32s3,esp32c3,...]
#
# If you pass no chip list, both targets used by this project are built
# (esp32s3 + esp32c3). Default IDF tag matches the pioarduino pin in
# firmware/platformio.ini (54.03.21-2 → arduino-esp32 v3.2.1 / IDF v5.4.2).
#
# Requires: Docker, ~6 GB free disk, ~90 min first run.

set -euo pipefail

# ── Config ──────────────────────────────────────────────────────────
LIB_BUILDER_REPO="https://github.com/espressif/esp32-arduino-lib-builder.git"
LIB_BUILDER_REF="release/v5.4"          # matches IDF v5.4.x
ARDUINO_TAG="3.2.1"                      # matches pioarduino 54.03.21-2
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

log() { printf "%s==>%s %s\n" "$B" "$N" "$*"; }
warn() { printf "%s[!]%s %s\n" "$Y" "$N" "$*" >&2; }
die() { printf "%s[x]%s %s\n" "$R" "$N" "$*" >&2; exit 1; }
ok() { printf "%s✓%s %s\n" "$G" "$N" "$*"; }

# ── Sanity checks ───────────────────────────────────────────────────
command -v docker >/dev/null || die "docker not found"
command -v rsync  >/dev/null || die "rsync not found"
[[ -d "$PIO_LIBS_DIR" ]] || die "PlatformIO arduino-esp32-libs not found at $PIO_LIBS_DIR (run \`pio run\` once first)"

log "Chips:           $CHIPS"
log "Lib builder ref: $LIB_BUILDER_REF (arduino-esp32 v$ARDUINO_TAG)"
log "Work dir:        $WORK_DIR"
log "PIO libs target: $PIO_LIBS_DIR"
echo

# ── Clone / update lib-builder ──────────────────────────────────────
mkdir -p "$WORK_DIR"
LIB_BUILDER_DIR="$WORK_DIR/esp32-arduino-lib-builder"
if [[ ! -d "$LIB_BUILDER_DIR/.git" ]]; then
    log "Cloning $LIB_BUILDER_REPO …"
    git clone --recursive "$LIB_BUILDER_REPO" "$LIB_BUILDER_DIR"
fi
log "Checking out $LIB_BUILDER_REF"
git -C "$LIB_BUILDER_DIR" fetch origin
git -C "$LIB_BUILDER_DIR" checkout "$LIB_BUILDER_REF"
git -C "$LIB_BUILDER_DIR" submodule update --init --recursive

# ── Enable LTO in sdkconfig.defaults ────────────────────────────────
SDKCONFIG_FILE="$LIB_BUILDER_DIR/configs/sdkconfig.defaults"
[[ -f "$SDKCONFIG_FILE" ]] || die "Expected $SDKCONFIG_FILE — lib-builder layout changed?"

if ! grep -q "^CONFIG_COMPILER_OPTIMIZATION_LTO=y" "$SDKCONFIG_FILE"; then
    log "Enabling CONFIG_COMPILER_OPTIMIZATION_LTO in sdkconfig.defaults"
    {
        echo ""
        echo "# Inversa: LTO override applied by rebuild_arduino_libs_lto.sh"
        echo "CONFIG_COMPILER_OPTIMIZATION_LTO=y"
    } >> "$SDKCONFIG_FILE"
else
    ok "LTO already enabled in sdkconfig.defaults"
fi

# ── Per-chip build ──────────────────────────────────────────────────
IFS=',' read -r -a CHIP_ARR <<< "$CHIPS"
for CHIP in "${CHIP_ARR[@]}"; do
    log "Building libs for $CHIP (this is the slow part)"
    (
        cd "$LIB_BUILDER_DIR"
        if [[ -x ./tools/docker-build.sh ]]; then
            ./tools/docker-build.sh -t "$CHIP"
        else
            ./build.sh -t "$CHIP"
        fi
    )

    OUT_DIR="$LIB_BUILDER_DIR/out/tools/esp32-arduino-libs/$CHIP"
    [[ -d "$OUT_DIR" ]] || die "Build for $CHIP produced no output at $OUT_DIR"

    BACKUP_DIR="$PIO_LIBS_DIR.backup-pre-lto-$(date +%Y%m%d-%H%M%S)"
    if [[ ! -d "$PIO_LIBS_DIR.backup-pre-lto" ]]; then
        log "Backing up original libs to $BACKUP_DIR"
        cp -r "$PIO_LIBS_DIR" "$BACKUP_DIR"
        ln -snf "$(basename "$BACKUP_DIR")" "$PIO_LIBS_DIR.backup-pre-lto"
    fi

    log "Installing LTO-enabled libs for $CHIP into PIO cache"
    rsync -a --delete \
        "$OUT_DIR/" \
        "$PIO_LIBS_DIR/$CHIP/"
    ok "$CHIP done"
done

echo
ok "All chips built. Next steps:"
cat <<'TIPS'

  1. Re-enable -flto in firmware/platformio.ini:
       [common_esp].build_flags += -flto -fuse-linker-plugin
       (drop the "LTO is omitted" comment)

  2. Clean rebuild + verify sizes:
       cd firmware
       pio run -e wemos_s3_mini -t clean && pio run -e wemos_s3_mini
       pio run -e wemos_c3_mini -t clean && pio run -e wemos_c3_mini

  3. Compare .flash.text / .iram0.text via objdump -h
     against the previous baseline.

  4. Hardware smoke test (boot, BLE, recipe, OTA, free heap stability).

  5. If you ever change the pioarduino version pin in platformio.ini,
     re-run this script — PlatformIO will overwrite the cache with the
     upstream (non-LTO) archives.

  Restore the original libs at any time with:
       rm -rf "$PIO_LIBS_DIR" && \
       cp -r "$PIO_LIBS_DIR.backup-pre-lto" "$PIO_LIBS_DIR"

TIPS
