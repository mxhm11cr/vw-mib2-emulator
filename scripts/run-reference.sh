#!/usr/bin/env bash
# Reproducible CPU/device validation test for the MIB2 Standard target architecture
# (ARMv7-A Cortex-A15). Boots a real, openly-licensed (GPL) Debian armhf Linux image
# on QEMU vexpress-a15 and captures the serial console.
#
# THIS IS A REFERENCE TEST, NOT A VOLKSWAGEN BOOT. It produces no VW interface and
# executes no VW firmware. It only validates that the emulated Cortex-A15 CPU, GIC,
# timers and PL011 UART execute real code correctly.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
Q="$ROOT/tools/qemu/extracted"
REF="$ROOT/firmware/reference"
DTB_URL="https://deb.debian.org/debian/dists/stable/main/installer-armhf/current/images/device-tree/vexpress-v2p-ca15-tc1.dtb"
mkdir -p "$REF"
[ -f "$REF/vmlinuz" ] || curl -sSL -o "$REF/vmlinuz" https://deb.debian.org/debian/dists/stable/main/installer-armhf/current/images/netboot/vmlinuz
[ -f "$REF/initrd.gz" ] || curl -sSL -o "$REF/initrd.gz" https://deb.debian.org/debian/dists/stable/main/installer-armhf/current/images/netboot/initrd.gz
[ -f "$REF/vexpress-v2p-ca15-tc1.dtb" ] || curl -sSL -o "$REF/vexpress-v2p-ca15-tc1.dtb" "$DTB_URL"
rm -f "$REF/reference-boot.log"
"$Q/qemu-system-arm.exe" -M vexpress-a15 -cpu cortex-a15 -m 1024 \
  -kernel "$REF/vmlinuz" -initrd "$REF/initrd.gz" -dtb "$REF/vexpress-v2p-ca15-tc1.dtb" \
  -append "console=ttyAMA0" -display none -serial file:"$REF/reference-boot.log" -monitor none -no-reboot &
QPID=$!
sleep "${BOOT_SECONDS:-70}"
kill "$QPID" 2>/dev/null || true
taskkill //F //IM qemu-system-arm.exe >/dev/null 2>&1 || true
echo "--- evidence (grep) ---"
grep -aiE 'Linux version|CPU: ARMv7|Machine model|cortex_a15|PL011|console \[ttyAMA0\]|Run /init' "$REF/reference-boot.log" | head -12
