// Track A Interactive Runner: Launch authentic Volkswagen MIB2 Standard HMI (Golf 7)
// on Windows using OpenJDK 21 and Google ANGLE Direct3D 11.
// The interactive desktop window (800x480) will open and respond to mouse/touch clicks.
import { spawn, execFileSync } from 'node:child_process';
import { existsSync, mkdirSync, copyFileSync, readdirSync } from 'node:fs';
import { dirname, join, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';
import { buildClasspathJar } from '../jxe/build-hmi-classpath.js';
import { findFirmwareArchive, prepareFirmware } from '../../../tools/prepare-firmware.js';

const root = resolve(dirname(fileURLToPath(import.meta.url)), '../../..');
const exe = process.platform === 'win32' ? '.exe' : '';
const sep = process.platform === 'win32' ? ';' : ':';

function javaHome() {
  if (process.env.JAVA_HOME) return process.env.JAVA_HOME;
  for (const c of ['C:/Program Files/Java/jdk-21.0.12.1', 'C:/Program Files/Eclipse Adoptium/jdk-21']) {
    if (existsSync(join(c, 'bin', 'java' + exe))) return c;
  }
  return null;
}
const jh = javaHome();
const bin = (t) => jh ? join(jh, 'bin', t + exe) : t + exe;

const hmiRom = join(root, 'firmware/analysis/p0369/hmi/rom.classes');
const p0475Rom = join(root, 'firmware/analysis/p0475/hmi/rom.classes');
const platRom = join(root, 'firmware/analysis/rom/rom.classes');
const rtDir = join(root, 'firmware/analysis/p0369/runtime');

try {
  const needsFirmware = !existsSync(hmiRom) || !existsSync(platRom);
  const archive = process.argv[2] || (needsFirmware ? findFirmwareArchive(root) : undefined);
  if (archive) await prepareFirmware(root, archive);
} catch (error) {
  console.error(`[start-hmi] ${error.message}`);
  process.exit(1);
}

if (!existsSync(hmiRom) || !existsSync(platRom)) {
  console.error('[start-hmi] Add the stock P0369 firmware archive beside run-vw-mib2.bat, or drag the archive onto it.');
  console.error('[start-hmi] See docs/SETUP.md for the supported files and Windows dependencies.');
  process.exit(1);
}

if (process.platform === 'win32') {
  const libraries = ['libEGL.dll', 'libGLESv2.dll', 'swap.dll', 'tsd.mibstd2.hmi.dsi.native.dll', 'tsd.opengl.jsr231.native.dll'];
  const missing = libraries.filter(name => !existsSync(join(rtDir, name)));
  if (missing.length) {
    console.error(`[start-hmi] Missing Windows DLLs: ${missing.join(', ')}. See docs/SETUP.md.`);
    process.exit(1);
  }
}

const ltDir = join(root, 'firmware/analysis/p0369/load-test');
const jarPath = join(ltDir, 'hmi-classpath.jar');
mkdirSync(ltDir, { recursive: true });

if (!existsSync(jarPath)) {
  console.log('[start-hmi] Building HMI+platform classpath JAR...');
  const roms = existsSync(p0475Rom) ? [hmiRom, p0475Rom, platRom] : [hmiRom, platRom];
  buildClasspathJar(roms, jarPath);
}

execFileSync(bin('javac'), ['-cp', jarPath + sep + ltDir, '-d', ltDir, join(root, 'src/track-a/poc/TrackAWebServer.java'), join(root, 'src/track-a/poc/TrackAVehicleSim.java'), join(root, 'src/track-a/poc/LoadMIBMain.java')], { stdio: 'inherit' });

for (const f of readdirSync(ltDir)) {
  if (f.endsWith('.class')) {
    copyFileSync(join(ltDir, f), join(rtDir, f));
  }
}

const props = {
  'de.vw.mib.installation.directory': 'HMI/',
  'de.vw.mib.configuration.directory': 'HMI/config/',
  'de.vw.mib.resources.directory': 'Resources/',
  'bundles.properties': 'HMI/config/bundles.properties',
  'threads.properties': 'HMI/config/threads.properties',
  'logging.properties': 'HMI/config/logging.properties',
  'system.properties': 'HMI/config/system.properties',
  'dsiexclude.properties': 'HMI/config/dsiexclude.properties',
  'de.vw.mib.startup.order': 'TARGET',
  'vw.mib.resources': 'HMI/res',
  'java.library.path': '.',
  'tsd.mibstd2.hmi.dsi.communication.DefaultPriority': '8',
  'de.vw.mib.log4mib.mlp.enabled': 'true',
  'de.vw.mib.threads.disableWatchdog': 'true',
  'STARTUP_LUM_READY': 'true',
  'de.vw.mib.runmode.is_normal': 'true',
  'viewhandler.format': 'jar',
  'dsi.adapter.factory.class': 'tsd.mibstd2.hmi.dsi.base.NavigationFactory',
  'GenericEvents.activatePooling': 'true',
  'GenericEventPool.poolSize': '2048',
  'GenericEventPool.poolPreFill': '512',
  'de.vw.mib.graphics.font.nativetextlayout': 'false',
  'de.vw.mib.graphics.image.nocheckerboard': 'true',
  'de.vw.mib.configuration.encodedVehicleType': '0203070000',
};

const args = ['-Xmx512m', '-Xverify:none'];
for (const [k, v] of Object.entries(props)) args.push(`-D${k}=${v}`);
args.push('-cp', jarPath + sep + ltDir, 'LoadMIBMain', 'de.vw.mib.MIBMain', 'run');

console.log('========================================================================');
console.log(' Starting Authentic Volkswagen MIB2 Infotainment (Golf 7)');
console.log(' Resolution: 800x480 | Backend: Google ANGLE / Direct3D 11');
console.log(' Controls:');
console.log('   [C] CAR menu       | [R] RADIO       | [E] MEDIA');
console.log('   [N] NAVIGATION     | [M] MENU        | [S] SETUP');
console.log('   [P] PHONE          | [<- / ->] SEEK  | [^ / v] ROTARY KNOB');
console.log('   [Enter] SELECT     | [Mouse] TOUCHSCREEN CLICK & DRAG');
console.log(' Close window or press Ctrl+C in this terminal to exit.');
console.log('========================================================================');

const child = spawn(bin('java'), args, { cwd: rtDir, stdio: ['pipe', 'inherit', 'inherit'] });

if (process.stdin.isTTY) {
  process.stdin.setRawMode(true);
  process.stdin.resume();
  process.stdin.on('data', (key) => {
    // Ctrl+C (SIGINT)
    if (key.length === 1 && key[0] === 3) {
      child.kill();
      process.exit(0);
    }
    // ANSI arrow keys
    if (key.length >= 3 && key[0] === 27 && key[1] === 91) {
      if (key[2] === 65) child.stdin.write('[');       // Up -> Rotary Left
      else if (key[2] === 66) child.stdin.write(']');  // Down -> Rotary Right
      else if (key[2] === 68) child.stdin.write('<');  // Left -> Seek Prev
      else if (key[2] === 67) child.stdin.write('>');  // Right -> Seek Next
      return;
    }
    child.stdin.write(key);
  });
} else {
  process.stdin.pipe(child.stdin);
}

child.on('exit', (code) => {
  if (process.stdin.isTTY) process.stdin.setRawMode(false);
  console.log(`[start-hmi] Infotainment runtime exited (code: ${code})`);
  process.exit(code || 0);
});
