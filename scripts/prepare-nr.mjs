/** Bundle immutable MIT reference code locally, without weights or upstream build scripts. */
import { readFile, writeFile, mkdir, copyFile } from 'node:fs/promises';
import { createHash } from 'node:crypto';
import { join, dirname } from 'node:path';
export async function prepareNR(root = 'dist/compiled') {
  const lock = JSON.parse(await readFile('models/nr-reference-lock.json', 'utf8'));
  const base = 'third_party/opendlss-nr';
  const dest = join(root, 'vendor/opendlss');
  const shaders = {};
  for (const [path, expected] of Object.entries(lock.files)) {
    let bytes;
    try { bytes = await readFile(join(base, path)); }
    catch { throw new Error('Pinned NR reference is missing. Run git submodule update --init -- third_party/opendlss-nr before building.'); }
    if (createHash('sha256').update(bytes).digest('hex') !== expected) throw new Error(`Pinned NR source changed: ${path}`);
    if (path.startsWith('ports/browser-webgpu/shaders/')) shaders[path.replace('ports/browser-webgpu/', '')] = bytes.toString('utf8');
    else {
      const target = join(dest, path.replace('ports/browser-webgpu/', ''));
      await mkdir(dirname(target), { recursive: true });
      await copyFile(join(base, path), target);
    }
  }
  await writeFile(join(dest, 'shader-sources.js'), `// Bundled MIT reference shaders; see LICENSE and upstream.json.\nexport const SHADERS = ${JSON.stringify(shaders)};\n`);
  let network = await readFile(join(dest, 'src/network.js'), 'utf8');
  const fetchStart = network.indexOf('const fetchText = async (path, base) => {');
  const fetchEnd = network.indexOf('\n};', fetchStart);
  if (fetchStart < 0 || fetchEnd < 0) throw new Error('Pinned network loader no longer matches the adapter.');
  network = `// Shiny Engine modification: bundled shader lookup replaces URL fetching.\nimport { SHADERS } from '../shader-sources.js';\n` +
    network.slice(0, fetchStart) + `const fetchText = async (path) => { if (!(path in SHADERS)) throw new Error('Unknown bundled shader'); return SHADERS[path]; };` + network.slice(fetchEnd + 3);
  await writeFile(join(dest, 'src/network.js'), network);
  let model = await readFile(join(dest, 'src/model.js'), 'utf8');
  const loadStart = model.indexOf('  async load(directory, onProgress) {');
  const loadEnd = model.indexOf('    for (const entry of manifest.tensors)', loadStart);
  if (loadStart < 0 || loadEnd < 0) throw new Error('Pinned model loader no longer matches the adapter.');
  model = '// Shiny Engine modification: accepts verified local data only; matrix layouts remain upstream.\n' + model.slice(0, loadStart) +
    `  async load({ manifest, stages }, onProgress) {\n    this.blockCount = manifest.totals.blockCount;\n    if (this.blockCount !== 71 || !(stages instanceof Map)) throw new Error('Invalid verified model');\n    let loaded = 0;\n    const total = manifest.stages.reduce((sum, stage) => sum + stage.packedByteLength, 0);\n    for (const stage of manifest.stages) {\n      const bytes = stages.get(stage.id);\n      if (!(bytes instanceof Uint8Array) || bytes.byteLength !== stage.packedByteLength) throw new Error('Invalid verified stage');\n      loaded += bytes.byteLength; onProgress?.(loaded, total);\n    }\n\n` + model.slice(loadEnd);
  await writeFile(join(dest, 'src/model.js'), model);
  await writeFile(join(dest, 'api.js'), `export {Network} from './src/network.js';\nexport {Model} from './src/model.js';\nexport {geometryFromValid} from './src/geometry.js';\nexport {SHADERS} from './shader-sources.js';\n`);
  await writeFile(join(dest, 'upstream.json'), JSON.stringify({ repository: lock.repository, commit: lock.commit,
    modifications: ['local-only model intake', 'bundled shader lookup'], weightsDistributed: false }, null, 2));
  const approval = JSON.parse(await readFile('models/nr-approvals.json', 'utf8'));
  if (approval.schema !== 1 || !Array.isArray(approval.approvals)) throw new Error('Invalid approval register.');
  await writeFile(join(root, 'packages/nr/approvals.js'), `// Repository-reviewed metadata, never model files.\nexport const APPROVALS = ${JSON.stringify(approval.approvals)};\n`);
}
