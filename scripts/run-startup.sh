#!/usr/bin/env bash
# Reproducible authentic QNX startup execution for MIB2 P0369 (read-only on the original).
# Entry/load address come from the VALIDATED QNX startup_header (sys/startup.h):
#   startup_vaddr = 0x10805FB4 (entry), image_paddr = 0x10800008 (load), machine = 40 (ARM).
# The TechniSat wrapper precedes the QNX image; the QNX image begins at file offset 0x122.
# We extract a derived analysis copy (original preserved) and load it at the aligned
# image_paddr, then transfer to startup_vaddr on QEMU i.MX6 (sabrelite, Cortex-A9).
# This executes authentic firmware startup code; it is NOT a full VW boot.
set -uo pipefail
WIN="F:/vw-mib2-emulator"
Q="$WIN/tools/qemu/extracted/qemu-system-arm.exe"
B="$WIN/firmware/extracted/P0369/cpu/cpuimage_sec_stdNavi/33/default/mibstd2_cpu.boot"
O="$WIN/firmware/analysis/exec"; mkdir -p "$O"
# Extract QNX image (from startup header at 0x122) into a derived copy; original untouched.
node -e "const fs=require('fs');const b=fs.readFileSync('$B');fs.writeFileSync('$O/qnx-image.bin',b.subarray(0x122));"
rm -f "$O/trace.log" "$O/serial.log"
"$Q" -M sabrelite -cpu cortex-a9 -m 512 \
  -device loader,file="$O/qnx-image.bin",addr=0x10800008 \
  -device loader,cpu-num=0,addr=0x10805fb4 \
  -d in_asm,cpu,int -D "$O/trace.log" \
  -serial "file:$O/serial.log" -display none -monitor none -no-reboot &
QP=$!; sleep "${SECS:-6}"; kill "$QP" 2>/dev/null || true
taskkill //F //IM qemu-system-arm.exe >/dev/null 2>&1 || true
echo "=== authentic startup instructions ==="; head -8 "$O/trace.log"
echo "=== first exception ==="; grep -m1 "Taking exception" "$O/trace.log"
