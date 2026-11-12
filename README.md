# vw-mib2-emulator

Real firmware emulator for **VW Golf MK7 MIB2** infotainment. Priority target:
**MIB2 Standard (TI Jacinto 6 / Cortex-A15 / Linux)**; second: MIB2 High (Harman MHI2 / QNX).

This is **not** a UI recreation. The goal is verified execution of authentic Volkswagen
firmware. A generic Linux/QNX boot is never counted as a VW boot.

## Status
- Hardware/ISA verified from TI primary sources (see `docs/hardware-research.md`).
- QEMU 11.1.0 obtained (official build, SHA-512 verified) and extracted locally (no admin).
- **Openly-licensed reference test passed**: Debian armhf Linux boots on QEMU `vexpress-a15`
  (Cortex-A15), validating CPU + PL011 UART + timers + GIC. This is a TEST, not a VW boot.
  Evidence: `docs/evidence/mib2-reference-boot.log`. Reproduce: `scripts/run-reference.sh`.
- Authentic VW firmware: **not yet obtained** — see `docs/firmware-analysis.md` for the exact
  missing artifact and how to provide it.

## Layout
- `docs/` — hardware research, firmware analysis, boot progress, evidence.
- `firmware/reference/` — openly-licensed GPL reference image (large; gitignored).
- `firmware/research/` — official TI/NHTSA documents (gitignored).
- `tools/qemu/` — extracted QEMU (large; gitignored).
- `scripts/run-reference.sh` — reproducible reference validation.

## Relationship to MIB3
The MIB3 effort (LG MOI3 / Qualcomm MSM8996 / AGL) is kept intact in `../vw-mib3-emulator`.
Progress reports are separate: `docs/boot-progress.md` here vs `../vw-mib3-emulator/docs/boot-progress.md`.
