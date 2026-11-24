# Windows setup

Track A has been exercised on Windows x64 with JDK 21 and Google ANGLE. The emulator runs firmware components in a desktop process with host-side adapters.

## 1. Prepare the local firmware

Install **Node.js 22.2+**, **JDK 21**, and **7-Zip**. The importer accepts either of these stock TechniSat P0369 files from a copy you have access to.

| Input | Size |
| --- | --- |
| `cpu_hmizr_EU_stdNavi_stdZR_vw_33_default.tar.gz` | 196 MB HMI module |
| `MST2_EU_VW_ZR_P0369T_3G0919360LN.7z` | 3.8 GB update archive |

Drag either file onto `run-vw-mib2.bat`. You can also import it without launching the emulator.

```powershell
npm run setup:firmware -- "C:\Path\To\cpu_hmizr_EU_stdNavi_stdZR_vw_33_default.tar.gz"
```

The importer verifies the input's SHA-256 and stages the two ROMs, HMI resources, and graphics caches into `firmware/analysis/`. If you already have the full archive, place it beside the batch file and launch to import it automatically. The first launch then builds the Java classpath. The [firmware inventory](P0369_FIRMWARE_ANALYSIS.md) records the archive identity.

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
