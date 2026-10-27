#!/usr/bin/env bash
# Emulator smoke test: boot the real firmware image (bootloader + partition table + otadata + app)
# in esp32sim on an emulated ESP32-C6 with nothing attached, and check that it reaches the expected
# no-hardware failsafe state without crashing.
#
#   smoke.sh run   --emu BIN --rom ELF (--build DIR | --image BIN [--elf ELF]...) [--seconds N]
#                  [--live-ms MS] [--timeout S] [--log FILE] [--stub SYM[=VAL]]...
#   smoke.sh check LOG [LIVE_MS]
#
# Pass/fail is decided from the log, never from esp32sim's exit code: esp32sim exits 0 on every
# stop reason (panic, reset, unimplemented instruction). The allow-list line `[emu] stop: Halted`
# (cycle budget reached) is what proves the run was not cut short.
#
# Liveness: the newest "sht40: I2C error" line (printed by the control task, AppTask, every sensor
# period when no SHT40 answers) must be at or after LIVE_MS (default: run length minus 3 s). Keyed
# to the control task on purpose: the display and Zigbee tasks keep logging when AppTask is stuck.
# esp32sim does not model the C6 task/interrupt watchdogs, so this is the only hang detector.
#
# DIR is an idf.py build directory (or the firmware-emu artifact) with flash_args; --image is an
# already merged 8 MB flash image (CI merges it in the IDF container, so no esptool here).
# The image must be built with CONFIG_PM_ENABLE off (sdkconfig.variant).
#
# Recorded PM-off boot (same 7000 ms liveness threshold as a 10 s run):
#   smoke.sh check test/emu/fixtures/pm-off-boot.log 7000
# fixtures/pm-off-boot.log was captured from the variant image at 28dab7e
# (App version 0.3.0-g28dab7e) and normalised to LF. To re-record it, run
# `smoke.sh run` against a PM-off image (`--build build-variant`, or a merged
# `--image`), strip CR with `sed 's/\r$//'`, and replace the fixture.
set -euo pipefail

REQUIRED=(
    'app_init: Project name: +thermostat_v3$'
    'evt: EVT cfg loaded '
    'evt: EVT ctrl fault=SENSOR FAIL$'
    'E \([0-9]+\) epd: BUSY timeout$'
    'display: refresh failed: ESP_ERR_TIMEOUT$'
    'zb: EVT zb retry in [0-9]+ ms \(steering failed'
    '^\[emu\] stop: Halted '
)
FORBIDDEN=(
    'Guru Meditation'
    "panic'ed"
    'abort\(\) was called'
    'ESP_ERROR_CHECK failed'
    'assert failed'
    'Stack (canary|protection)|stack overflow'
    'register dump:'
    '^Backtrace:'
    'Task watchdog got triggered|task_wdt: |Interrupt wdt timeout'
    'CORRUPT HEAP|heap corruption'
    'Rebooting\.\.\.'
    '\[emu\] (chip reset|restart:|unimplemented|last bus fault)'
    '^E \([0-9]+\) (boot|esp_image):'
    'load partition table error|Failed to verify partition table|No bootable app partitions'
    'EVT fault (sensor|display|buttons) init|EVT safe mode|app task: out of memory|unhealthy: outputs off'
)

die() { echo "smoke: $*" >&2; exit 2; }
tmpfiles=()
cleanup() { [ ${#tmpfiles[@]} -eq 0 ] || rm -f "${tmpfiles[@]}"; }
trap cleanup EXIT

check_log() {
    local log=$1 live_ms=${2:-0} clean rc=0 re last n
    [ -s "$log" ] || die "empty or missing log: $log"
    clean=$(mktemp); tmpfiles+=("$clean")
    sed 's/\x1b\[[0-9;]*m//g; s/\r$//' "$log" > "$clean"
    for re in "${FORBIDDEN[@]}"; do
        if grep -Eq -- "$re" "$clean"; then
            echo "FAIL forbidden: /$re/"
            grep -En -m3 -- "$re" "$clean" | sed 's/^/    /' || true
            rc=1
        fi
    done
    # Exactly one ROM boot: a second one is a reset that slipped past --no-reboot.
    n=$(grep -c '^rst:0x' "$clean" || true)
    if [ "$n" -ne 1 ]; then echo "FAIL boots: $n ROM 'rst:' lines, want exactly 1"; rc=1; fi
    for re in "${REQUIRED[@]}"; do
        if grep -Eq -- "$re" "$clean"; then
            echo "ok   $(grep -E -m1 -- "$re" "$clean" | cut -c1-110)"
        else
            echo "FAIL missing: /$re/"
            rc=1
        fi
    done
    last=$({ grep -E '^W \([0-9]+\) sht40: I2C error$' "$clean" || true; } \
           | sed -E 's/^W \(([0-9]+)\).*/\1/' | sort -n | tail -n 1)
    last=${last:-0}
    if [ "$last" -ge "$live_ms" ]; then
        echo "ok   alive: control task last logged at ${last} ms (need >= ${live_ms})"
    else
        echo "FAIL hung: control task last logged at ${last} ms, need >= ${live_ms} (light sleep left on? AppTask stuck?)"
        rc=1
    fi
    if [ "$rc" -eq 0 ]; then echo "smoke: PASS"; else echo "smoke: FAIL (full log: $log)"; fi
    return "$rc"
}

# App-task stack free (bytes) on the 10 s boot log, sensor absent. 5248 B leaves 1652 B.
# A 4608 B stack would leave 1012 B, and 4096 B about 500 B, so a floor of 1400 fails both.
# This log does not open the menu. Device info is not measured: a MODE/DOWN script does not
# reach that screen in esp32sim, and the boot frame already builds the same app-task rows.
# Factory reset is a second DisplayModel (1076 B free). `run` measures it with hold_factory,
# floor 800, which fails a 4608 B stack (~436 B) and a 4096 B stack (overflow).
check_stack() {
    local log=$1 clean line free
    clean=$(mktemp); tmpfiles+=("$clean")
    sed 's/\x1b\[[0-9;]*m//g; s/\r$//' "$log" > "$clean"
    line=$(grep -E 'EVT stack boot free=[0-9]+' "$clean" | head -n 1 || true)
    if [ -z "$line" ]; then
        echo "FAIL missing: EVT stack boot free="
        return 1
    fi
    free=$(printf '%s\n' "$line" | sed -E 's/.*free=([0-9]+).*/\1/')
    if [ "$free" -ge 1400 ]; then
        echo "ok   stack boot free=${free} B (need >= 1400)"
        return 0
    fi
    echo "FAIL stack: ${free} B free at boot, need >= 1400 (4096 or 4608 B app stack)"
    return 1
}

# Hold MODE for 16 s from the home screen so FactoryReset runs. That frame is a second
# DisplayModel on the app task, which is the deepest point. The mark is logged before the
# restart. 1076 B free on a 5248 B stack; 800 B still passes, and 4608 B (~436 B) or
# 4096 B (overflow) does not.
hold_factory() {
    local emu=$1 rom=$2 image=$3 script flog clean line free rc=0
    shift 3
    script=$(mktemp); tmpfiles+=("$script")
    flog=$(mktemp); tmpfiles+=("$flog")
    printf '3.0 press 3 16000\n' > "$script"
    local args=(--chip c6 --rom "$rom" --boot rom --flash-mb 8 --flash-image "$image"
                --console usb --max-seconds 21 --no-reboot --no-dump --script "$script" "$@")
    echo "smoke: factory-reset stack: $emu ${args[*]}"
    timeout --kill-after=10 90 "$emu" "${args[@]}" > "$flog" 2>&1 || rc=$?
    if [ "$rc" -eq 124 ] || [ "$rc" -eq 137 ]; then
        tail -n 20 "$flog"
        echo "smoke: FAIL (factory-reset wall-clock timeout, rc $rc)"
        return 1
    fi
    clean=$(mktemp); tmpfiles+=("$clean")
    sed 's/\x1b\[[0-9;]*m//g; s/\r$//' "$flog" > "$clean"
    if grep -Eq 'Guru Meditation|panic.ed|stack overflow|Stack protection|Stack canary' "$clean"; then
        echo "FAIL factory-reset run hit a fault"
        grep -En -m3 'Guru Meditation|panic|stack overflow|Stack ' "$clean" | sed 's/^/    /' || true
        return 1
    fi
    line=$(grep -E 'EVT stack factory free=[0-9]+' "$clean" | head -n 1 || true)
    if [ -z "$line" ]; then
        echo "FAIL missing: EVT stack factory free="
        tail -n 15 "$clean"
        return 1
    fi
    free=$(printf '%s\n' "$line" | sed -E 's/.*free=([0-9]+).*/\1/')
    if [ "$free" -ge 800 ]; then
        echo "ok   stack factory free=${free} B (need >= 800)"
        return 0
    fi
    echo "FAIL stack: ${free} B free at factory reset, need >= 800 (4096 or 4608 B app stack)"
    return 1
}

run() {
    local emu='' rom='' build='' image='' seconds=10 live_ms='' wall=300 log='' elf rc=0
    local -a stubs=() elfs=()
    while [ $# -gt 0 ]; do
        [ $# -ge 2 ] || die "$1 needs a value"
        case $1 in
            --emu) emu=$2 ;; --rom) rom=$2 ;; --build) build=$2 ;; --image) image=$2 ;;
            --elf) elfs+=(--elf "$2") ;; --seconds) seconds=$2 ;; --live-ms) live_ms=$2 ;;
            --timeout) wall=$2 ;; --log) log=$2 ;; --stub) stubs+=(--stub "$2") ;;
            *) die "unknown option $1" ;;
        esac
        shift 2
    done
    [[ $seconds =~ ^[0-9]+$ && $seconds -ge 4 ]] || die "--seconds: whole number >= 4"
    [ -x "$emu" ] || die "--emu: not an executable: $emu"
    [ -f "$rom" ] || die "--rom: no such file: $rom"
    if [ -n "$build" ]; then
        [ -f "$build/flash_args" ] || die "--build: no flash_args in $build"
        build=$(cd "$build" && pwd)
        image=$(mktemp --suffix=.bin); tmpfiles+=("$image")
        (cd "$build" && "${ESPTOOL:-esptool.py}" --chip esp32c6 merge_bin -o "$image" \
            --fill-flash-size 8MB @flash_args) || die "esptool merge_bin failed"
        for elf in "$build"/*.elf "$build"/bootloader/bootloader.elf; do
            if [ -f "$elf" ]; then elfs+=(--elf "$elf"); fi
        done
    fi
    [ -f "$image" ] || die "need --build DIR or --image BIN"
    log=${log:-${build:-.}/emu-smoke.log}

    local args=(--chip c6 --rom "$rom" --boot rom --flash-mb 8 --flash-image "$image"
                --console usb --max-seconds "$seconds" --no-reboot --no-dump
                "${elfs[@]}" "${stubs[@]}")
    echo "smoke: $emu ${args[*]}"
    timeout --kill-after=10 "$wall" "$emu" "${args[@]}" > "$log" 2>&1 || rc=$?
    if [ "$rc" -eq 124 ] || [ "$rc" -eq 137 ]; then
        tail -n 40 "$log"
        echo "smoke: FAIL (wall-clock timeout ${wall}s, rc $rc)"
        return 1
    fi
    [ "$rc" -eq 0 ] || echo "smoke: emulator exited with $rc"
    local crc=0 src=0 frc=0
    check_log "$log" "${live_ms:-$(( seconds * 1000 - 3000 ))}" || crc=$?
    check_stack "$log" || src=$?
    if [ "$rc" -eq 0 ] && [ "$crc" -eq 0 ] && [ "$src" -eq 0 ]; then
        hold_factory "$emu" "$rom" "$image" "${elfs[@]}" || frc=$?
    fi
    [ "$rc" -eq 0 ] && [ "$crc" -eq 0 ] && [ "$src" -eq 0 ] && [ "$frc" -eq 0 ]
}

case ${1:-} in
    run) shift; run "$@" ;;
    check) [ $# -ge 2 ] || die "usage: $0 check LOG [LIVE_MS]"; check_log "$2" "${3:-0}" ;;
    *) die "usage: $0 run --emu BIN --rom ELF (--build DIR | --image BIN) [...] | $0 check LOG [LIVE_MS]" ;;
esac
