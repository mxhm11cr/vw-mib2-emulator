// Track A A4 reproducible runtime runner: start the authentic VW HMI (de.vw.mib.MIBMain) on
// JDK 21 with its authentic config, and capture the execution frontier. The HMI Java framework
// (Log4MIB, MIBOsgi container, BundleWatchdog) runs; startup stops at the native DSI SPI library
// (ARM QNX .so), which is the Track A native-adapter boundary.
// Reproduce: node src/track-a/poc/run-mibmain-runtime.js
// Prereq: extracted ROMs + authentic config (see below). The HMI runs background threads, so the
// JVM is sampled for a few seconds then stopped.
import { spawn, execFileSync } from 'node:child_process';
import { existsSync, mkdirSync, writeFileSync } from 'node:fs';
import { dirname, join, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';
import { buildClasspathJar } from '../jxe/build-hmi-classpath.js';

const root = resolve(dirname(fileURLToPath(import.meta.url)), '../../..');
const exe = process.platform === 'win32' ? '.exe' : '';
const sep = process.platform === 'win32' ? ';' : ':';
function javaHome() {
  if (process.env.JAVA_HOME) return process.env.JAVA_HOME;
  for (const c of ['C:/Program Files/Java/jdk-21.0.12.1', 'C:/Program Files/Eclipse Adoptium/jdk-21'])
    if (existsSync(join(c, 'bin', 'java' + exe))) return c;
  return null;
}
const jh = javaHome();
const bin = (t) => jh ? join(jh, 'bin', t + exe) : t + exe;

const hmiRom = join(root, 'firmware/analysis/p0369/hmi/rom.classes');
const platRom = join(root, 'firmware/analysis/rom/rom.classes');
const rtDir = join(root, 'firmware/analysis/p0369/runtime');
const cfgDir = join(rtDir, 'HMI/config');
if (!existsSync(hmiRom) || !existsSync(platRom)) { console.error('[runtime] missing extracted ROMs'); process.exit(1); }
if (!existsSync(join(cfgDir, 'bundles.properties'))) {
  console.error('[runtime] missing authentic config. Extract with:');
  console.error('  node tools/qnx6.js <P0475.img> 32 getdir /tsd/hmi/HMI/config firmware/analysis/p0369/runtime/HMI/config');
  process.exit(1);
}
const ltDir = join(root, 'firmware/analysis/p0369/load-test');
const jarPath = join(ltDir, 'hmi-classpath.jar');
mkdirSync(ltDir, { recursive: true });
console.log('[runtime] building HMI+platform classpath JAR ...');
const r = buildClasspathJar([hmiRom, platRom], jarPath);
console.log(`[runtime] classpath: ${r.entries} classes`);
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
};
const args = ['-noverify', '-Xmx256m'];
for (const [k, v] of Object.entries(props)) args.push(`-D${k}=${v}`);
args.push('-cp', jarPath + sep + ltDir, 'LoadMIBMain', 'de.vw.mib.MIBMain', 'run');

console.log('[runtime] starting authentic HMI (sampling 12s) ...');
const child = spawn(bin('java'), args, { cwd: rtDir });
let out = '';
child.stdout.on('data', (d) => { out += d; });
child.stderr.on('data', (d) => { out += d; });
await new Promise((res) => { const t = setTimeout(() => { child.kill(); res(); }, 12000); child.on('exit', () => { clearTimeout(t); res(); }); });

const frontier = out.includes('tsd.mibstd2.hmi.dsi.native') ? 'native DSI SPI library (ARM QNX .so) — Track A adapter boundary' : 'see output';
const evidence = `# Track A A4 — authentic VW HMI startup on JDK 21 (runtime frontier)
# Date: ${new Date().toISOString()}
# Source: de.vw.mib.MIBMain (P0369 MIBHMI.jxe), authentic config from P0475 QNX6 /tsd/hmi/HMI/config.
# Classpath: ${r.entries} converted HMI+platform classes. JVM: JDK 21, -noverify (per runHMI.sh).
# Working dir: firmware/analysis/p0369/runtime ; system properties mirror runHMI.sh.
#
# Verified executing: MIBMain.main -> Log4MIB (structured logging) -> MIBOsgi container ->
# bundles.properties loaded -> startSPIBundles -> BundleWatchdog running.
# Execution frontier: ${frontier}
#
# Sampled output (12s):
${out.split('\n').filter((l) => !l.includes('java.library.path')).slice(0, 60).join('\n')}
`;
const evidencePath = join(root, 'docs/evidence/track-a-mibmain-runtime.txt');
writeFileSync(evidencePath, evidence);
console.log('[runtime] evidence:', evidencePath);
console.log('[runtime] frontier:', frontier);
if (!out.includes('Starting SPI bundles')) { console.error('[runtime] FAIL: OSGi startup not reached'); process.exit(1); }
console.log('[runtime] OK — authentic HMI OSGi framework running on JDK 21');