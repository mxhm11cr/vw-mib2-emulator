# Windows setup

Track A has been exercised on Windows x64 with JDK 21 and Google ANGLE. The emulator runs firmware components in a desktop process with host-side adapters.

## 1. Prepare the local firmware

Use your own **TechniSat MIB2 Standard ZR P0369** archive, identified in the project as `MST2_EU_VW_ZR_P0369T_3G0919360LN.7z`. The [firmware inventory](P0369_FIRMWARE_ANALYSIS.md) lists its checksum and files. The [HMI analysis](TRACK_A_HMI_ANALYSIS.md#1-verified-firmware-inventory-read-only-extraction) identifies the HMI tarball and its contents.

The launcher expects this layout relative to the clone. Preparation is manual and starts with the files from your own firmware.

| Path | Source |
| --- | --- |
| `firmware/analysis/rom/rom.classes` | `rom.classes` from `tsd.mibstd2.hmi.v2.jxe` |
| `firmware/analysis/p0369/hmi/rom.classes` | P0369 HMI ROM extracted from `MIBHMI.jxe` in the HMI IFS |
| `firmware/analysis/p0369/runtime/HMI/` | Firmware HMI configuration and resources |
| `firmware/analysis/p0369/runtime/Resources/` | Firmware skin resources, including the converted skin 1 view handler |
| `firmware/analysis/p0369/runtime/libEGL.dll` and `libGLESv2.dll` | Matching x64 ANGLE runtime binaries |

The repository contains an [IFS extraction tool](../tools/ifs-extract.js) and a [view handler converter](../src/track-a/jxe/build-viewhandler-jar.js). The launcher builds its classpath JAR if it is missing. A prepared P0475 HMI ROM at `firmware/analysis/p0475/hmi/rom.classes` is used as an optional supplemental input.

## 2. Build the adapters

Install the Visual Studio C++ Build Tools and Windows SDK. Use an **x64 Native Tools Command Prompt for Visual Studio** in the project root.

```bat
set "JAVA_HOME=C:\Path\To\jdk-21"
npm run track-a:jni
```

The build writes the DSI, resource and OpenGL adapter DLLs into the runtime folder. Put the ANGLE DLLs beside them.

## 3. Start the HMI

Run the launcher from the project root with JDK 21 available through `JAVA_HOME` or `PATH`.

```powershell
npm start
```

The desktop window accepts mouse clicks and drags. Use **C** for CAR, **R** for Radio, **E** for Media, **N** for Navigation, **M** for Menu, **P** for Phone and **S** for Setup. The arrow keys and Enter provide rotary and seek actions when the launcher has terminal focus.

Open `http://localhost:8080/` for the dashboard. Keep the dashboard on a trusted development network. It exposes simulator controls over HTTP.

```powershell
npm test
```

The test suite runs without Node dependencies. Firmware-specific checks are enabled by the corresponding local fixtures.
