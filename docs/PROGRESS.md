# PROGRESS — VW MIB2 real firmware (P0369)

This log records early milestones. See the [project README](../README.md) for current HMI status.

Target (firmware-verified): **TechniSat MIB2 Standard ZR — NXP i.MX6 (Cortex-A9, ARMv7-A 32-bit),
QNX Neutrino 6.5.0 SMP.** Firmware: `MST2_EU_VW_ZR_P0369T_3G0919360LN.7z`.

Two complementary tracks (effort ≈ 70% A / 30% B):
- **Track A (primary):** authentic Java HMI execution on Windows — `docs/TRACK_A_HMI_ANALYSIS.md`
- **Track B (long-term):** full QNX boot via QEMU i.MX6 — `docs/P0369_BOOT_REPORT.md`

## Track A — authentic Java HMI on Windows

| Milestone | Status | Evidence |
|---|---|---|
| J9 runtime + HMI components located & extracted | **Done** | cpu_j9 + hmizr tarballs → firmware/analysis/{j9,hmizr} |
| Launch architecture recovered | **Done** | runHMI.sh: de.vw.mib.MIBMain, J9 2.3 foun11, JXE bootclasspath, OSGi bundles |
| Reference architecture studied (public docs only, no file reuse) | **Done** | JDK 21 + ANGLE GL ES 2 + Swing + Java backend; docs/TRACK_A_HMI_ANALYSIS.md §4 |
| A1: authentic GEM bytecode executes on JDK 21 | **Verified 2026-11-13** | docs/evidence/track-a-gem-poc.txt; `npm run track-a:poc` |
| A2: J9 ROM → .class converter (single class) | **Verified 2026-11-13** | authentic java/lang/Void converted + executed on JDK 21 (Void.TYPE==void.class); docs/evidence/track-a-void-poc.txt; `npm run track-a:void`; tests 14/14 |
| A3: full platform JXE batch conversion + JVM verification + behavioral execution | **Verified 2026-11-13** | 1422/1436 converted (99%); 747 classes JVM-verify (99.7% of non-java.*), all 41 de.vw HMI classes verify; authentic IntObjectOptHashMap executes (put/get/remove/clear/growth); platform.jar; tests 17/17 |
| A4: MIBHMI.jxe extracted from HMI IFS + converted | In progress — IFS LZO decompression VERIFIED; MIBHMI.jxe payload transformation pending | Cracked QNX startup LZO compression (flags1=0x8): independent LZO1X (tools/lzo1x.js) + IFS extractor (tools/ifs-extract.js) decompress the 21.4MB IFS to 47.6MB; imagefs dir_offset=0x5c recovered (docs/evidence/track-a-ifs-decompress.txt). MIBMain confirmed NOT in platform JXE — it is in MIBHMI.jxe inside the IFS. IFS file payloads carry a TechniSat per-file transformation (J99J intact, header fields transformed) — reversal is the Track B security-transform overlap. |
| A5: JSR-231 GL adapter renders authentic view | Pending | |
| A6: DSI simulation + authentic main menu + input | Pending | |

## Track B — full QNX hardware emulation

| Milestone | Status | Evidence |
|---|---|---|
| Firmware archive found | **Done** | firmware/MST2_EU_VW_ZR_P0369T_3G0919360LN.7z (3,803,093,204 B) |
| Archive integrity verified | **Done** | SHA-256 36bc458c…; 7z "Everything is Ok" (CRC) |
| Firmware extracted | **Done** | extracted/P0369: 920 files / 1224 folders / 4,025,406,943 B; original preserved |
| Firmware architecture identified | **Done** | ELF EM_ARM 32-bit; i.MX6 (vpu_fw_imx6q.bin, dev-can-mx6x); QNX (ldqnx, dumpifs, IFS) |
| Original executable located | **Done** | cpu/cpuimage_sec_stdNavi/33/default/mibstd2_cpu.boot (QNX IFS, 5,513,828 B) |
| Authentic instructions executed | **Verified (_start)** | load=0x10800002 → authentic `push {r0-r15}; movt; bl` at _start 0x10800108, SVC32 (docs/evidence/p0369-startup-verified.txt) |
| Startup header / entry established reliably | **Yes** | startup_vaddr=0x10805FB4, image_paddr=0x10800008, machine=40(ARM); parser tests 3/3; correct load base 0x10800002 verified by scan |
| Full-image coherent load | Blocked | non-uniform TechniSat section transformation; _start coheres, bl-target/startup_vaddr need per-section byte ordering (tools/align-map.js) |
| Bootloader reached | Pending | |
| Kernel started | Pending | |
| Original UI displayed | Pending | Track A is the short-term path to authentic UI |

## Verified facts
- SWDL update package (category B) containing authentic QNX IFS boot images + BSP userland.
- CPU = ARMv7-A 32-bit Cortex-A9 (i.MX6), OS = QNX Neutrino 6.5.0. Corrects earlier TI Jacinto guess.
- Firmware is signed (module.info RSA Signature); protections documented, not defeated.
- HMI = Java (IBM J9 2.3, CDC Foundation 1.1, JSR-231 OpenGL ES), TechniSat DSI middleware,
  VW ASL OSGi bundles, skin5 resources. GEM.jar + Foundation classes.zip are standard bytecode.

## Blockers / next
- Track A: J9 ROM class format conversion (critical path); HMI IFS imagefs directory decode.
- Track B: per-section TechniSat transformation map; IPL→startup register handoff; MRC2 p1 cp1 semantics.
- QEMU has no VW-MIB i.MX6 board; deeper boot needs UART/GIC/timers/eMMC/IPU modeling.

## Session log
- 2026-11-12: Located/verified/extracted archive; identified i.MX6/Cortex-A9/QNX from firmware;
  wrote P0369_FIRMWARE_ANALYSIS.md; corrected hardware research. Authentic _start execution verified.
- 2026-11-13: Phase 0 recovery (tests 8/8, Node 25 test-runner fix, firmware archives gitignored).
  Track A started: J9+HMI extracted, architecture recovered, reference studied (no reuse),
  **A1 verified — authentic GEM bytecode executes on Windows/JDK 21** (docs/evidence/track-a-gem-poc.txt).
  **A2 verified — independent J9 2.3 ROM→.class converter; authentic firmware java/lang/Void
  executes on JDK 21** (docs/evidence/track-a-void-poc.txt). ROM format fully reverse-engineered
  from bytes (directory self-rel pairs, methodsOffset=raw[8]+32 validated 1415/1415, LE bytecode
  operands, J9 return opcodes 0xAC/AD/AE, CP shapes). Tests 14/14.
  **A3 verified — full platform batch conversion + JVM verification + behavioral execution.**
  1422/1436 classes converted; 747 JVM-verify (all 41 de.vw HMI classes); authentic VW HMI
  IntObjectOptHashMap executes correctly (docs/evidence/track-a-behavior-poc.txt). Solved:
  variable-length field table (raw[10]+40), J9 CP index space excludes field slots, stack-depth
  constant type resolution (int/float/long/double via consumer field/param type), empty String
  constants, J9 quick opcodes 0xD7/0xE7/0xF9, synthetic field flag. Reproducible platform.jar.
  Known limits: 14 classes (6 IBM text utilities w/ J9 quick opcodes 0xD5/0xEE, 8 CP edge cases);
  2 verify failures (swapped field name/desc in constant-heavy DSI/GLInfo classes). Tests 17/17.
  **A4 progress — QNX HMI IFS LZO decompression cracked** (independent LZO1X, verified vs QNX
  dumpifs.c framing): 21.4MB IFS → 47.6MB imagefs, real dir_offset=0x5c recovered. MIBMain is in
  MIBHMI.jxe (IFS); IFS payloads are TechniSat-transformed (next: reverse per-file transform).
