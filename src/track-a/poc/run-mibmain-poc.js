// Track A A4 reproducible runner: load + link + initialize the authentic de.vw.mib.MIBMain
// (the HMI entry point) on JDK 21, from freshly converted P0369 HMI + platform ROM classes.
// Reproduce: node src/track-a/poc/run-mibmain-poc.js
// Requires the extracted ROMs (firmware/analysis/p0369/hmi/rom.classes and firmware/analysis/rom/
// rom.classes) and a JDK 21 (JAVA_HOME, or the default install path below).
import { execFileSync } from 'node:child_process';
import { existsSync, mkdirSync, writeFileSync } from 'node:fs';
import { dirname, join, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';
import { buildClasspathJar } from '../jxe/build-hmi-classpath.js';

const root = resolve(dirname(fileURLToPath(import.meta.url)), '../../..');
const exe = process.platform === 'win32' ? '.exe' : '';
const sep = process.platform === 'win32' ? ';' : ':';
function javaHome() {
  if (process.env.JAVA_HOME) return process.env.JAVA_HOME;
  const candidates = ['C:/Program Files/Java/jdk-21.0.12.1', 'C:/Program Files/Eclipse Adoptium/jdk-21'];
  for (const c of candidates) if (existsSync(join(c, 'bin', 'java' + exe))) return c;
  return null;
}
const jh = javaHome();
const bin = (t) => jh ? join(jh, 'bin', t + exe) : t + exe;

const hmiRom = join(root, 'firmware/analysis/p0369/hmi/rom.classes');
const platRom = join(root, 'firmware/analysis/rom/rom.classes');
if (!existsSync(hmiRom) || !existsSync(platRom)) {
  console.error('[run-mibmain] missing extracted ROMs. Extract MIBHMI.jxe rom.classes (see docs/evidence/track-a-ifs-decompress.txt).');
  process.exit(1);
}
const ltDir = join(root, 'firmware/analysis/p0369/load-test');
const jarPath = join(ltDir, 'hmi-classpath.jar');
mkdirSync(ltDir, { recursive: true });

console.log('[run-mibmain] building HMI+platform classpath JAR from authentic ROM classes ...');
const r = buildClasspathJar([hmiRom, platRom], jarPath);
console.log(`[run-mibmain] classpath: ${r.entries} classes (ok=${r.ok} fail=${r.fail})`);
console.log('[run-mibmain] compiling load harness ...');
execFileSync(bin('javac'), ['-d', ltDir, join(root, 'src/track-a/poc/LoadMIBMain.java')], { stdio: 'inherit' });
console.log('[run-mibmain] loading + initializing authentic de.vw.mib.MIBMain on JDK 21 (-noverify) ...');
const output = execFileSync(bin('java'), ['-noverify', '-cp', jarPath + sep + ltDir, 'LoadMIBMain', 'de.vw.mib.MIBMain', 'init'], { encoding: 'utf8' });
process.stdout.write(output);

const evidence = `# Track A A4 — authentic de.vw.mib.MIBMain loads + initializes on JDK 21
# Date: ${new Date().toISOString()}
# Source: de.vw.mib.MIBMain from P0369 MIBHMI.jxe / rom.classes (authentic, unmodified bytecode),
#         converted by the independent ROM->.class converter (src/track-a/jxe/rom-to-class.js).
# Classpath: ${r.entries} converted HMI+platform classes (java.* provided by JDK bootstrap).
# Harness: src/track-a/poc/LoadMIBMain.java (our code; loaded class is authentic VW HMI).
# JVM: JDK 21, -noverify (mirrors firmware runHMI.sh). initialize=true runs the authentic <clinit>.
#
# Execution frontier (next): invoking main() spawns MAIN_THREAD; authentic Log4MIB runs, then
# stops at an interface-methodref tagging bug (Set.iterator() emitted as Methodref, needs
# InterfaceMethodref). See task: converter interface-call CP tagging.

${output}`;
const evidencePath = join(root, 'docs/evidence/track-a-mibmain-load.txt');
writeFileSync(evidencePath, evidence);
console.log('[run-mibmain] evidence:', evidencePath);
if (!output.includes('LOADED: de.vw.mib.MIBMain') || !output.includes('INITIALIZED')) {
  console.error('[run-mibmain] FAIL'); process.exit(1);
}
console.log('[run-mibmain] OK');