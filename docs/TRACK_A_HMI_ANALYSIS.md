# Track A — Authentic Java HMI Execution (P0369)

Goal: run the ORIGINAL VW MIB2 Standard Java HMI on Windows using authentic firmware
bytecode and resources. Not an HTML/React/screenshot recreation. Platform adapters are
independently implemented; HMI code and assets are unmodified firmware artifacts.

## 1. Verified firmware inventory (read-only extraction)

Source tarballs (under `firmware/extracted/P0369/cpu/`):
- `j9/33/default/cpu_j9_33_default.tar.gz` (8.7 MB) → `firmware/analysis/j9/`
- `hmizr_EU_stdNavi_stdZR_vw/33/default/cpu_hmizr_…tar.gz` (195 MB) → `firmware/analysis/hmizr/`

| Component | Path (under analysis/) | Format | Verified facts |
|---|---|---|---|
| J9 VM | `j9/tsd/tmp/j9/bin/j9` + `libj9*23.so` | QNX ARM ELF | IBM J9 2.3, CDC Foundation 1.1 (`-jcl:foun11`) |
| Foundation classlib | `j9/tsd/tmp/j9/lib/jclFoundation11/classes.zip` | standard ZIP of .class | 1043 files, JSR-219 Foundation 1.1, IBM 2.3.1 (2007-03-13) |
| HMI platform JXE | `hmizr/tsd/tmp/hmi/tsd.mibstd2.hmi.v2.jxe` | ZIP containing `rom.classes` | 5,695,799 B; rom.classes 5,589,952 B; J99J magic; jar2jxe 2.3; interpretable=1; containsJCLs=1 |
| HMI IFS | `hmizr/tsd/tmp/hmi/tsd.mibstd2.hmi.ifs` | plain QNX IFS (sig @0, machine=0) | 21,410,108 B; imagefs @0x107, flags=0x4, image_size=0x3148FCC; contains MIBHMI.jxe (string @392); dirent decode pending |
| GEM middleware | `hmizr/tsd/tmp/hmi/HMI/jar/GEM.jar` | standard JAR | 287 entries, 184 .class, `de.audi.gem.*`, Java 1.2 bytecode (v46), built 2015-10-19 |
| ASL debug bundles | `hmizr/tsd/tmp/hmi/HMI/jar/MIBTestDebug-H28.28.208-STD2Nav_EU.jar` | JAR of nested JARs | OSGi-style `ASL-<domain>-{Interface,Implementation}-*.jar` |
| HMI config | `hmizr/tsd/tmp/hmi/HMI/config/` | properties/swap/res | bundles.properties (OSGi startup order), system/threads/logging.properties, coding.swap, adaptation.swap |
| HMI resources | `hmizr/tsd/tmp/hmi/HMI/res/` | .res | statemachines.res, widgetfactory.res, asldatapool.res, modeldatapool.res, persistenceIds.res, popupinfo.res |
| Skins | `hmizr/tsd/tmp/hmi/Resources/skin{1,2,5,6}/` | MCF/res/zip | images.mcf, images.res, viewhandler.zip, i18n/*.res, fonts.conf |
| Native libs | `hmizr/tsd/tmp/hmi/lib*.so` | QNX ARM ELF | libtsd.mibstd2.hmi.dsi.native.so (5.6 MB DSI), libtsd.opengl.jsr231.native.so (615 KB GL), libmcfobjectreader.so, libswap.so, libxip.so |

## 2. Authentic launch architecture (from runHMI.sh, unmodified firmware script)

- Main class: `de.vw.mib.MIBMain`; HMI version H28.28.208, variant STD2Nav_EU.
- VM: `on -p 20 /j9/bin/j9 … -jcl:foun11 -Xbootclasspath:$IFSDIR/tsd.mibstd2.hmi.v2.jxe:$IFSDIR/MIBHMI.jxe:$J9LIB/charconv.zip:$J9LIBF/locale.zip:$MIBJAR/GEM.jar`
- Memory: `-Xmx46m`, micro JIT `-Xmjit:code=4096,singlecache`.
- Bundle layers (bundles.properties): SPI preload (Debug/Timer/Image/Font/File/Display) → Core → ASL (20-29) → GUI Framework (30-39) → HMI (40-59) → Speech (60-69) → Debug/Mockup/Simulation.
- DSI adapter factory: `tsd.mibstd2.hmi.dsi.base.NavigationFactory`; viewhandler format=zip.
- Graphics: JSR-231 (`javax.media.opengl`) implemented by `tsd.opengl.jsr231.*` (inside rom.classes) → native GL ES 2 (Vivante GPU on target).
- Key system properties: `de.vw.mib.installation.directory`, `de.vw.mib.resources.directory`, `de.vw.mib.startup.order=TARGET`, `dsiTimeout=20000`, `domainTimeout=120000`.

## 3. JXE / J9 ROM format (verified header bytes)

`tsd.mibstd2.hmi.v2.jxe` is a ZIP. `rom.classes` begins:
```
4a 39 39 4a  "J99J" magic
06 09 09 06  version
90 4b 55 00  romSize = 0x554B90 (5,589,904; +48 header = file size)
9c 05 00 00  0x59C (1436 — candidate class count)
c0 ff ff ff  …
uuid @0x20 = 2FC6FD67-0152-1000-8001-81522FC6FD67 (matches JXE.MF)
```
JXE.MF: `ImageVersion 2004/12/09.A.9`, `builderName jar2jxe`, `builderVersion 2.3`,
`buildDate 20070313`, `interpretable 1`, `containsJCLs 1`, `bigEndian 0`.
Package directory (from ZIP entries): `de.vw.mib.*` (graphics/activation/debug/extstartup/file/timer),
`tsd.mibstd2.hmi.dsi.*` (~90 domains), `tsd.opengl.jsr231.*`, `javax.media.opengl`,
`com.ibm.oti.*`, `com.microdoc.*` (objectreader, j9.xip), `de.vw.extlibs.collections.*`.

**Conversion required:** J9 ROM classes are not standard .class files. To execute on a
modern JVM we must convert ROM → standard class files (own implementation, see §5).

## 4. Reference architecture (public documentation only — NO file reuse)

`https://github.com/JEtsKi8271/mibstd2-emulator` README (fetched 2026-11-13) documents:
runs real TechniSat MST2 HMI on Windows; JDK 21 exactly (Java 21 FFM API); ANGLE GL ES 2
composited into a Swing window (800×480 @1.5×); backend re-implemented in Java answering
the unmodified HMI; mouse=touch + hard keys. LICENSE: "No Reuse of Project Files" —
source, scripts, firmware, assets, configurations all prohibited from reuse.
**We only studied this public architecture description. All our code is independently
written from our own firmware artifacts.**

## 5. Independent Windows execution strategy

1. **JDK 21.0.12.1** (installed, verified: `C:\Program Files\Java\jdk-21.0.12.1`).
2. **Standard-JAR components run directly**: GEM.jar (PoC 1 ✓), Foundation classes.zip,
   ASL debug bundles. Classpath: authentic jars + Foundation classes.
3. **J9 ROM → .class converter** (`src/track-a/jxe/`, Node.js): parse J9ROMImageHeader,
   class directory, J9ROMClass/Method/Field/ConstantPool; rebuild standard class files;
   verify with javap + execution. Format reference: OpenJ9 j9rom.h (open source) +
   empirical byte validation against our rom.classes.
4. **MIBHMI.jxe extraction**: decode plain QNX imagefs in tsd.mibstd2.hmi.ifs
   (dir_offset field reads 0x2920005C — layout needs correction; flags=0x4 may indicate
   compression). Then convert its ROM classes too.
5. **JSR-231 adapter**: own implementation of `javax.media.opengl` / `tsd.opengl.jsr231`
   native binding over ANGLE or LWJGL GL ES 2 on Windows. The Java-side JSR-231 classes
   are authentic (in rom.classes); only the native GL backend is replaced.
6. **DSI/backend simulation**: own Java services answering the authentic HMI's DSI calls
   (startup, power, persistence, car data, radio, media, phone…). Clearly separated from
   authentic HMI code; never claimed as authentic execution.
7. **Display/input**: Swing window (or GLFW) compositing the GL surface; mouse→touch,
   keyboard→hard keys. Authentic skins/fonts/views rendered by authentic HMI code.

## 6. Milestones (evidence-driven)

| # | Milestone | Status | Evidence |
|---|---|---|---|
| A1 | Authentic GEM bytecode executes on Windows/JDK 21 | **Verified 2026-11-13** | `docs/evidence/track-a-gem-poc.txt`; repro: `npm run track-a:poc` |
| A2 | J9 ROM single-class → .class conversion + execution | Pending | |
| A3 | Full platform JXE converted; de.vw.mib.MIBMain class loads | Pending | |
| A4 | MIBHMI.jxe extracted from IFS + converted | Pending | |
| A5 | JSR-231 GL adapter renders an authentic view | Pending | |
| A6 | DSI simulation + authentic main menu + mouse input | Pending | |

Reproduce A1: extract tarballs (§1), then `node src/track-a/poc/run-poc.js`.
Harness `src/track-a/poc/GemPoc.java` is our test code; the executed class
`de.audi.gem.collections.HashtableOfLong` is unmodified firmware bytecode (code source
printed at runtime proves origin: GEM.jar).

## 7. Honesty rules

- Parsing ≠ execution. A converted class is not "HMI running" until it executes with
  verifiable output. A simulated backend is never counted as authentic HMI.
- No instruction skipping, no fake boot logs, no integrity/secure-boot bypass (Track B).
- Original firmware archives preserved unchanged; all extraction read-only into
  gitignored `firmware/analysis/`.
