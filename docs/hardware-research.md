# MIB2 Hardware Research — VW Golf MK7 (priority target)

> **FIRMWARE-VERIFIED CORRECTION (P0369, 2026-11-12):** The authentic TechniSat MIB2 Standard
> ZR firmware `MST2_EU_VW_ZR_P0369T` proves the target is **NXP i.MX6 (Cortex-A9, ARMv7-A
> 32-bit) + QNX Neutrino**, NOT TI Jacinto/Cortex-A15. Evidence: ELF `EM_ARM(40)` 32-bit;
> `fw/vpu/vpu_fw_imx6q.bin`, `bin/dev-can-mx6x`; boot-image strings `i.MX6`, `ldqnx.so.2`,
> `valid IFS/`, `QNX6 filesystem`. The TI Jacinto article (SSZTBM2) describes the **Delphi**
> MIB2 Standard variant — a different hardware platform. QEMU reference: `sabrelite` (i.MX6Q,
> Cortex-A9). See docs/P0369_FIRMWARE_ANALYSIS.md. The TI Jacinto rows below are retained for
> the Delphi variant only.

Goal: verified execution of authentic VW infotainment code. MIB2 is prioritized over MIB3
because it is more openly documented and its CPU core is directly supported by QEMU.
MIB3 research/code is kept intact in `../vw-mib3-emulator`.

## Confidence convention
- **verified** — read from a primary source (TI/QEMU/official) or directly from an image.
- **reported** — stated by a reputable community source; not yet re-verified from an image.
- **unknown** — must be read from an authentic image/unit; do not assume.

## 1. Two distinct MIB2 platforms (do not conflate)

| Platform | Golf MK7 trim | Supplier(s) | SoC | CPU / ISA | OS | Emulation practicality |
|---|---|---|---|---|---|---|
| **MIB2 Standard** | Discover Media / Composition | **TechniSat/Preh** (`…T`), **Delphi** (`…D`) — TI silicon | **TI Jacinto 6** (DRA74x/DRA75x) | **Dual ARM Cortex-A15, ARMv7-A, 32-bit** | **QNX Neutrino 6.5** (reported, strong evidence) | **Higher** — public firmware/tooling, TI docs, QEMU Cortex-A15 core |
| **MIB2 High** | Discover Pro | **Harman** (MHI2/MHI2Q, no suffix); AISIN in Asia (identical hw) | Nvidia Tegra (reported) | ARM Cortex-A9 (reported), 32-bit | **QNX** (reported) | Lower — less open tooling |

> **Correction (2026-11-12):** MIB2 Standard runs **QNX**, not Linux. Evidence: the
> TechniSat/Preh toolbox mounts the unit's eMMC inside a **QNX Neutrino 6.5** VM, uses `ksh`,
> `/fs/...` mount paths and `root/root` console login. Both MIB2 variants are QNX (proprietary
> RTOS). The openly-licensed **Linux** reference image is used ONLY to validate the emulated
> Cortex-A15 CPU/devices — it is neither VW firmware nor the VW OS.

### Selected initial target: **MIB2 Standard (TI Jacinto 6, Cortex-A15, QNX)**
Rationale: abundant public firmware/tooling (olli991 toolbox), TI primary documentation, and a
QEMU machine (`vexpress-a15`) modeling the same Cortex-A15 core. Authentic OS is QNX Neutrino
(commercial; no openly-licensed reference image), so authentic boot will need a QNX-capable
environment and a custom Jacinto device model. MIB2 High (Harman) is the second target.

## 2. Verified facts — MIB2 Standard

- **SoC family:** TI **Jacinto 6** (DRA75x/DRA74x). TI: "Dual 1.5 GHz Arm Cortex-A15 SoC
  processor with graphics & DSP for automotive infotainment & cluster". CPU = 2× Cortex-A15;
  4× Cortex-M4 IPU; 3D GPU dual PowerVR SGX544; 2D Vivante GC320; C66x DSP; HDMI + 3× LCD;
  secure boot/secure storage. **verified** (TI DRA746 product page).
- **Direct VW link:** TI article *"Infotainment for the Masses – Volkswagen MIB II Standard
  Powered by TI"* (SSZTBM2, 2016): "TI collaborated with Volkswagen and their suppliers,
  **Delphi and TechniSat**, on the MIB II infotainment platform" using "Jacinto processors,
  power management, and FPD-Link III serializers/deserializers." **verified** (TI primary).
- **ISA / endianness:** ARMv7-A, 32-bit ARM (A32/T32), little-endian. **verified** (Cortex-A15).
- **OS:** QNX Neutrino 6.5. **reported, strong evidence** (toolbox QNX 6.5 VM + ksh + /fs paths).
- **Storage:** most units have a **7.3 GB eMMC**. **reported** (toolbox FAQ).
- **Train naming:** `MST2_<region>_<brand>_<vendor>_<version><suffix>`; suffix **T**=TechniSat/Preh,
  **D**=Delphi, none=Harman. Example `MST2_EU_SK_ZR_P0478T`. **reported** (toolbox FAQ).
- **Update integrity:** `metainfo2.txt` carries a **digital signature** at end of file (cannot
  be edited). SWDL updates contain `cpu`/`cpuplus` module folders (e.g. `esd_sec`). **reported**.
- **QEMU reference board:** `vexpress-a15` (Cortex-A15) with PL011 UART, SP804 timers, PL181 SD,
  PL111 LCD, LAN9118, GIC, virtio-mmio. **verified** (QEMU docs). QEMU has **no Jacinto/DRA7
  machine**; vexpress-a15 is a core-compatible reference, not the authentic board.

## 3. Reported facts — MIB2 High (to re-verify from image)
- Harman **MHI2/MHI2Q**: VW Discover Pro `MHI2_VWG11/13`; Škoda Columbus `MHI2_SKG11/13`;
  SEAT `MHI2_SEG11`; Audi MMI Nav Plus `MHI2_AU`/`MHI2Q_AU`; Porsche PCM4 `MHI2_POG11/24`.
  Asia: AISIN, 1:1 identical hardware. **reported** (M.I.B. wiki).
- OS QNX; exact QNX version and Nvidia Tegra model **unknown** until read from an image.

## 4. Unknowns (read from authentic image; do not invent)
- Exact Jacinto part (DRA74x vs DRA75x vs DRA72x) and board DTB; RAM size; eMMC partition map;
  display resolution; console UART address; QNX boot chain (IFS image) layout.
- MIB2 High: exact Tegra SoC, QNX version, boot chain.

## 5. Sources (real URLs, retrieved 2026-11-12)
- TI DRA746 (Jacinto 6): https://www.ti.com/product/DRA746
- TI article SSZTBM2 "VW MIB II Standard Powered by TI": https://www.ti.com/lit/pdf/SSZTBM2
- TI DRA75x/DRA74x TRM: https://www.ti.com/lit/pdf/SPRUI30
- QEMU Versatile Express: https://www.qemu.org/docs/master/system/arm/vexpress.html
- olli991/mib-std2-pq-zr-toolbox (MIB2 Standard TechniSat/Preh; QNX/eMMC/train facts):
  https://github.com/olli991/mib-std2-pq-zr-toolbox (README, master branch)
- M.I.B. (Harman MIB2) wiki: https://github.com/Mr-MIBonk/M.I.B._More-Incredible-Bash/wiki
- jilleb/mib2-toolbox (MIB2-High): https://github.com/jilleb/mib2-toolbox
- NHTSA "MIB II" SSP (MC-10121137-9999) = SSP 890153: App-Connect feature overview, NOT a
  hardware/CPU doc — not used for CPU facts. https://static.nhtsa.gov/odi/tsbs/2016/MC-10121137-9999.pdf
