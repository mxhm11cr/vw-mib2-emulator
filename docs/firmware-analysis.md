# MIB2 Firmware Analysis & Acquisition

## 0. Status (honest)
- **No authentic VW firmware image is present.** Verified by drive scan.
- A **real, openly-licensed (GPL) reference image** (Debian armhf Linux) has been obtained to
  validate QEMU CPU/device emulation. This is a **test, NOT a Volkswagen boot** (see boot-progress.md).
- Authentic VW execution is blocked on obtaining a legitimate MIB2 image (request below).

## 1. Target packages
- **MIB2 Standard** (Delphi/TechniSat/Preh, "MIB STD2 PQ/ZR"): Linux on TI Jacinto 6.
  Update packages distributed via SD/USB; tooling: olli991/mib-std2-pq-zr-toolbox.
  Exact container layout to be verified from a real package.
- **MIB2 High** (Harman MHI2/MHI2Q): QNX. SWDL update via SD card; `metainfo2.txt` manifest;
  tooling: jilleb/mib2-toolbox, Mr-MIBonk/M.I.B. Exact layout to verify from a real package.

## 2. Legitimate sources (priority order)
1. Official VW service/dealer update package for the exact unit (cleanest provenance).
2. A legally obtained dump/package from the user's own unit.
3. Community mirror of an unmodified official package — only with recorded provenance + hash.
   (The MIB-Solution site referenced in M.I.B. issues is MIB2-patch oriented and reported down.)
4. Official VW **map** updates are public but are map data, not bootable firmware.

We do not use fabricated/modified binaries as proof of authentic execution, and we do not
defeat secure boot, component protection, or firmware signing.

## 3. Reference image obtained (openly licensed, for emulation validation ONLY)
| File | Source | SHA-256 | License |
|---|---|---|---|
| vmlinuz (6,046,208 B) | Debian armhf netboot | b508abca3127cb0d9e1b3aba37f400cbd7dac170040641756807e0e496039de8 | GPL (Linux) |
| initrd.gz (25,985,449 B) | Debian armhf netboot | e7ca77e9dfd998ef8a6f5a39ae3bf7d8c6cfcbdc3c4f59b84077ace585928fbb | GPL (debian-installer) |

URLs: https://deb.debian.org/debian/dists/stable/main/installer-armhf/current/images/netboot/
These boot on QEMU `vexpress-a15` (Cortex-A15) to validate CPU + PL011 UART + timers + GIC.
They are **not** VW firmware and produce **no** VW interface.

## 4. Inspection tooling (planned, reused from MIB3 core)
For any authentic image: SHA-256, container/format detection, partition/filesystem listing,
ELF census + ISA (expect ARMv7-A 32-bit for MIB2), boot-critical components (bootloader/kernel
vs update-only), and provenance. Read-only; original preserved; COW overlays for experiments.

## 5. Precise dependency / request to the user
To attempt authentic MIB2 execution we need ONE of:
- an original **MIB2 Standard** (TechniSat/Preh/Delphi) update package or eMMC/NAND dump, **or**
- an original **MIB2 High** (Harman MHI2/MHI2Q) update package or dump,
with its train/version (e.g. `MHI2_VWG13_…` or `MST2_…`) and how it was obtained.
Place under `firmware/` (or give a path). Until then, only the openly-licensed reference test
runs; no authentic VW boot is claimed.
