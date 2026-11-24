# P0369 Firmware Analysis — MST2_EU_VW_ZR_P0369T (authentic, locally available)

## 1. Archive identity & integrity (Phase 1 — verified)
| Property | Value |
|---|---|
| File | `firmware/MST2_EU_VW_ZR_P0369T_3G0919360LN.7z` |
| Format | 7z (magic `37 7A BC AF 27 1C`), LZMA2:26, solid, 1 block |
| Compressed size | 3,803,093,204 bytes |
| SHA-256 | `36bc458cb9ef96d92367a90496493415fba93ea4d7b1453ecaf4dcfe253659dd` |
| Integrity | 7-Zip extraction returned **"Everything is Ok"** (CRC verified) |
| Extracted | `firmware/extracted/P0369/` — 920 files, 1224 folders, 4,025,406,943 bytes |
| Original | preserved unchanged (read-only extraction) |
| Path safety | all entry names relative; no `..` traversal / absolute paths |
| Disk | F: 355 GB free (sufficient) |

Unrelated files in `firmware/` (NOT used): a 0-byte `MST2_EU_VW_PQ_P0475T_…7z` and an
incomplete `…7z.part` (different train PQ/P0475T).

## 2. Manifest (metainfo2.txt, 327,936 bytes)
- `Release = "MST2_EU_VW_ZR_P0369T"`, `MUVersion = "0369"`, `Vendor = "TechniSat"`.
- Variants 17201–17207, 17224; Region Europe / RoW; CPU ID **33**.
- Installer: `/tsd/bin/swdownload/tsd.mibstd2.system.swdownload` (RequiredVersion 3101.236).
- Per-module `FileSize`, `FileName`, `Checksum` (SHA-1) + block checksums; `MetafileChecksum`.
- `module.info` carries an RSA `Signature` (base64) — firmware is signed (do not defeat).

## 3. Classification: **B — SWDL firmware update containing OS + applications + boot images**
Not a raw bootable eMMC image (A). It is the official software-download package the unit's
`swdownload` installer applies. It **does** contain authentic bootable components (QNX IFS
boot images, boot table, BSP filesystem), so authentic execution can be attempted from it.

## 4. Key authentic components (sizes + SHA-256, verified)
| Component | Path (under extracted/P0369) | Size | SHA-256 | Notes |
|---|---|---|---|---|
| CPU boot image (QNX IFS) | `cpu/cpuimage_sec_stdNavi/33/default/mibstd2_cpu.boot` | 5,513,828 | `20e1fd4623268589d155ca7dde3bd1846e7a583758b74f509e4e1f459c597965` | magic `58 21 54 00…`; strings: `ldqnx.so.2`, `i.MX6`, `valid IFS/`, `QNX6 filesystem` |
| Main boot image | `main/mainimage_EU_dab0_hwr0_sec_stdNavi_stdZR_vw/33/default/mibstd2_main.boot` | 7,180,696 | `03bdc129e1cc77ff338fbcaa7b9b98f370db78d2b1994b66208c6a926c70c092` | magic `00 6d 90 68…` |
| CPU init data | `cpu/cpuboot_sec/33/default/init_mibstd2_cpu.dat` | 45,056 | `34fb1c8b0590deb1b93de5b7319b6eea6daafe2c04fb396ccc56cd0303c1769a` | all `0xFF` (erased-flash placeholder) |
| Main init data (sec) | `main/mainboot_sec/33/default/init_mibstd2_main_sec.dat` | 48,084 | `e9f974a0e92f5680ca08175e0a05833c0c579c82e70f5a688f634e663b7e79a0` | header `00 00 26 00…` |
| Boot table | `main/boottable/33/default/boottable.bin` | 28 | `4b203ab74d2d9cc2f7f7aec70a2f94ec8c3dfb2b40a9c4d545f4daf6fe4946f6` | magic `44 58 42 49` = "DXBI" |
| BSP filesystem | `cpu/cpubspfs/33/default/cpu_cpubspfs_33_default.tar.gz` | (tar.gz) | — | QNX userland: bin, sbin, lib, etc, fw, tsd, usr |
| CPU image module info | `cpu/cpuimage_sec_stdNavi/33/default/tsd/etc/swdownload/modules/module.info` | — | — | Version 3369009; RSA Signature |
| cpuplus variant | `cpuplus/cpuimage_sec_stdPlus/33/default/mibstd2_cpu.boot` | — | — | non-Navi (stdPlus) CPU image |

Other top-level trees: `mainstd/`, `mainplus/` (radio/plus variants), `DUV*/DUW*` (display
unit firmware), `VW_MIB2_AMP/` (amplifier `*_Apl.bin`/`*_Pbl.bin`), `bluetooth/` (`.mot` BT
firmware), `inic/`, `powerc/`, `MIBLite2_APN/`. Largest files are resource tarballs
(`hmizr` HMI resources, `nuanceres.*` speech, `stationdb`).

## 5. Target hardware — VERIFIED FROM FIRMWARE (corrects prior research)
- **CPU/ISA:** ARM 32-bit little-endian, `EM_ARM (40)`, ARMv7-A. ELF census: `bin/uname`,
  `bin/pidin` → class 32, machine 40. **Cortex-A9** (i.MX6 family).
- **SoC:** **NXP/Freescale i.MX6** — evidence: `fw/vpu/vpu_fw_imx6q.bin` (i.MX6Q VPU firmware),
  `bin/dev-can-mx6x` (i.MX6 CAN driver), boot-image strings `i.MX6`, `i.MX6x`,
  `Ethernet driver for VW-MIB, imx`.
- **OS:** **QNX Neutrino** — `ldqnx.so.2` (QNX dynamic linker), `dumpifs`, `pidin`,
  `mm-renderer`, `pf.os`, `QNX6 filesystem`, `valid IFS/`. Boot image is a **QNX IFS**.
- **Correction:** earlier research assumed TI Jacinto 6 / Cortex-A15 for MIB2 Standard.
  That applies to the **Delphi** variant (TI article SSZTBM2). This **TechniSat ZR** unit is
  **i.MX6 / Cortex-A9 / QNX**. Emulator architecture updated accordingly.
- **QEMU reference machine:** `sabrelite` (i.MX6 Quad, Cortex-A9) is the closest QEMU board;
  `realview-pbx-a9` (Cortex-A9) is a fallback. QEMU has no VW-MIB board; authentic boot needs
  i.MX6 + custom VW-MIB peripheral modeling.

## 6. Boot chain (evidence-based, partial)
`boottable.bin` ("DXBI") → `init_mibstd2_*` / `mainboot_sec` → `mibstd2_cpu.boot` (QNX IFS:
startup + procnto kernel + boot script) → QNX userland from `cpubspfs` (/tsd, /bin, /lib…).
Exact IFS load address / entry point and the secure-header layout: **to be determined** from
the IFS header (next step). The `init_mibstd2_cpu.dat` being all-0xFF indicates it is a
placeholder filled during flashing, not code.

## 7. Unknowns (do not invent; resolve from image)
- Exact i.MX6 part (Quad/DualLite/Solo), RAM size, memory map.
- QNX IFS header/entry/load address; whether `mibstd2_cpu.boot` has a TechniSat secure wrapper
  before the IFS (magic `58 21 54 00` is not a standard ARM branch).
- UART/console peripheral addresses for VW-MIB i.MX6 board; interrupt controller (GIC/AVIC),
  timers, eMMC/SDHCI, display (IPU), GPU.

## 8. Next engineering step
Parse the QNX IFS header in `mibstd2_cpu.boot` to locate the authentic entry point, then
attempt execution on QEMU i.MX6 (`sabrelite`) / Cortex-A9 with instruction tracing, capturing
registers/trace/console as evidence. Parsing/loading is NOT execution; a generic Linux/QNX
boot is NOT a VW boot. See the [progress log](PROGRESS.md) and [QNX boot report](P0369_BOOT_REPORT.md).

## 9. imagefs contents — authentic OS components confirmed (read-only)
The QNX imagefs (signature `imagefs` @ file 0x12C10; image_size 0x900DF0, dir_offset 0x5C)
contains the authentic QNX userland. Readable directory entries confirm:
- **Kernel:** `proc/boot/procnto-smp-instr` (QNX Neutrino SMP microkernel, instrumented).
- **i.MX6 board drivers:** `devb-sdmmc-mx6q` (SD/MMC), `devc-sermx1` (UART), `sdma-imx6x`,
  `i2c-mx3`, `ecspi`, `spi-master`, `graphics/iMX6X/csc_gamma.conf`, `devu-ehci-mx28`,
  `io-usb`, `io-blk`, `fs-qnx6`, `fs-dos`, `fs-cd`, `fs-nfs3`.
- **OS version:** `6.5.0` (QNX Neutrino 6.5.0).
- **TechniSat HMI:** `tsd/.../tsd.mib...hmi.anim`, `libtsd.videocapture`, `res-technisat`,
  `wfd`/`wfdcfg`/`screen` (Windowing Framework Director), `libxmodes`.
- **Boot script:** references `tsd_imx60`, `J5_CPU_REG_CTRL_SECTAP`, mounts qnx6 root
  `mount -b -t qnx6 ... /dev/hd0t177 /`; shell access gated on a CPU register tap value.
- Standard QNX userland: `ksh`, `ps`, `ls`, `cat`, `mount`, `slogger`, `pipe`, `inetd`,
  `libm.so`, `libstdc++.so.6`, `libz`, `socket.so`, `passwd`/`group`/`shadow`.

This independently confirms the target: **TechniSat MIB2 Standard, NXP i.MX6Q, QNX Neutrino
6.5.0 SMP**, with a TechniSat HMI on the WFD/Screen graphics stack.

**Parsing limitation (honest):** `tools/ifs-dir.js` reads the imagefs header correctly, but
individual `image_dirent` records do not decode cleanly with the stock `sys/image.h` layout
(fields appear packed/encoded, consistent with the TechniSat `…_sec` wrapper). Component
names above are taken from readable path strings, not a fully validated directory walk.
A clean dirent decode (and the exact TechniSat encoding) is still to be resolved.
