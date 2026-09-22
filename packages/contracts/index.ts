/** Public contracts contain data only: never paths, HWNDs, code or frame payloads. */
export const VERSION = 1 as const;
export interface Settings {
  enabled: boolean;
  strength: number;
  denoise: number;
  split: number;
  scale: number;
}
export const DEFAULTS: Readonly<Settings> = Object.freeze({
  enabled: true, strength: 0.4, denoise: 0.08, split: 0.5, scale: 1,
});
export const PRESETS = {
  balanced: { strength: 0.4, denoise: 0.08 },
  soft: { strength: 0.18, denoise: 0.18 },
  detail: { strength: 0.65, denoise: 0.02 },
} as const;
export function record(value: unknown): value is Record<string, unknown> {
  return typeof value === 'object' && value !== null && !Array.isArray(value);
}
function bounded(value: unknown, min: number, max: number): value is number {
  return typeof value === 'number' && Number.isFinite(value) && value >= min && value <= max;
}
export function settings(value: unknown): Settings {
  if (!record(value) || Object.keys(value).some(k => !Object.hasOwn(DEFAULTS, k))) {
    throw new Error('Unrecognized settings.');
  }
  const s = { ...DEFAULTS, ...value };
  if (typeof s.enabled !== 'boolean' || !bounded(s.strength, 0, 1) ||
      !bounded(s.denoise, 0, 0.5) || !bounded(s.split, 0, 1) || !bounded(s.scale, 1, 2)) {
    throw new Error('Settings are outside supported limits.');
  }
  return s as Settings;
}
export function geometry(width: number, height: number, scale = 1, limit = 4096) {
  if (![width, height, scale, limit].every(Number.isFinite) || width < 1 || height < 1 ||
      scale < 1 || scale > 2 || limit < 1) throw new Error('Invalid media dimensions.');
  const factor = Math.min(scale, limit / width, limit / height,
    Math.sqrt(8_294_400 / (width * height)));
  return { width: Math.max(1, Math.floor(width * factor)), height: Math.max(1, Math.floor(height * factor)) };
}
export function percentile(samples: readonly number[], fraction: number): number {
  if (!samples.length) return 0;
  const sorted = [...samples].sort((a, b) => a - b);
  return sorted[Math.max(0, Math.min(sorted.length - 1, Math.ceil(fraction * sorted.length) - 1))] ?? 0;
}
export type Command = 'hello' | 'getCapabilities' | 'selectSource' | 'startSession' |
  'updateSettings' | 'pauseSession' | 'resumeSession' | 'stopSession' | 'getMetrics';
export interface NativeRequest {
  v: 1; id: string; command: Command; session?: string;
  settings?: Pick<Settings, 'enabled' | 'strength' | 'denoise' | 'split'>;
}
const COMMANDS: readonly string[] = ['hello', 'getCapabilities', 'selectSource', 'startSession',
  'updateSettings', 'pauseSession', 'resumeSession', 'stopSession', 'getMetrics'];
export function nativeRequest(value: unknown): NativeRequest {
  if (!record(value) || Object.keys(value).some(k => !['v', 'id', 'command', 'session', 'settings'].includes(k)) ||
      value.v !== VERSION || typeof value.id !== 'string' || !/^[\w-]{1,64}$/.test(value.id) ||
      typeof value.command !== 'string' || !COMMANDS.includes(value.command)) {
    throw new Error('Invalid protocol envelope.');
  }
  if (['startSession', 'updateSettings', 'pauseSession', 'resumeSession', 'stopSession', 'getMetrics'].includes(value.command) &&
      (typeof value.session !== 'string' || !/^[\w-]{1,64}$/.test(value.session))) {
    throw new Error('A valid session is required.');
  }
  if (value.session !== undefined && (typeof value.session !== 'string' || !/^[\w-]{1,64}$/.test(value.session))) {
    throw new Error('Invalid session.');
  }
  if (value.settings !== undefined) {
    if (value.command !== 'updateSettings' || !record(value.settings) || 'scale' in value.settings) {
      throw new Error('Invalid native settings.');
    }
    settings(value.settings);
  }
  if (value.command === 'updateSettings' && value.settings === undefined) throw new Error('Settings required.');
  return value as unknown as NativeRequest;
}
export const BACKENDS = [
  { id: 'spatial-webgpu-v1', label: 'Spatial · WebGPU', available: true, neural: false },
  { id: 'spatial-webgl2-v1', label: 'Spatial · WebGL2', available: true, neural: false },
  { id: 'nr-experimental', label: 'Experimental neural rendering', available: false,
    reason: 'Requires approved model rights, a native adapter and independent parity tests.' },
  { id: 'animation-neural', label: 'Animation model', available: false,
    reason: 'No model has been approved or bundled.' },
] as const;
