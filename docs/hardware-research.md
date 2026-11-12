# MIB2 Hardware Research — VW Golf MK7 (priority target)

Goal: verified execution of authentic VW infotainment code. MIB2 is prioritized over MIB3
because it is more openly documented, runs Linux (Standard), and its CPU core is directly
supported by QEMU. MIB3 research/code is kept intact in `../vw-mib3-emulator`.

## Confidence convention
- **verified** — read from a primary source (TI/QEMU/official) or directly from an image.
- **reported** — stated by a reputable community source; not yet re-verified from an image.
- **unknown** — must be read from an authentic image/unit; do not assume.

## 1. Two distinct MIB2 platforms (do not conflate)

| Platform | Golf MK7 trim | Supplier(s) | SoC | CPU / ISA | OS | Emulation practicality |
|---|---|---|---|---|---|---|
| **MIB2 Standard** | Discover Media / Composition | **Delphi, TechniSat** (TI silicon) | **TI Jacinto 6** (DRA74x/DRA75x) | **Dual ARM Cortex-A15, ARMv7-A, 32-bit** | **Linux** | **High** — Linux, public firmware, QEMU Cortex-A15 core |
| **MIB2 High** | Discover Pro | **Harman** (MHI2/MHI2Q); AISIN in Asia (identical hw) | Nvidia Tegra (reported) | ARM Cortex-A9 (reported), 32-bit | **QNX** | Lower — proprietary RTOS, harder reference |

### Selected initial target: **MIB2 Standard (TI Jacinto 6, Cortex-A15, Linux)**
Rationale: openly licensed Linux userland, abundant public firmware/tooling, TI primary
documentation, and a QEMU machine (`vexpress-a15`) that models the same Cortex-A15 core for
reference validation. MIB2 High (Harman/QNX) is the second target.

## 2. Verified facts — MIB2 Standard

- **SoC family:** TI **Jacinto 6** (DRA75x/DRA74x). TI product page: "Dual 1.5 GHz Arm
  Cortex-A15 SoC processor with graphics & DSP for automotive infotainment & cluster".
  CPU = 2× Arm Cortex-A15; coprocessors = 4× Cortex-M4 (IPU); 3D GPU = dual PowerVR SGX544;
  2D = Vivante GC320; DSP = C66x; display = HDMI + 3× LCD; OS support Android/Linux/RTOS;
  secure boot/secure storage present. **verified** (TI DRA746 product page).
- **Direct VW link:** TI technical article *"Infotainment for the Masses – Volkswagen MIB II
  Standard Powered by TI"* (SSZTBM2, 2016-02-17): "TI collaborated with Volkswagen and their
  suppliers, **Delphi and TechniSat**, on the MIB II infotainment platform" using "Jacinto
  processors, power management, and FPD-Link III serializers and deserializers."
  **verified** (TI primary article, downloaded to firmware/research/).
- **ISA / endianness:** ARMv7-A, 32-bit ARM (A32/T32), little-endian. **verified** (Cortex-A15).
- **QEMU reference board:** `vexpress-a15` models Cortex-A15 with PL011 UART, SP804 timers,
  PL181 SD, PL050 KMI, PL111 LCD, PL031 RTC, LAN9118 Ethernet, flash, GIC, virtio-mmio.
  **verified** (QEMU docs). NOTE: QEMU has **no Jacinto/DRA7 machine**; vexpress-a15 is a
  *core-compatible reference*, not the authentic board. A custom DRA7 machine model is needed
  for authentic firmware boot.

## 3. Reported facts — MIB2 High (to re-verify from image)

- Units: Harman **MHI2 / MHI2Q**. VW Discover Pro = `MHI2_VWG11`/`MHI2_VWG13`; Škoda Columbus
  = `MHI2_SKG11/13`; SEAT = `MHI2_SEG11`; Audi MMI Navigation Plus = `MHI2_AU`/`MHI2Q_AU`;
  Porsche PCM4 = `MHI2_POG11`/`POG24`. Asia units built by AISIN, 1:1 identical hardware.
  **reported** (M.I.B. wiki "Which MIB2 units are supported?").
- OS: **QNX** (community consensus; MHI2Q CarPlay repos tag `qnx`). Exact QNX version and the
  Nvidia Tegra model are **unknown** until read from an authentic image/teardown.
- Firmware update mechanism: Harman SWDL via SD card, `metainfo2.txt` manifests (to verify).

## 4. Unknowns (read from authentic image; do not invent)
- Exact Jacinto part in MIB2 Standard (DRA74x vs DRA75x vs DRA72x) and board DTB.
- RAM size, eMMC/NAND layout, display resolution, UART/console address.
- MIB2 High: exact Tegra SoC, QNX version, boot chain, IFS image layout.
- Whether a given unit is Delphi vs TechniSat vs Preh (read train/variant from firmware).

## 5. Sources (real URLs, retrieved 2026-11-12)
- TI DRA746 (Jacinto 6) product page: https://www.ti.com/product/DRA746
- TI article SSZTBM2 "VW MIB II Standard Powered by TI": https://www.ti.com/lit/pdf/SSZTBM2
  (downloaded: firmware/research/ti-mib2-standard-SSZTBM2.pdf)
- TI DRA75x/DRA74x TRM: https://www.ti.com/lit/pdf/SPRUI30
- QEMU Versatile Express docs: https://www.qemu.org/docs/master/system/arm/vexpress.html
- QEMU ARM targets: https://www.qemu.org/docs/master/system/target-arm.html
- M.I.B. (Harman MIB2) wiki: https://github.com/Mr-MIBonk/M.I.B._More-Incredible-Bash/wiki
- jilleb/mib2-toolbox (MIB2-High): https://github.com/jilleb/mib2-toolbox
- olli991/mib-std2-pq-zr-toolbox (MIB2 Standard TechniSat/Preh): https://github.com/olli991/mib-std2-pq-zr-toolbox
- Official NHTSA "MIB II" SSP (MC-10121137-9999) = SSP 890153: an App-Connect feature overview,
  NOT a hardware/CPU document — not used for CPU facts. https://static.nhtsa.gov/odi/tsbs/2016/MC-10121137-9999.pdf
