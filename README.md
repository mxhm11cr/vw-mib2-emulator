# vw-mib2-emulator

Real firmware emulator for **VW Golf MK7 MIB2 Standard** infotainment.
Target (firmware-verified): **TechniSat MIB2 Standard ZR — NXP i.MX6Q / Cortex-A9 / QNX Neutrino 6.5.0 SMP**.
Firmware: `MST2_EU_VW_ZR_P0369T_3G0919360LN.7z` (authentic, SHA-256 verified, preserved unchanged).

This is **not** a UI recreation. The goal is verified execution of authentic Volkswagen
firmware. A generic Linux/QNX boot is never counted as a VW boot; a simulated interface is
never counted as authentic HMI execution.

## Two tracks (≈70% A / 30% B)

- **Track A (primary, short-term):** run the ORIGINAL Java HMI on Windows — authentic
  firmware bytecode (IBM J9 2.3 / CDC Foundation 1.1 / JSR-231 OpenGL ES) with independently
  implemented platform adapters. See `docs/TRACK_A_HMI_ANALYSIS.md`.
  - **A1 verified:** authentic `de.audi.gem.collections.HashtableOfLong` (GEM.jar) executes
    on JDK 21. Reproduce: `npm run track-a:poc`. Evidence: `docs/evidence/track-a-gem-poc.txt`.
- **Track B (long-term):** full authentic QNX boot via QEMU i.MX6 (`sabrelite`, Cortex-A9).
  Authentic `_start` executes at validated entry; blocked on TechniSat section transformation
  + IPL/cp1 handoff. See `docs/P0369_BOOT_REPORT.md`, `PROGRESS.md`.

## Status
- Firmware acquired, integrity-verified, extracted (920 files); architecture identified from
  authentic ELF/IFS evidence (corrects earlier TI Jacinto guess — that applies to Delphi variants).
- QEMU 11.1.0 local (official build, SHA-512 verified). Openly-licensed reference test passed:
  Debian armhf boots on `vexpress-a15` (CPU/UART/timers/GIC validation only — NOT a VW boot).
  Evidence: `docs/evidence/mib2-reference-boot.log`. Reproduce: `scripts/run-reference.sh`.
- JDK 21.0.12.1 installed; authentic GEM bytecode execution verified (Track A PoC 1).
- Tests: `npm test` (8/8, Node ≥20; runner uses explicit glob for Node 25 compatibility).

## Layout
- `src/track-a/` — Track A: authentic HMI execution (PoC harness, JXE/ROM converter, adapters).
- `tools/`, `scripts/` — Track B: IFS parsers, QEMU run scripts, alignment analysis.
- `docs/` — research, analysis, boot report, evidence (`docs/evidence/`).
- `firmware/` — original archives (gitignored), extracted P0369 + analysis outputs (gitignored).
- `tools/qemu/` — extracted QEMU (gitignored).
- `tests/` — reproducible parser/inspector tests (`node --test`).

## Relationship to MIB3
The MIB3 effort (LG MOI3 / Qualcomm MSM8996 / AGL) is kept intact in `../vw-mib3-emulator`.
Progress reports are separate: `docs/boot-progress.md` here vs `../vw-mib3-emulator/docs/boot-progress.md`.
