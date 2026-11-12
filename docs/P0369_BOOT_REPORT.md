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

## Execution attempts (Phase 5 — evidence, honest)
Tool: `scripts/run-authentic.sh` (QEMU `sabrelite`, Cortex-A9, generic loader, `-d in_asm,cpu,int`).
Image loaded at the i.MX6 RAM address `0x10800000` (from the IFS header field at file 0x110).

Observed (trace `docs/evidence/p0369-authentic-trace.txt`):
- Authentic firmware instructions DO execute, e.g. `0x10800190: eb0020c4 bl #0x108084a8`
  with `R14=0x10800194` correctly set, then conditional ops at the target.
- However each candidate entry soon takes `Undefined Instruction` (exception 1) and falls
  through the empty vector table (runaway). The BL targets land in data, not code.

**Conclusion:** authentic instruction execution is demonstrated, but the **true QNX IFS boot
entry point and load mapping are not yet resolved**, so this is NOT a boot and NOT counted as
the "authentic instructions executed" boot milestone. The image has a TechniSat secure header
(magic 0x00542158 = IFS size; file_size − 0x10C) followed by a QNX IFS whose header/entry must
be parsed with the QNX IFS (`dumpifs`) format rather than guessed.

## Current blocker (precise)
1. Resolve the QNX IFS header → exact startup entry (`startup_vaddr`) + load address
   (`image_paddr`). The firmware's own `dumpifs` confirms these fields exist, but the QNX
   `IMAGE_SIGNATURE`/`STARTUP_SIGNATURE` constants (QNX `sys/image.h`) are required to locate
   the header. Empirical isolation (tools/ifs-sig.js cross-referencing dumpifs) is exhausted:
   the header is NOT in the first 0x400 bytes (TechniSat secure wrapper), and whole-image
   constant matching only yields coincidental ARM-code overlaps. **Next: obtain the QNX IFS
   header signature constants / sys/image.h, then parse startup_vaddr format-correctly.**
2. QEMU has no VW-MIB i.MX6 board; full boot needs custom peripherals (UART, GIC/AVIC, timers,
   eMMC/SDHCI, IPU display) and the QNX boot flow.
Blind entry guessing is discontinued (no new evidence); resume only with the header definition.
