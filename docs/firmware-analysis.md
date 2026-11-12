# MIB2 Firmware Analysis & Acquisition

## 0. Status (honest)
- **No authentic VW firmware image is present.** Verified by drive scan.
- A real, openly-licensed (GPL) **Linux** reference image validates QEMU CPU/device emulation
  only. It is a **TEST, NOT a Volkswagen boot**, and not even the VW OS (MIB2 runs QNX).
- Authentic VW execution is blocked on a legitimate MIB2 image (precise request below).

## 1. MIB2 Standard package format (reported by the TechniSat/Preh toolbox)
- **Train:** `MST2_<region>_<brand>_<vendor>_<version><suffix>`; suffix T=TechniSat/Preh,
  D=Delphi, none=Harman (e.g. `MST2_EU_SK_ZR_P0478T`).
- **Update container:** SWDL software-download package; root contains `metainfo2.txt`
  (**digitally signed** at end of file — do not edit) and `cpu`/`cpuplus` module folders
  (modules include `esd_sec`, etc.). Delivered as a zip/SD-card image.
- **Storage:** ~**7.3 GB eMMC**; a raw eMMC dump is a complete bootable system image
  (QNX Neutrino 6.5 + apps). The toolbox can dump it ("eMMC content" menu).
- **OS:** QNX Neutrino 6.5 (ksh, /fs paths, root/root). Boot chain / IFS layout to verify.
- Tooling: olli991/mib-std2-pq-zr-toolbox (TechniSat/Preh ONLY; not Delphi/Harman).

## 2. MIB2 High package format (Harman, to verify from image)
- Harman SWDL via SD card; `metainfo2.txt` manifest; QNX. Tooling: jilleb/mib2-toolbox, M.I.B.

## 3. Legitimate sources (priority order)
1. Official VW service/dealer update package for the exact unit (cleanest provenance).
2. A legally obtained raw eMMC dump / update package from the user's own unit.
3. Community mirror of an unmodified official package — only with recorded provenance + hash.
We do not use fabricated/modified binaries as proof of authentic execution, and we do not
defeat secure boot, component protection, or the metainfo2 digital signature.

## 4. Reference image obtained (openly licensed, CPU/device validation ONLY)
| File | Source | SHA-256 | License |
|---|---|---|---|
| vmlinuz (6,046,208 B) | Debian armhf netboot | b508abca3127cb0d9e1b3aba37f400cbd7dac170040641756807e0e496039de8 | GPL (Linux) |
| initrd.gz (25,985,449 B) | Debian armhf netboot | e7ca77e9dfd998ef8a6f5a39ae3bf7d8c6cfcbdc3c4f59b84077ace585928fbb | GPL |
| vexpress-v2p-ca15-tc1.dtb (13,104 B) | Debian device-tree | 08ae26a8a5240cfad659a6ff93600873e7fcc91d7efe3b004eba25204ea8db1b | GPL |

Boots on QEMU `vexpress-a15` (Cortex-A15). **Not** VW firmware; produces **no** VW interface;
the authentic VW OS is QNX, not Linux.

## 5. Inspection tooling (implemented + tested)
`tools/inspect-firmware.js` (read-only): SHA-256, container magic detection (7z/zip/gzip/
Android boot/sparse/ELF/ISO9660/ext4/metainfo), ELF ISA census (expects ARM 0x28 for MIB2),
zip central-directory listing, ext4 superblock. Tests: `tests/inspect.test.js` (synthetic
fixtures, clearly not authentic firmware). Run: `npm test`, `npm run inspect -- <path>`.

## 6. Precise dependency / request to the user (MIB2 Standard chosen)
Provide ONE of, legally obtained and unmodified, under `firmware/` (or give a path):
- a **MIB2 Standard SWDL update package** (zip/SD image with `metainfo2.txt` + `cpu`/`cpuplus`), **or**
- a **raw eMMC dump** (~7.3 GB) of a TechniSat/Preh/Delphi MIB2 Standard unit.
Include: train/version (e.g. `MST2_EU_VW_ZR_P0478T`), supplier suffix (T/D), and how obtained.
Until then only the openly-licensed reference test runs; no authentic VW boot is claimed.
