// Track A A2 reproducible runner: convert authentic firmware java.lang.Void from
// J9 ROM to a standard .class (our independent converter), then execute it on JDK 21
// via --patch-module java.base (replacing the JDK's own Void with the firmware one).
// Reproduce: node src/track-a/poc/run-void-poc.js
import { execFileSync } from 'node:child_process';
import { existsSync, mkdirSync, writeFileSync, readFileSync } from 'node:fs';
import { dirname, join, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';
import { convertClass } from '../jxe/rom-to-class.js';

const root = resolve(dirname(fileURLToPath(import.meta.url)), '../../..');
const romPath = join(root, 'firmware/analysis/rom/rom.classes');
if (!existsSync(romPath)) {
  console.error('rom.classes not found. Extract the platform JXE first:');
  console.error('  unzip -o -j firmware/analysis/hmizr/tsd/tmp/hmi/tsd.mibstd2.hmi.v2.jxe rom.classes -d firmware/analysis/rom');
  process.exit(2);
}
const javac = process.env.JAVA_HOME ? join(process.env.JAVA_HOME, 'bin', 'javac') : 'javac';
const java = process.env.JAVA_HOME ? join(process.env.JAVA_HOME, 'bin', 'java') : 'java';

const { bytes, lines } = convertClass(readFileSync(romPath), 'java/lang/Void');
const classDir = join(root, 'firmware/analysis/converted/java/lang');
mkdirSync(classDir, { recursive: true });
writeFileSync(join(classDir, 'Void.class'), bytes);
console.log('[run-void] converted authentic java/lang/Void:', bytes.length, 'bytes');
for (const l of lines) console.log('  ' + l);

const pocOut = join(root, 'firmware/analysis/poc/out');
mkdirSync(pocOut, { recursive: true });
execFileSync(javac, ['-d', pocOut, join(root, 'src/track-a/poc/TestConvertedVoid.java')], { stdio: 'inherit' });

const convertedRoot = join(root, 'firmware/analysis/converted');
console.log('[run-void] executing firmware Void on JDK 21 (--patch-module java.base) ...');
const output = execFileSync(java, ['--patch-module', 'java.base=' + convertedRoot, '-cp', pocOut, 'TestConvertedVoid'], { encoding: 'utf8' });
process.stdout.write(output);

const evidence = `# Track A A2 — authentic firmware java.lang.Void executes on JDK 21
# Date: ${new Date().toISOString()}
# Source: J9 2.3 ROM class (tsd.mibstd2.hmi.v2.jxe / rom.classes), unmodified firmware bytecode
# Converter: src/track-a/jxe/rom-to-class.js (independent implementation, empirically derived format)
# Execution: JDK 21 --patch-module java.base replaces the JDK Void with the converted firmware Void
# Test harness: src/track-a/poc/TestConvertedVoid.java (our code; executed class is authentic)

${lines.join('\n')}

${output}`;
const evidencePath = join(root, 'docs/evidence/track-a-void-poc.txt');
writeFileSync(evidencePath, evidence);
console.log('[run-void] evidence:', evidencePath);
if (!output.includes('[a2] PASS')) { console.error('[run-void] FAIL'); process.exit(1); }
console.log('[run-void] OK');
