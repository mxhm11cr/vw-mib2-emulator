// Track A A3: build a reproducible platform JAR from all converted ROM classes.
// Usage: node src/track-a/jxe/build-platform-jar.js [outDir] [jarPath]
import { execFileSync } from 'node:child_process';
import { existsSync } from 'node:fs';
import { join } from 'node:path';

const outDir = process.argv[2] || 'firmware/analysis/converted-all';
const jarPath = process.argv[3] || 'firmware/analysis/platform.jar';
if (!existsSync(outDir)) { console.error('converted dir missing; run batch-convert.js first'); process.exit(2); }
const exe = process.platform === 'win32' ? '.exe' : '';
function javaHome() {
  if (process.env.JAVA_HOME) return process.env.JAVA_HOME;
  for (const c of ['C:/Program Files/Java/jdk-21.0.12.1', 'C:/Program Files/Eclipse Adoptium/jdk-21'])
    if (existsSync(join(c, 'bin', 'java' + exe))) return c;
  return null;
}
const jh = javaHome();
const jar = jh ? join(jh, 'bin', 'jar' + exe) : 'jar' + exe;
execFileSync(jar, ['cf', jarPath, '-C', outDir, '.'], { stdio: 'inherit' });
console.log('platform JAR written:', jarPath);
