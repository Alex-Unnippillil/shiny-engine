/** Data-only model intake. No DLLs, URLs, extraction, executable plugins or downloads. */
export const UPSTREAM = '9d08f4184bbcb9d858e2fb7a7834ec0837a9d2f1';
export const MAX_MODEL_BYTES = 256 * 1024 * 1024;
export const MAX_MANIFEST_BYTES = 2 * 1024 * 1024;
export interface Approval {
  manifestSha256: string; upstream: string; reviewId: string; reviewedBy: string;
  runtimeUse: 'approved'; parityEvidence: string; scope: 'browser-independent-frames';
}
export interface Stage { id: string | number; file: string; packedByteLength: number; sha256: string; }
export interface Tensor { name: string; block: number; layer: number; parameter?: string;
  stage: string | number; stageOffset: number; byteLength: number; }
export interface Manifest { totals: { blockCount: number }; stages: Stage[]; tensors: Tensor[]; }
export interface Inspection { manifest: Manifest; manifestSha256: string; approval: Approval | null;
  totalBytes: number; files: Map<string, File>; }
export interface VerifiedModel { manifestBytes: ArrayBuffer; manifest: Manifest; manifestSha256: string; stages: Map<string | number, Uint8Array>; }
const object = (x: unknown): x is Record<string, unknown> => !!x && typeof x === 'object' && !Array.isArray(x);
const integer = (x: unknown, min: number, max: number): x is number => Number.isSafeInteger(x) && Number(x) >= min && Number(x) <= max;
const hash = (x: unknown): x is string => typeof x === 'string' && /^[a-f0-9]{64}$/.test(x);
const ident = (x: unknown): x is string | number => integer(x, 0, 1024) || (typeof x === 'string' && /^[A-Za-z0-9_-]{1,80}$/.test(x));
export function safePath(value: unknown): string {
  if (typeof value !== 'string' || value.length > 240 || !/^[A-Za-z0-9_.\-/]+$/.test(value) ||
    value.split('/').some(x => !x || x === '.' || x === '..')) throw new Error('Unsafe model-relative path.');
  return value;
}
export function approvals(input: readonly unknown[]): Approval[] {
  const seen = new Set<string>();
  return input.map(value => {
    if (!object(value) || !hash(value.manifestSha256) || value.upstream !== UPSTREAM ||
      value.runtimeUse !== 'approved' || value.scope !== 'browser-independent-frames' ||
      !['reviewId', 'reviewedBy', 'parityEvidence'].every(k => typeof value[k] === 'string' && String(value[k]).trim().length >= 3) ||
      seen.has(value.manifestSha256)) throw new Error('Invalid or duplicate reviewed model approval.');
    seen.add(value.manifestSha256); return value as unknown as Approval;
  });
}
export function parseManifest(value: unknown): Manifest {
  if (!object(value) || !object(value.totals) || value.totals.blockCount !== 71 ||
    !Array.isArray(value.stages) || !value.stages.length || value.stages.length > 256 ||
    !Array.isArray(value.tensors) || !value.tensors.length || value.tensors.length > 1024) throw new Error('Expected the 71-block OpenDLSS-NR manifest.');
  const stages = new Map<string | number, Stage>(), paths = new Set<string>(), names = new Set<string>();
  let total = 0;
  for (const raw of value.stages) {
    if (!object(raw) || !ident(raw.id) || stages.has(raw.id) || !integer(raw.packedByteLength, 1, MAX_MODEL_BYTES) || !hash(raw.sha256))
      throw new Error('Invalid stage metadata, size or SHA-256.');
    const file = safePath(raw.file);
    if (!file.endsWith('.bin') || paths.has(file)) throw new Error('Duplicate or unsupported stage filename.');
    total += raw.packedByteLength;
    if (total > MAX_MODEL_BYTES) throw new Error('Model exceeds the 256 MiB limit.');
    paths.add(file); stages.set(raw.id, raw as unknown as Stage);
  }
  for (const raw of value.tensors) {
    if (!object(raw) || typeof raw.name !== 'string' || !/^block\d{1,2}\.layer\d{1,2}\.[a-z_0-9]+$/.test(raw.name) || names.has(raw.name) ||
      !integer(raw.block, 0, 70) || !integer(raw.layer, 0, 99) || !ident(raw.stage) ||
      !integer(raw.stageOffset, 0, MAX_MODEL_BYTES) || !integer(raw.byteLength, 1, MAX_MODEL_BYTES)) throw new Error('Invalid or duplicate tensor metadata.');
    const stage = stages.get(raw.stage);
    if (!stage || raw.stageOffset + raw.byteLength > stage.packedByteLength || !raw.name.startsWith(`block${raw.block}.layer${raw.layer}.`))
      throw new Error('Tensor exceeds its stage or its name contradicts its index.');
    names.add(raw.name);
  }
  if (!names.has('block70.layer0.blend_scale')) throw new Error('Model has no final composition parameter.');
  return value as unknown as Manifest;
}
export async function sha256(bytes: ArrayBuffer): Promise<string> {
  return Array.from(new Uint8Array(await crypto.subtle.digest('SHA-256', bytes)), b => b.toString(16).padStart(2, '0')).join('');
}
export function assertApproved(digest: string, trusted: readonly unknown[]): Approval {
  const entry = approvals(trusted).find(a => a.manifestSha256 === digest);
  if (!entry) throw new Error('This model has no repository-reviewed use and correctness approval. Inference remains disabled.');
  return entry;
}
export async function inspectModel(selected: readonly File[], trusted: readonly unknown[], signal: AbortSignal): Promise<Inspection> {
  signal.throwIfAborted();
  if (!selected.length || selected.length > 512) throw new Error('Select one model folder containing at most 512 files.');
  const files = new Map<string, File>(); let root = '';
  for (const file of selected) {
    const relative = safePath(file.webkitRelativePath || file.name), parts = relative.split('/');
    if (parts.length < 2) throw new Error('Select a folder, not individual files.');
    const folder = parts.shift()!;
    if (root && root !== folder) throw new Error('Select a single model root.'); root = folder;
    const path = parts.join('/');
    if (files.has(path)) throw new Error('Duplicate model path.'); files.set(path, file);
  }
  const file = files.get('manifest.json');
  if (!file || file.size > MAX_MANIFEST_BYTES) throw new Error('Missing manifest.json or manifest exceeds 2 MiB.');
  const bytes = await file.arrayBuffer(); signal.throwIfAborted();
  const manifest = parseManifest(JSON.parse(new TextDecoder('utf-8', { fatal: true }).decode(bytes)));
  const manifestSha256 = await sha256(bytes); signal.throwIfAborted();
  let totalBytes = 0;
  for (const stage of manifest.stages) {
    const file = files.get(`model/${stage.file}`);
    if (!file || file.size !== stage.packedByteLength) throw new Error(`Missing or wrong-sized model/${stage.file}.`);
    totalBytes += stage.packedByteLength;
  }
  const approval = approvals(trusted).find(a => a.manifestSha256 === manifestSha256) ?? null;
  return { manifest, manifestSha256, approval, totalBytes, files };
}
export async function verifyModel(inspection: Inspection, trusted: readonly unknown[], signal: AbortSignal,
  progress: (loaded: number, total: number) => void): Promise<VerifiedModel> {
  assertApproved(inspection.manifestSha256, trusted); signal.throwIfAborted();
  const manifestBytes = await inspection.files.get('manifest.json')!.arrayBuffer();
  if (await sha256(manifestBytes) !== inspection.manifestSha256) throw new Error('Manifest changed since inspection.');
  const manifest = parseManifest(JSON.parse(new TextDecoder('utf-8', { fatal: true }).decode(manifestBytes)));
  const stages = new Map<string | number, Uint8Array>(); let loaded = 0;
  try {
    for (const stage of manifest.stages) {
      signal.throwIfAborted(); const file = inspection.files.get(`model/${stage.file}`);
      if (!file || file.size !== stage.packedByteLength) throw new Error('Model stage changed.');
      const bytes = await file.arrayBuffer();
      if (await sha256(bytes) !== stage.sha256) throw new Error(`SHA-256 mismatch for stage ${stage.id}.`);
      signal.throwIfAborted(); stages.set(stage.id, new Uint8Array(bytes)); loaded += bytes.byteLength;
      progress(loaded, inspection.totalBytes);
    }
    return { manifestBytes, manifest, manifestSha256: inspection.manifestSha256, stages };
  } catch (error) { stages.clear(); throw error; }
}
export function processingSize(width: number, height: number, edge: number): { width: number; height: number } {
  if (![320, 512].includes(edge) || !integer(width, 1, 8192) || !integer(height, 1, 8192)) throw new Error('Unsupported input dimensions or processing size.');
  const ratio = Math.min(1, edge / Math.max(width, height));
  const size = { width: Math.max(1, Math.round(width * ratio)), height: Math.max(1, Math.round(height * ratio)) };
  if (Math.min(size.width, size.height) < 33) throw new Error('This aspect ratio is too extreme for the neural graph. Use the spatial lab.');
  return size;
}
/** Recheck transferred data inside the worker before GPU/model construction. */
export async function revalidateModel(input: VerifiedModel, trusted: readonly unknown[]): Promise<VerifiedModel> {
  if (!(input?.manifestBytes instanceof ArrayBuffer) || input.manifestBytes.byteLength > MAX_MANIFEST_BYTES || !(input.stages instanceof Map)) throw new Error('Invalid transferred model.');
  const digest = await sha256(input.manifestBytes); assertApproved(digest, trusted);
  if (digest !== input.manifestSha256) throw new Error('Transferred manifest fingerprint mismatch.');
  const manifest = parseManifest(JSON.parse(new TextDecoder('utf-8', { fatal: true }).decode(input.manifestBytes)));
  if (input.stages.size !== manifest.stages.length) throw new Error('Transferred stage count mismatch.');
  for (const stage of manifest.stages) {
    const bytes = input.stages.get(stage.id);
    if (!(bytes instanceof Uint8Array) || bytes.byteLength !== stage.packedByteLength || await sha256(bytes.slice().buffer) !== stage.sha256) throw new Error('Transferred stage fingerprint mismatch.');
  }
  return { ...input, manifest };
}
