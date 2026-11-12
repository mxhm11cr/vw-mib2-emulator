#!/usr/bin/env bash
# Attempt authentic VW MIB2 firmware execution on QEMU i.MX6 (sabrelite, Cortex-A9).
# Loads the authentic QNX IFS boot image (mibstd2_cpu.boot) at the i.MX6 RAM address
# found in its header (0x10800000) and traces CPU instructions. Executes authentic firmware
# code; NOT a full VW boot (custom VW-MIB peripherals are not yet modeled).
set -uo pipefail
WIN="F:/vw-mib2-emulator"   # Windows-style paths for the native QEMU executable
Q="$WIN/tools/qemu/extracted/qemu-system-arm.exe"
B="$WIN/firmware/extracted/P0369/cpu/cpuimage_sec_stdNavi/33/default/mibstd2_cpu.boot"
OUT="$WIN/firmware/analysis/exec"; mkdir -p "$OUT"
LOADADDR="${LOADADDR:-0x107ffef0}"   # IFS (file off 0x110) linked at 0x10800000
ENTRY="${ENTRY:-0x10800000}"
SECS="${SECS:-8}"
rm -f "$OUT/trace.log" "$OUT/serial.log"
echo "load=$LOADADDR entry=$ENTRY"
"$Q" -M sabrelite -cpu cortex-a9 -m 512 \
  -device loader,file="$B",addr="$LOADADDR" \
  -device loader,cpu-num=0,addr="$ENTRY" \
  -d in_asm,cpu,int -D "$OUT/trace.log" \
  -serial "file:$OUT/serial.log" -display none -monitor none -no-reboot &
QPID=$!
sleep "$SECS"
kill "$QPID" 2>/dev/null || true
taskkill //F //IM qemu-system-arm.exe >/dev/null 2>&1 || true
echo "=== trace size ==="; wc -l "$OUT/trace.log" 2>/dev/null
echo "=== first instructions ==="; head -45 "$OUT/trace.log" 2>/dev/null
echo "=== serial ==="; head -10 "$OUT/serial.log" 2>/dev/null
