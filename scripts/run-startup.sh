#!/usr/bin/env bash
# Reproducible authentic QNX startup execution for MIB2 P0369 (read-only on original).
#
# VERIFIED alignment (empirical scan of load bases): the QNX image must be loaded at
#   LOAD_BASE = image_paddr - 6 = 0x10800002
# so that the canonical QNX ARM _start prologue (push {r0-r15} = 0xe92dffff) lands exactly
# at _start = image_paddr + header_size = 0x10800108. Other bases misdecode.
# The TechniSat wrapper stores the header at file 0x122; the QNX image is extracted from
# there into a derived copy (original preserved).
#
# NOTE: alignment is NON-UNIFORM across the image (the startup_vaddr / bl-target regions
# decode only under different byte shifts) -> a section-based TechniSat transformation is
# present; a single load base coheres _start but not the whole image. See boot report.
set -uo pipefail
WIN="F:/vw-mib2-emulator"
Q="$WIN/tools/qemu/extracted/qemu-system-arm.exe"
B="$WIN/firmware/extracted/P0369/cpu/cpuimage_sec_stdNavi/33/default/mibstd2_cpu.boot"
O="$WIN/firmware/analysis/exec"; mkdir -p "$O"
node -e "const fs=require('fs');const b=fs.readFileSync('$B');fs.writeFileSync('$O/qnx-image.bin',b.subarray(0x122));"
LOAD_BASE="${LOAD_BASE:-0x10800002}"
PC="${PC:-0x10800108}"   # _start (verified coherent); header startup_vaddr=0x10805fb4
rm -f "$O/trace.log" "$O/serial.log"
"$Q" -M sabrelite -cpu cortex-a9 -m 512 \
  -device loader,file="$O/qnx-image.bin",addr="$LOAD_BASE" \
  -device loader,cpu-num=0,addr="$PC" \
  -d in_asm,cpu,int -D "$O/trace.log" \
  -serial "file:$O/serial.log" -display none -monitor none -no-reboot &
QP=$!; sleep "${SECS:-5}"; kill "$QP" 2>/dev/null || true
taskkill //F //IM qemu-system-arm.exe >/dev/null 2>&1 || true
echo "=== authentic instructions (load=$LOAD_BASE pc=$PC) ==="; grep -E '^0x' "$O/trace.log" | head -8
echo "=== first exception ==="; grep -m1 "Taking exception" "$O/trace.log" || echo "  NONE"
