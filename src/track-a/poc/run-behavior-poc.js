// Track A A3 reproducible behavioral runner: build the platform JAR from authentic ROM
// classes, then execute an authentic VW HMI collection class (IntObjectOptHashMap) on JDK 21.
// Reproduce: node src/track-a/poc/run-behavior-poc.js
import { execFileSync } from 'node:child_process';
import { existsSync, mkdirSync, writeFileSync } from 'node:fs';
import { dirname, join, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';

const root = resolve(dirname(fileURLToPath(import.meta.url)), '../../..');
const exe = process.platform === 'win32' ? '.exe' : '';
const bin = (t) => process.env.JAVA_HOME ? join(process.env.JAVA_HOME, 'bin', t + exe) : t + exe;
const sep = process.platform === 'win32' ? ';' : ':';
const jarPath = join(root, 'firmware/analysis/platform.jar');
const outDir = join(root, 'firmware/analysis/poc/out');
mkdirSync(outDir, { recursive: true });

console.log('[run-behavior] building platform JAR from freshly converted classes ...');
execFileSync(process.execPath, [join(root, 'src/track-a/jxe/build-platform-jar.js')], { stdio: 'inherit' });
console.log('[run-behavior] compiling behavioral test against authentic platform JAR ...');
execFileSync(bin('javac'), ['-cp', jarPath, '-d', outDir, join(root, 'src/track-a/poc/BehaviorTest.java')], { stdio: 'inherit' });
console.log('[run-behavior] executing authentic de.vw.extlibs IntObjectOptHashMap on JDK 21 ...');
const output = execFileSync(bin('java'), ['-cp', outDir + sep + jarPath, 'BehaviorTest'], { encoding: 'utf8' });
process.stdout.write(output);

const evidence = `# Track A A3 — authentic VW HMI code behavioral execution on JDK 21
# Date: ${new Date().toISOString()}
# Source: de.vw.extlibs.collections.ints.IntObjectOptHashMap from P0369 platform JXE
#         (tsd.mibstd2.hmi.v2.jxe / rom.classes), converted by independent ROM->.class converter
# Platform JAR: ${jarPath}
# Harness: src/track-a/poc/BehaviorTest.java (our test code; executed class is authentic VW HMI)

${output}`;
const evidencePath = join(root, 'docs/evidence/track-a-behavior-poc.txt');
writeFileSync(evidencePath, evidence);
console.log('[run-behavior] evidence:', evidencePath);
if (!output.includes('[behavior] PASS')) { console.error('[run-behavior] FAIL'); process.exit(1); }
console.log('[run-behavior] OK');
