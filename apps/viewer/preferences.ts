import { DEFAULTS, PRESETS, settings, type Settings } from '../../packages/contracts/index.js';

export const SETTINGS_KEY = 'shiny-settings-v1';
export type PresetName = keyof typeof PRESETS | 'custom';
export function presetName(config: Settings): PresetName {
  for (const name of Object.keys(PRESETS) as (keyof typeof PRESETS)[]) {
    const preset = PRESETS[name];
    if (Math.abs(config.strength - preset.strength) < 0.00001 && Math.abs(config.denoise - preset.denoise) < 0.00001) return name;
  }
  return 'custom';
}
export function readPreferences(storage: Pick<Storage, 'getItem'>): Settings {
  try {
    const raw = storage.getItem(SETTINGS_KEY);
    const value = raw ? settings(JSON.parse(raw)) : { ...DEFAULTS };
    // Only expose scales that can also be selected in the interface.
    return { ...value, scale: [1, 1.5, 2].includes(value.scale) ? value.scale : 1 };
  } catch { return { ...DEFAULTS }; }
}
export function writePreferences(storage: Pick<Storage, 'setItem'>, config: Settings): boolean {
  try { storage.setItem(SETTINGS_KEY, JSON.stringify(settings(config))); return true; }
  catch { return false; }
}
export function formatTime(seconds: number): string {
  if (!Number.isFinite(seconds) || seconds < 0) return '0:00';
  const total = Math.floor(seconds), hours = Math.floor(total / 3600);
  const minutes = Math.floor((total % 3600) / 60), rest = String(total % 60).padStart(2, '0');
  return hours ? `${hours}:${String(minutes).padStart(2, '0')}:${rest}` : `${minutes}:${rest}`;
}
export function exportName(label: string, view: string): string {
  const stem = label.replace(/\.[^.]+$/, '').replace(/[^a-zA-Z0-9_-]+/g, '-').replace(/^-+|-+$/g, '').slice(0, 72);
  return `${stem || 'shiny-engine'}-${view}.png`;
}
