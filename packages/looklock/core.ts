/** Deterministic, local still-frame composition. This module does not perform neural inference. */
export const MAX_PIXELS = 4_194_304;
export const MAX_ZONES = 24;
export interface Raster { width: number; height: number; data: Uint8ClampedArray }
export interface Zone { id: string; label: string; x: number; y: number; width: number; height: number }
export interface Adjustments { blend: number; feather: number; threshold: number }
export interface Identity { width: number; height: number; sourceHash: string; candidateHash: string }
export interface Recipe extends Identity, Adjustments { schema: 1; product: 'shiny-looklock'; zones: Zone[] }
export interface Metrics { pixels: number; protectedPixels: number; protectedMismatches: number; rawChanged: number; outputChanged: number; meanRawDelta: number; meanOutputDelta: number }
export const DEFAULTS: Readonly<Adjustments> = Object.freeze({ blend: 1, feather: 8, threshold: 8 });
const obj = (v: unknown): v is Record<string, unknown> => typeof v === 'object' && v !== null && !Array.isArray(v);
const finite = (v: unknown, lo: number, hi: number): v is number => typeof v === 'number' && Number.isFinite(v) && v >= lo && v <= hi;
const keys = (o: Record<string, unknown>, allowed: string[]) => Object.keys(o).every(k => allowed.includes(k));
export function geometry(width: number, height: number): void {
  if (!Number.isInteger(width) || !Number.isInteger(height) || width < 1 || height < 1 || width > 4096 || height > 4096 || width * height > MAX_PIXELS)
    throw new Error('Use a frame up to 4 megapixels and 4096 pixels per side. LookLock does not silently resize your media.');
}
export function validateRaster(r: Raster): void {
  geometry(r.width, r.height);
  if (!(r.data instanceof Uint8ClampedArray) || r.data.length !== r.width * r.height * 4) throw new Error('Invalid RGBA frame.');
}
export function adjustments(value: unknown): Adjustments {
  if (!obj(value) || !keys(value, ['blend', 'feather', 'threshold']) || !finite(value.blend, 0, 1) || !finite(value.feather, 0, 64) || !Number.isInteger(value.feather) || !finite(value.threshold, 1, 255) || !Number.isInteger(value.threshold)) throw new Error('Invalid comparison adjustments.');
  return { blend: value.blend, feather: value.feather, threshold: value.threshold };
}
export function zone(value: unknown): Zone {
  if (!obj(value) || !keys(value, ['id','label','x','y','width','height']) || typeof value.id !== 'string' || !/^[a-zA-Z0-9_-]{1,64}$/.test(value.id) || typeof value.label !== 'string' || value.label.length > 60 || /[\x00-\x1f\x7f]/.test(value.label) || !finite(value.x, 0, 1) || !finite(value.y, 0, 1) || !finite(value.width, 0.000001, 1) || !finite(value.height, 0.000001, 1) || value.x + value.width > 1.000000001 || value.y + value.height > 1.000000001) throw new Error('Invalid protected region.');
  return { id: value.id, label: value.label, x: value.x, y: value.y, width: value.width, height: value.height };
}
export function zones(value: unknown): Zone[] {
  if (!Array.isArray(value) || value.length > MAX_ZONES) throw new Error(`Use at most ${MAX_ZONES} protected regions.`);
  const result = value.map(zone);
  if (new Set(result.map(z => z.id)).size !== result.length) throw new Error('Duplicate region IDs.');
  return result;
}
/** Snap outwards, with epsilon for normalized integer coordinates that round in binary. */
export function bounds(z: Zone, width: number, height: number) {
  return { x: Math.max(0, Math.floor(z.x * width + 1e-7)), y: Math.max(0, Math.floor(z.y * height + 1e-7)),
    right: Math.min(width, Math.ceil((z.x + z.width) * width - 1e-7)), bottom: Math.min(height, Math.ceil((z.y + z.height) * height - 1e-7)) };
}
/** 255 = exact source. Feather extends OUTSIDE, never into the protected core. */
export function protectionMask(width: number, height: number, regions: Zone[], feather: number): Uint8Array {
  geometry(width, height); const valid = zones(regions);
  if (!Number.isInteger(feather) || feather < 0 || feather > 64) throw new Error('Invalid feather width.');
  const mask = new Uint8Array(width * height);
  for (const region of valid) {
    const b = bounds(region, width, height);
    for (let y = Math.max(0, b.y - feather); y < Math.min(height, b.bottom + feather); y++) {
      for (let x = Math.max(0, b.x - feather); x < Math.min(width, b.right + feather); x++) {
        const distance = Math.max(b.x - x, x - b.right + 1, b.y - y, y - b.bottom + 1, 0);
        const amount = distance === 0 ? 255 : Math.round(255 * Math.max(0, 1 - distance / (feather + 1)));
        const i = y * width + x; mask[i] = Math.max(mask[i]!, amount);
      }
    }
  }
  return mask;
}
export function compose(source: Raster, candidate: Raster, regions: Zone[], options: Adjustments) {
  validateRaster(source); validateRaster(candidate); const a = adjustments(options);
  if (source.width !== candidate.width || source.height !== candidate.height) throw new Error('Frames must have identical dimensions and matching crop. No automatic alignment is performed.');
  const mask = protectionMask(source.width, source.height, regions, a.feather);
  const data = new Uint8ClampedArray(source.data.length), heat = new Uint8ClampedArray(data.length);
  const metrics: Metrics = { pixels: mask.length, protectedPixels: 0, protectedMismatches: 0, rawChanged: 0, outputChanged: 0, meanRawDelta: 0, meanOutputDelta: 0 };
  let rawSum = 0, outputSum = 0;
  for (let p = 0; p < mask.length; p++) {
    const i = p * 4, w = a.blend * (1 - mask[p]! / 255), s = source.data, c = candidate.data;
    if (w === 0 || w === 1) { const from = w === 0 ? s : c; for (let k = 0; k < 4; k++) data[i+k] = from[i+k]!; }
    else {
      const alpha = s[i+3]! * (1-w) + c[i+3]! * w; data[i+3] = alpha;
      for (let k = 0; k < 3; k++) data[i+k] = alpha > 0 ? (s[i+k]! * s[i+3]! * (1-w) + c[i+k]! * c[i+3]! * w) / alpha : 0;
    }
    let raw = 0, output = 0;
    for (let k = 0; k < 4; k++) { raw = Math.max(raw, Math.abs(s[i+k]! - c[i+k]!)); output = Math.max(output, Math.abs(s[i+k]! - data[i+k]!)); }
    if (raw >= a.threshold) metrics.rawChanged++; if (output >= a.threshold) metrics.outputChanged++;
    if (mask[p] === 255) { metrics.protectedPixels++; if (output !== 0) metrics.protectedMismatches++; }
    rawSum += raw; outputSum += output;
    // Raw treatment differences, NOT a confidence map or a detector of invented details.
    const t = raw >= a.threshold ? Math.min(1, raw / 80) : 0;
    heat[i] = 20 + 235*t; heat[i+1] = 27 + 140*t; heat[i+2] = 38 + 18*t; heat[i+3] = 255;
  }
  metrics.meanRawDelta = rawSum / mask.length; metrics.meanOutputDelta = outputSum / mask.length;
  return { output: { width: source.width, height: source.height, data }, heatmap: { width: source.width, height: source.height, data: heat }, mask, metrics };
}
export async function rasterHash(r: Raster): Promise<string> {
  validateRaster(r); const header = new TextEncoder().encode(`looklock-rgba-v1:${r.width}:${r.height}:`);
  const bytes = new Uint8Array(header.length + r.data.length); bytes.set(header); bytes.set(r.data, header.length);
  return Array.from(new Uint8Array(await crypto.subtle.digest('SHA-256', bytes)), b => b.toString(16).padStart(2, '0')).join('');
}
export function recipe(identity: Identity, regions: Zone[], config: Adjustments): Recipe {
  geometry(identity.width, identity.height);
  if (![identity.sourceHash, identity.candidateHash].every(h => /^[a-f0-9]{64}$/.test(h))) throw new Error('Invalid frame identity.');
  return { schema: 1, product: 'shiny-looklock', width: identity.width, height: identity.height,
    sourceHash: identity.sourceHash, candidateHash: identity.candidateHash, ...adjustments(config), zones: zones(regions) };
}
export function parseRecipe(text: string, expected: Identity): Recipe {
  if (text.length > 32768) throw new Error('Recipe exceeds 32 KiB.');
  const r: unknown = JSON.parse(text);
  if (!obj(r) || !keys(r, ['schema','product','width','height','sourceHash','candidateHash','blend','feather','threshold','zones']) || r.schema !== 1 || r.product !== 'shiny-looklock') throw new Error('This is not a supported LookLock recipe.');
  for (const k of ['width','height','sourceHash','candidateHash'] as const) if (r[k] !== expected[k]) throw new Error('Recipe belongs to different decoded frames. Load the exact original and treatment first.');
  return recipe(expected, zones(r.zones), adjustments({ blend: r.blend, feather: r.feather, threshold: r.threshold }));
}
export class History<T> {
  private past: T[] = []; private future: T[] = [];
  constructor(public value: T, private limit = 60) {}
  get canUndo() { return this.past.length > 0; }
  get canRedo() { return this.future.length > 0; }
  set(value: T) { this.past.push(structuredClone(this.value)); if (this.past.length > this.limit) this.past.shift(); this.future = []; this.value = structuredClone(value); }
  undo() { if (!this.canUndo) return; this.future.push(structuredClone(this.value)); this.value = this.past.pop()!; }
  redo() { if (!this.canRedo) return; this.past.push(structuredClone(this.value)); this.value = this.future.pop()!; }
}
