import { execFileSync } from 'node:child_process';
import { createHash } from 'node:crypto';
import { cpSync, createReadStream, existsSync, mkdirSync, mkdtempSync, rmSync } from 'node:fs';
import { basename, join, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';

const archiveName = 'MST2_EU_VW_ZR_P0369T_3G0919360LN.7z';
const archiveHash = '36bc458cb9ef96d92367a90496493415fba93ea4d7b1453ecaf4dcfe253659dd';
const hmiTar = 'cpu/hmizr_EU_stdNavi_stdZR_vw/33/default/cpu_hmizr_EU_stdNavi_stdZR_vw_33_default.tar.gz';
const hmiTarHash = '3c86b1d6c28cb1752ac3d286cd9794c87f8188a7c459e470be0bb4a9863ab743';

export function findFirmwareArchive(root) {
  return [root, join(root, 'firmware')]
    .map(directory => join(directory, archiveName))
    .find(path => existsSync(path));
}

function run(command, args, label) {
  try {
    execFileSync(command, args, { stdio: 'inherit' });
  } catch (error) {
    if (error.code === 'ENOENT') throw new Error(`Could not find ${command}. ${label}`, { cause: error });
    throw new Error(`${label} (exit code ${error.status ?? 'unknown'})`, { cause: error });
  }
}

export async function prepareFirmware(root, input) {
  const source = resolve(input);
  console.log(`[setup] Verifying ${basename(source)}...`);
  const hash = createHash('sha256');
  for await (const chunk of createReadStream(source)) hash.update(chunk);
  const digest = hash.digest('hex');
  if (digest !== archiveHash && digest !== hmiTarHash) {
    throw new Error('The file does not match the stock P0369 firmware or its HMI tarball. See docs/SETUP.md.');
  }

  const installed7z = join(process.env.ProgramFiles || 'C:/Program Files', '7-Zip', '7z.exe');
  const sevenZip = existsSync(installed7z) ? installed7z : '7z';
  const analysis = join(root, 'firmware', 'analysis');
  mkdirSync(analysis, { recursive: true });
  const temporary = mkdtempSync(join(analysis, '.prepare-'));
  const extract = (archive, entry, target, format = []) => {
    mkdirSync(target, { recursive: true });
    run(sevenZip, ['x', '-y', ...format, `-o${target}`, archive, entry], 'Install 7-Zip to extract the firmware.');
  };
  const node = (script, ...args) => run(process.execPath, [join(root, script), ...args], `Could not complete ${script}.`);

  try {
    const tarball = digest === hmiTarHash ? source : join(temporary, hmiTar);
    if (digest === archiveHash) extract(source, hmiTar, temporary);
    run('tar', ['-xzf', tarball, '-C', temporary, 'tsd/tmp/hmi'], 'Could not extract the HMI tarball.');

    const hmi = join(temporary, 'tsd', 'tmp', 'hmi');
    const stage = join(temporary, 'analysis');
    const platform = join(stage, 'rom');
    const hmiRom = join(stage, 'p0369', 'hmi');
    const runtime = join(stage, 'p0369', 'runtime');
    const mibHmi = join(temporary, 'MIBHMI.jxe');

    extract(join(hmi, 'tsd.mibstd2.hmi.v2.jxe'), 'rom.classes', platform, ['-tzip']);
    node('tools/ifs-extract.js', join(hmi, 'tsd.mibstd2.hmi.ifs'), 'MIBHMI.jxe', mibHmi);
    extract(mibHmi, 'rom.classes', hmiRom, ['-tzip']);
    const shaders = 'de/vw/mib/graphics/internal/shader';
    extract(mibHmi, `${shaders}/*`, temporary, ['-tzip']);
    cpSync(join(hmi, 'HMI'), join(runtime, 'HMI'), { recursive: true });
    cpSync(join(hmi, 'Resources'), join(runtime, 'Resources'), { recursive: true });
    for (const folder of ['fragment', 'vertex']) {
      cpSync(join(temporary, shaders, folder), join(runtime, folder), { recursive: true });
    }

    const skin0 = join(runtime, 'Resources', 'skin0');
    const skin1 = join(runtime, 'Resources', 'skin1');
    node('src/track-a/jxe/build-viewhandler-jar.js', join(skin1, 'viewhandler.zip'), join(skin1, 'viewhandler.jar'));
    node('tools/build-skin-cache.cjs', join(skin0, 'images.mcf'), join(runtime, 'skin0_cache.bin'));
    node('tools/build-skin-cache.cjs', join(skin1, 'images.mcf'), join(runtime, 'skin1_cache.bin'), '--expand-alpha');

    cpSync(runtime, join(analysis, 'p0369', 'runtime'), { recursive: true, force: false });
    cpSync(platform, join(analysis, 'rom'), { recursive: true, force: false });
    cpSync(hmiRom, join(analysis, 'p0369', 'hmi'), { recursive: true, force: false });
    console.log('[setup] P0369 runtime files are ready.');
  } finally {
    rmSync(temporary, { recursive: true, force: true });
  }
}

if (process.argv[1] && resolve(process.argv[1]) === fileURLToPath(import.meta.url)) {
  const root = fileURLToPath(new URL('../', import.meta.url));
  const archive = process.argv[2] || findFirmwareArchive(root);
  try {
    if (!archive) throw new Error(`Pass the path to ${archiveName} or the P0369 HMI tarball. See docs/SETUP.md.`);
    await prepareFirmware(root, archive);
  } catch (error) {
    console.error(`[setup] ${error.message}`);
    process.exitCode = 1;
  }
}
