// Build script for Windows JNI adapters:
// 1. tsd.mibstd2.hmi.dsi.native.dll (HMI DSI SPI middleware)
// 2. swap.dll (MicroDoc ObjectSwapper binary resource deserializer)
// 3. tsd.opengl.jsr231.native.dll (TechniSat / VW OpenGL stack via Google ANGLE)
import { execFileSync } from 'node:child_process';
import { existsSync, mkdirSync, copyFileSync } from 'node:fs';
import { join, resolve } from 'node:path';

const root = resolve('F:/vw-mib2-emulator');
const outDir = join(root, 'firmware/analysis/p0369/runtime');

const cl = 'C:/Program Files (x86)/Microsoft Visual Studio/18/BuildTools/VC/Tools/MSVC/14.50.35717/bin/Hostx64/x64/cl.exe';
const jdk = 'C:/Program Files/Java/jdk-21.0.12.1';
const msvcInc = 'C:/Program Files (x86)/Microsoft Visual Studio/18/BuildTools/VC/Tools/MSVC/14.50.35717/include';
const sdkInc = 'C:/Program Files (x86)/Windows Kits/10/Include/10.0.26100.0';

const msvcLib = 'C:/Program Files (x86)/Microsoft Visual Studio/18/BuildTools/VC/Tools/MSVC/14.50.35717/lib/x64';
const sdkLib = 'C:/Program Files (x86)/Windows Kits/10/Lib/10.0.26100.0';

mkdirSync(outDir, { recursive: true });

function compileDll(srcName, dllName) {
  const src = join(root, 'src/track-a/jni', srcName);
  const outDll = join(outDir, dllName);
  const args = [
    '/LD', // Build DLL
    '/O2',
    '/W3',
    `/I${join(jdk, 'include')}`,
    `/I${join(jdk, 'include/win32')}`,
    `/I${msvcInc}`,
    `/I${join(sdkInc, 'ucrt')}`,
    `/I${join(sdkInc, 'shared')}`,
    `/I${join(sdkInc, 'um')}`,
    src,
    `/Fe:${outDll}`,
    '/link',
    `/LIBPATH:${msvcLib}`,
    `/LIBPATH:${join(sdkLib, 'ucrt/x64')}`,
    `/LIBPATH:${join(sdkLib, 'um/x64')}`,
    'kernel32.lib',
    'user32.lib',
  ];
  console.log(`[build-jni] Compiling ${srcName} -> ${dllName}`);
  execFileSync(cl, args, { stdio: 'inherit', cwd: outDir });
  console.log(`[build-jni] Build successful: ${outDll}`);
}

compileDll('tsd_mibstd2_hmi_dsi_native.c', 'tsd.mibstd2.hmi.dsi.native.dll');
compileDll('swap.c', 'swap.dll');
compileDll('tsd_opengl_jsr231_native.c', 'tsd.opengl.jsr231.native.dll');
