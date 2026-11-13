// Track A PoC 1 runner: compile + execute authentic GEM bytecode on the local JDK.
// Reproducible: node src/track-a/poc/run-poc.js  (requires J9/HMI extraction, see below)
import { execFileSync } from 'node:child_process';
import { existsSync, mkdirSync, writeFileSync } from 'node:fs';
import { dirname, join, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';

const root = resolve(dirname(fileURLToPath(import.meta.url)), '../../..');
const gemJar = join(root, 'firmware/analysis/hmizr/tsd/tmp/hmi/HMI/jar/GEM.jar');
const pocDir = join(root, 'src/track-a/poc');
const outDir = join(root, 'firmware/analysis/poc/out');
const evidenceDir = join(root, 'docs/evidence');

if (!existsSync(gemJar)) {
  console.error('GEM.jar not found. Extract the HMI tarball first:');
  console.error('  mkdir -p firmware/analysis/hmizr');
  console.error('  tar -xzf firmware/extracted/P0369/cpu/hmizr_EU_stdNavi_stdZR_vw/33/default/cpu_hmizr_EU_stdNavi_stdZR_vw_33_default.tar.gz -C firmware/analysis/hmizr');
  process.exit(2);
}

mkdirSync(outDir, { recursive: true });
mkdirSync(evidenceDir, { recursive: true });

const sep = process.platform === 'win32' ? ';' : ':';
const javac = process.env.JAVA_HOME ? join(process.env.JAVA_HOME, 'bin', 'javac') : 'javac';
const java = process.env.JAVA_HOME ? join(process.env.JAVA_HOME, 'bin', 'java') : 'java';

console.log('[run-poc] compiling GemPoc.java against authentic GEM.jar ...');
execFileSync(javac, ['-cp', gemJar, '-d', outDir, join(pocDir, 'GemPoc.java')], { stdio: 'inherit' });

console.log('[run-poc] executing authentic de.audi.gem.collections.HashtableOfLong on JDK 21 ...');
const output = execFileSync(java, ['-cp', outDir + sep + gemJar, 'GemPoc'], { encoding: 'utf8' });
process.stdout.write(output);

const log = `# Track A PoC 1 — authentic GEM bytecode execution on Windows/JDK 21
# Date: ${new Date().toISOString()}
# JDK: ${execFileSync(java, ['-version'], { encoding: 'utf8', stdio: ['pipe', 'pipe', 'pipe'] }).trim()}
# Authentic artifact: ${gemJar}
# Class under test: de.audi.gem.collections.HashtableOfLong (unmodified, Java 1.2 bytecode v46)
# Harness: src/track-a/poc/GemPoc.java (our own test code, not firmware)

${output}`;
const evidencePath = join(evidenceDir, 'track-a-gem-poc.txt');
writeFileSync(evidencePath, log);
console.log(`[run-poc] evidence written: ${evidencePath}`);

if (!output.includes('[poc] PASS')) {
  console.error('[run-poc] FAIL');
  process.exit(1);
}
console.log('[run-poc] OK');
