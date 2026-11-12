# MIB2 Boot Progress Report (separate from MIB3)

Priority target: **MIB2 Standard — TI Jacinto 6, Cortex-A15 (ARMv7-A), Linux.**
Second target: MIB2 High — Harman MHI2/MHI2Q, QNX.

Rule: a generic Linux/QNX boot is **NOT** a VW firmware boot. A custom splash or recreated
UI is **NOT** a milestone. Only authentic VW code execution counts as VW progress.

## Emulation environment (verified)
- QEMU 11.1.0 (official Windows build, SHA-512 verified), extracted locally, no admin.
- Reference machine `vexpress-a15` (Cortex-A15) — core-compatible with MIB2 Standard.
  QEMU has **no Jacinto/DRA7 machine**; authentic boot needs a custom DRA7 device model.

## Progress table

| Stage | Status | Evidence | Blocker |
|---|---|---|---|
| Hardware/ISA identified (Standard) | **Verified** | TI DRA746 page + TI SSZTBM2 article (Cortex-A15, ARMv7-A) | — |
| Hardware/ISA identified (High) | Reported | M.I.B. wiki (Harman MHI2/QNX); exact Tegra/QNX ver unknown | authentic image |
| Emulation env built | **Verified** | QEMU 11.1.0 runs; `vexpress-a15` listed | — |
| Openly-licensed reference boot (TEST, not VW) | **Verified** | docs/evidence/mib2-reference-boot.log: Linux 6.12 armmp, CPU ARMv7 [414fc0f0] Cortex-A15, V2P-CA15, PL011 ttyAMA0, Run /init | — |
| Authentic VW firmware identified | Pending | — | no image yet |
| Authentic VW firmware inspected | Pending | — | no image yet |
| Authentic VW CPU execution | Pending | — | no image yet |
| Authentic VW bootloader reached | Pending | — | no image yet |
| Authentic VW kernel reached | Pending | — | no image yet |
| Authentic VW services initialized | Pending | — | no image yet |
| Original VW UI rendered | Pending | — | no image yet |

## Verified reference-test evidence (NOT a Volkswagen boot)
Command: `scripts/run-reference.sh` (QEMU vexpress-a15, Cortex-A15, Debian armhf netboot).
Serial log (267 lines) includes:
- `Booting Linux on physical CPU 0x0`
- `Linux version 6.12.107+deb13-armmp ... arm-linux-gnueabihf-gcc-14 ... SMP Debian`
- `CPU: ARMv7 Processor [414fc0f0] revision 0 (ARMv7)`  (0x414fc0f0 = Cortex-A15)
- `OF: fdt: Machine model: V2P-CA15`
- `hw perfevents: enabled with armv7_cortex_a15 PMU driver`
- `1c090000.serial: ttyAMA0 ... is a PL011 rev1` / `printk: legacy console [ttyAMA0] enabled`
- `Freeing unused kernel image (initmem) memory: 2048K` / `Run /init as init process`

This proves the emulated Cortex-A15 CPU, GIC, timers, memory and PL011 UART execute real
AArch32 code. It is a CPU/device validation test using GPL Linux, clearly distinct from any
VW firmware boot.

## Missing artifact (precise)
An authentic, legally obtained **MIB2 Standard** (TechniSat/Preh/Delphi) or **MIB2 High**
(Harman MHI2/MHI2Q) update package or flash dump, with train/version. See firmware-analysis.md.
No authentic VW execution is claimed until such an image is inspected and run.
