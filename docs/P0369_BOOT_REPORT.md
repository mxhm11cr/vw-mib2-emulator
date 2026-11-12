# P0369 Boot Report — authentic execution tracking

Firmware: MST2_EU_VW_ZR_P0369T (TechniSat MIB2 Standard ZR).
Hardware (firmware-verified): NXP i.MX6, Cortex-A9, ARMv7-A 32-bit LE, QNX Neutrino IFS.

Rule: binary parsing ≠ execution; loading bytes ≠ boot; a generic Linux/QNX boot ≠ a VW boot.
Only authentic VW instruction execution counts, with reproducible trace evidence.

| Milestone | Status | Evidence |
|---|---|---|
| Firmware archive found | Done | 3,803,093,204 B; SHA-256 36bc458c… |
| Archive integrity verified | Done | 7z CRC "Everything is Ok" |
| Firmware extracted | Done | 920 files / 4,025,406,943 B; original unchanged |
| Firmware architecture identified | Done | EM_ARM(40) 32-bit; i.MX6 + QNX strings/drivers |
| Original executable located | Done | mibstd2_cpu.boot (QNX IFS, 5,513,828 B, SHA-256 20e1fd46…) |
| Authentic instructions executed | Pending | — |
| Bootloader reached | Pending | — |
| Kernel started | Pending | — |
| Original UI displayed | Pending | — |

## Execution attempt plan (Phase 5)
1. Parse QNX IFS header in mibstd2_cpu.boot; resolve entry point + load address; identify any
   TechniSat secure wrapper (magic 58 21 54 00 is not a standard ARM branch).
2. Load authentic image into QEMU i.MX6 (sabrelite, Cortex-A9) or a Cortex-A9 machine; set PC
   to the authentic entry; enable `-d in_asm,cpu,int` tracing and serial capture.
3. Capture: register state, instruction trace, memory/MMIO accesses, exceptions, console.
4. Record last successfully executed authentic instruction; classify the exact blocker.

## Current blocker
IFS entry/load address and secure-header layout not yet resolved; QEMU lacks a VW-MIB i.MX6
board model. No authentic execution claimed yet.
