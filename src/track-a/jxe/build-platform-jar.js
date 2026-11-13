// Track A A3: build a reproducible platform JAR from all converted ROM classes.
// Usage: node src/track-a/jxe/build-platform-jar.js [outDir] [jarPath]
import { execFileSync } from 'node:child_process';
import { existsSync } from 'node:fs';
import { join } from 'node:path';

const outDir = process.argv[2] || 'firmware/analysis/converted-all';
const jarPath = process.argv[3] || 'firmware/analysis/platform.jar';
if (!existsSync(outDir)) { console.error('converted dir missing; run batch-convert.js first'); process.exit(2); }
const exe = process.platform === 'win32' ? '.exe' : '';
const jar = process.env.JAVA_HOME ? join(process.env.JAVA_HOME, 'bin', 'jar' + exe) : 'jar' + exe;
execFileSync(jar, ['cf', jarPath, '-C', outDir, '.'], { stdio: 'inherit' });
console.log('platform JAR written:', jarPath);
