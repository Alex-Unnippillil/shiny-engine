import { readFile, readdir } from 'node:fs/promises';
import assert from 'node:assert/strict';
import { join } from 'node:path';
const manifest = JSON.parse(await readFile('dist/extension/manifest.json', 'utf8'));
assert.equal(manifest.manifest_version, 3);
assert.deepEqual(manifest.permissions, ['activeTab', 'scripting', 'storage']);
assert.equal(manifest.host_permissions, undefined);
assert.equal(manifest.externally_connectable, undefined);
assert(!manifest.content_security_policy.extension_pages.includes('unsafe-eval'));
const registry = JSON.parse(await readFile('models/registry.json', 'utf8'));
assert(registry.backends.filter(b => b.kind !== 'spatial').every(b => !b.enabled && !b.weights));
async function walk(path) {
  for (const entry of await readdir(path, { withFileTypes: true })) {
    const file = join(path, entry.name);
    if (entry.isDirectory()) { await walk(file); continue; }
    assert(!/\.(dll|onnx|safetensors|bin|pt)$/i.test(file), `Unapproved binary: ${file}`);
    if (file.endsWith('.js')) {
      const code = await readFile(file, 'utf8');
      assert(!/\beval\s*\(|new Function\s*\(|import\s*\(\s*['"]https?:/.test(code), `Remote/dynamic code: ${file}`);
      assert(!/\bfetch\s*\(|new WebSocket|XMLHttpRequest/.test(code), `Unexpected network path: ${file}`);
    }
  }
}
await walk('dist/extension');
console.log('PASS: MV3 permissions, bundled assets, no network inference, no unapproved model binaries.');
