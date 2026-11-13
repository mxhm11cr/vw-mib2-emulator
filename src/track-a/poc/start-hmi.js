// Track A Interactive Runner: Launch authentic Volkswagen MIB2 Standard HMI (Golf 7)
// on Windows using OpenJDK 21 and Google ANGLE Direct3D 11.
// The interactive desktop window (800x480) will open and respond to mouse/touch clicks.
import { spawn, execFileSync } from 'node:child_process';
import { existsSync, mkdirSync } from 'node:fs';
import { dirname, join, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';
import { buildClasspathJar } from '../jxe/build-hmi-classpath.js';

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
const cfgDir = join(rtDir, 'HMI/config');

if (!existsSync(hmiRom) || !existsSync(platRom)) {
  console.error('[start-hmi] Error: missing extracted firmware ROMs');
  process.exit(1);
}

const ltDir = join(root, 'firmware/analysis/p0369/load-test');
const jarPath = join(ltDir, 'hmi-classpath.jar');
mkdirSync(ltDir, { recursive: true });

if (!existsSync(jarPath)) {
  console.log('[start-hmi] Building HMI+platform classpath JAR...');
  const roms = existsSync(p0475Rom) ? [hmiRom, p0475Rom, platRom] : [hmiRom, platRom];
  buildClasspathJar(roms, jarPath);
}

execFileSync(bin('javac'), ['-d', ltDir, join(root, 'src/track-a/poc/LoadMIBMain.java')], { stdio: 'inherit' });

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
  'de.vw.mib.graphics.font.mockspi': 'true',
  'de.vw.mib.graphics.image.nocheckerboard': 'true',
};

const args = ['-Xmx512m'];
for (const [k, v] of Object.entries(props)) args.push(`-D${k}=${v}`);
args.push('-cp', jarPath + sep + ltDir, 'LoadMIBMain', 'de.vw.mib.MIBMain', 'run');

console.log('========================================================================');
console.log(' Starting Authentic Volkswagen MIB2 Infotainment (Golf 7)');
console.log(' Resolution: 800x480 | Backend: Google ANGLE / Direct3D 11');
console.log(' Close window or press Ctrl+C in this terminal to exit.');
console.log('========================================================================');

const child = spawn(bin('java'), args, { cwd: rtDir, stdio: 'inherit' });
child.on('exit', (code) => {
  console.log(`[start-hmi] Infotainment runtime exited (code: ${code})`);
});
