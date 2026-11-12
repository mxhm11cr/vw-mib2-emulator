# PROGRESS — VW MIB2 real firmware boot (P0369)

Target (firmware-verified): **TechniSat MIB2 Standard ZR — NXP i.MX6 (Cortex-A9, ARMv7-A 32-bit),
QNX Neutrino IFS.** Firmware: `MST2_EU_VW_ZR_P0369T_3G0919360LN.7z`.

| Milestone | Status | Evidence |
|---|---|---|
| Firmware archive found | **Done** | firmware/MST2_EU_VW_ZR_P0369T_3G0919360LN.7z (3,803,093,204 B) |
| Archive integrity verified | **Done** | SHA-256 36bc458c…; 7z "Everything is Ok" (CRC) |
| Firmware extracted | **Done** | extracted/P0369: 920 files / 1224 folders / 4,025,406,943 B; original preserved |
| Firmware architecture identified | **Done** | ELF EM_ARM 32-bit; i.MX6 (vpu_fw_imx6q.bin, dev-can-mx6x); QNX (ldqnx, dumpifs, IFS) |
| Original executable located | **Done** | cpu/cpuimage_sec_stdNavi/33/default/mibstd2_cpu.boot (QNX IFS, 5,513,828 B) |
| Authentic instructions executed | **Verified (_start)** | load=0x10800002 → authentic `push {r0-r15}; movt; bl` at _start 0x10800108, SVC32 (docs/evidence/p0369-startup-verified.txt) |
| Startup header / entry established reliably | **Yes** | startup_vaddr=0x10805FB4, image_paddr=0x10800008, machine=40(ARM); parser tests 3/3; correct load base 0x10800002 verified by scan |
| Full-image coherent load | Blocked | non-uniform TechniSat section transformation; _start coheres, bl-target/startup_vaddr need per-section byte ordering |
| Bootloader reached | Pending | |
| Kernel started | Pending | |
| Original UI displayed | Pending | |

## Verified facts
- SWDL update package (category B) containing authentic QNX IFS boot images + BSP userland.
- CPU = ARMv7-A 32-bit Cortex-A9 (i.MX6), OS = QNX Neutrino. Corrects earlier TI Jacinto guess.
- Firmware is signed (module.info RSA Signature); protections documented, not defeated.

## Blockers / next
- Determine QNX IFS entry/load address and any TechniSat secure header in mibstd2_cpu.boot.
- QEMU has no VW-MIB i.MX6 board; need i.MX6 (sabrelite) + custom peripherals for full boot.
- Next: parse IFS header, attempt authentic instruction execution with trace evidence.

## Session log
- 2026-11-12: Located/verified/extracted archive; identified i.MX6/Cortex-A9/QNX from firmware;
  wrote P0369_FIRMWARE_ANALYSIS.md; corrected hardware research. No authentic execution yet.
