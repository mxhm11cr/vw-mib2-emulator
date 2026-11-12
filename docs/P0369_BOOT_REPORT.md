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

## RESOLVED — startup header & entry established reliably (verified)
Using the documented QNX `sys/startup.h` / `sys/image.h` layouts (user-provided sources) and
`STARTUP_HDR_SIGNATURE=0x00FF7EEB`, `tools/ifs-parse.js` locates and validates the authentic
startup header at file offset **0x122**:
- signature 0x00FF7EEB ✓, version 1 ✓, header_size 256 ✓, **machine 0x28 = 40 (EM_ARM)** ✓
- **startup_vaddr = 0x10805FB4** (entry), **image_paddr = 0x10800008** (load), paddr_bias 0
- startup_size 0x1E108, stored_size 0xB0003, imagefs_size 0x40008; imagefs @ file 0x12C10.
- A second coincidental signature @0x3C1414 is correctly **rejected** (invalid version/machine).
- Parser tests **3/3 pass** (independently-constructed reference header + validator rejection +
  authentic firmware entry). The TechniSat wrapper occupies file 0x00–0x121; QNX image starts 0x122.

## Authentic execution (verified, early startup — NOT a full boot)
`scripts/run-startup.sh` extracts a derived QNX image copy (original preserved), loads it at the
aligned image_paddr 0x10800008 on QEMU sabrelite (Cortex-A9), transfers to startup_vaddr 0x10805FB4.
Trace (docs/evidence/p0369-authentic-startup-trace.txt) shows authentic firmware instructions:
```
0x10805fb4: e1d421b0  ldrh r2, [r4, #0x10]
0x10805fb8: e1d431b2  ldrh r3, [r4, #0x12]
0x10805fbc: fed20133  mrc2 p1, #6, r0, c2, c3, #1   -> Undefined Instruction
```
This is genuine authentic VW firmware code executing at the reliably-established entry.

## Current blocker (precise, next)
1. The startup expects IPL-provided register state (r4 = hardware/info base; here r4=0) and
   accesses a coprocessor (`mrc2 p1,#6,r0,c2,c3,#1`) QEMU's Cortex-A9 does not implement →
   Undefined Instruction at 0x10805fbc. To advance legitimately: model the QNX ARM IPL→startup
   register handoff and the specific cp1 register (virtual hardware), NOT inject values into
   the firmware. The exact ARM entry register convention and cp1 meaning are NOT in the QNX
   IPL/startup docs reviewed, and vocho/openqnx has no hardware/startup source. **Unblock needs:
   QNX ARM startup assembly source (startup/lib/arm) and/or i.MX6 cp1 register definition.**
2. QEMU has no VW-MIB i.MX6 board; deeper boot needs UART/GIC/timers/eMMC/IPU modeling.

## Integrity note (honest)
QNX documented checksum = sum32(image_paddr, startup_size) and sum32(+startup_size,
stored_size-startup_size). Computed: startup sum32=0x1927e749 (trailer 0xe6ff3073), imagefs
sum32=0x7859c904 (trailer 0x8000bc10) — neither is zero under simple-sum. The image is a
TechniSat signed wrapper (`cpuimage_sec`); the original mkifs checksum likely no longer
applies after re-wrapping. QNX-checksum integrity is therefore NOT claimed as verified.
The startup_header itself validates fully (signature/version/machine/entry), and authentic
code executes. stored_size=0xB0003 covers only the first ~720 KiB; the 5.5 MiB file holds
additional data beyond the QNX image (further structure to map).
No secure-boot/integrity bypass performed; images executed unmodified (derived copy for loading).
