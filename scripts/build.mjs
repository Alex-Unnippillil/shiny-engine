import { prepareNR } from './prepare-nr.mjs';
import { spawnSync } from 'node:child_process';
import { cp, mkdir, readdir, rm, copyFile } from 'node:fs/promises';
import { join } from 'node:path';
const command = process.platform === 'win32' ? 'tsc.cmd' : 'tsc';
await rm('dist', { recursive: true, force: true });
const result = spawnSync(command, [], { stdio: 'inherit', shell: process.platform === 'win32' });
if (result.status !== 0) process.exit(result.status ?? 1);
async function assets(path) {
  for (const item of await readdir(path, { withFileTypes: true })) {
    const source = join(path, item.name);
    if (item.isDirectory()) await assets(source);
    else if (/\.(html|css|svg|json)$/.test(item.name)) {
      const destination = join('dist/compiled', source); await mkdir(join(destination, '..'), { recursive: true });
      await copyFile(source, destination);
    }
  }
}
await assets('apps');
await prepareNR();
for (const kind of ['extension', 'site']) {
  await cp('dist/compiled', `dist/${kind}`, { recursive: true });
  await copyFile('apps/viewer/index.html', `dist/${kind}/index.html`);
}
await copyFile('apps/extension/manifest.json', 'dist/extension/manifest.json');
await rm('dist/site/apps/extension', { recursive: true, force: true });
console.log('Built dist/extension and dist/site. All executable assets are bundled; no model files are distributed.');
