import { DEFAULTS, PRESETS, settings, percentile, record, type Settings, type Command } from '../../packages/contracts/index.js';
import { browser, request } from '../../packages/contracts/browser.js';
import { createRenderer, dimensions, type Renderer } from '../../packages/gpu-web/renderer.js';
import { FrameLoop } from '../../packages/gpu-web/scheduler.js';
import { fixture } from './demo.js';
import { loadFile, loadStream, fileType, type Media } from './media.js';
import { presetName, readPreferences, writePreferences, formatTime, exportName } from './preferences.js';

function element<T extends HTMLElement>(id: string): T {
  const found = document.getElementById(id);
  if (!found) throw new Error(`Missing application element: ${id}`);
  return found as T;
}
const mount = element('canvas-mount'), stage = element('stage');
const fileInput = element<HTMLInputElement>('file');
const dialog = element<HTMLDialogElement>('help-dialog');
const sliders = { strength: element<HTMLInputElement>('strength'), denoise: element<HTMLInputElement>('denoise'),
  split: element<HTMLInputElement>('split'), scale: element<HTMLSelectElement>('scale') };
let config: Settings = { ...DEFAULTS };
try { config = readPreferences(localStorage); } catch { /* Storage may be blocked. */ }
let active: Media | null = null, renderer: Renderer | null = null, canvas: HTMLCanvasElement | null = null;
let loop: FrameLoop | null = null, mediaEvents: AbortController | null = null;
let generation = 0, inFlight: Promise<number> | null = null, dirty = false;
let loadController: AbortController | null = null, loading = false;
let operation: 'export' | 'benchmark' | null = null, cancelled = false;
let samples: number[] = [], lastSplit = config.split > 0 && config.split < 1 ? config.split : 0.5;
let nativeSession: string | null = null, nativeConnected = false, nativeBusy = false;
let storageWarningShown = false, dragging = false;
let videoVolume = 1, videoMuted = false, videoRate = 1;
const currentVideo = () => active?.source instanceof HTMLVideoElement ? active.source : null;
function status(message: string, error = false) {
  element('status').textContent = message; element('status').classList.toggle('error', error);
}
function persist() {
  try {
    if (!writePreferences(localStorage, config) && !storageWarningShown) {
      storageWarningShown = true; element('preferences-note').textContent = 'Settings are session-only because browser storage is unavailable.';
    }
  } catch { element('preferences-note').textContent = 'Settings are session-only in this browser.'; }
}
function controls() {
  sliders.strength.value = String(Math.round(config.strength * 100));
  sliders.denoise.value = String(Math.round(config.denoise * 100));
  sliders.split.value = String(Math.round(config.split * 100)); sliders.scale.value = String(config.scale);
  element('strength-value').textContent = `${Math.round(config.strength * 100)}%`;
  element('denoise-value').textContent = `${Math.round(config.denoise * 100)}%`;
  const splitLabel = `${Math.round(config.split * 100)}% original / ${100 - Math.round(config.split * 100)}% enhanced`;
  element('split-value').textContent = `${Math.round(config.split * 100)} / ${100 - Math.round(config.split * 100)}`;
  sliders.split.setAttribute('aria-valuetext', splitLabel);
  const divider = element<HTMLButtonElement>('comparison-line');
  divider.style.left = `${config.split * 100}%`;
  divider.hidden = !config.enabled || !renderer;
  divider.setAttribute('aria-valuenow', String(Math.round(config.split * 100)));
  divider.setAttribute('aria-valuetext', splitLabel);
  divider.disabled = !renderer || !!operation || loading;
  element('original-tag').hidden = !active || (!!renderer && config.enabled && config.split === 0);
  element('enhanced-tag').hidden = !renderer || !config.enabled || config.split === 1;
  const view = !config.enabled || config.split === 1 ? 'original' : config.split === 0 ? 'enhanced' : 'split';
  for (const item of document.querySelectorAll<HTMLButtonElement>('[data-view]')) {
    item.setAttribute('aria-pressed', String(item.dataset.view === view)); item.disabled = !renderer || !!operation || loading;
  }
  const preset = presetName(config);
  element('preset-name').textContent = preset === 'custom' ? 'Custom adjustments' : `${preset[0]!.toUpperCase()}${preset.slice(1)} preset`;
  for (const button of document.querySelectorAll<HTMLButtonElement>('[data-preset]')) {
    button.setAttribute('aria-pressed', String(button.dataset.preset === preset)); button.disabled = !!operation;
  }
  for (const input of Object.values(sliders)) input.disabled = !!operation;
  sliders.split.disabled = !renderer || !config.enabled || !!operation || loading;
  element<HTMLButtonElement>('reset').disabled = !!operation;
  element<HTMLButtonElement>('stop').disabled = !active && !loading;
  element<HTMLButtonElement>('export').disabled = !renderer || !!operation || loading;
  element<HTMLButtonElement>('benchmark').disabled = !renderer || !!currentVideo() || !!operation || loading;
  element<HTMLButtonElement>('report').disabled = samples.length === 0;
  element<HTMLButtonElement>('focus').disabled = !active;
  element('session-dot').classList.toggle('idle', !active);
  element('session-kind').textContent = loading ? 'LOADING' : !active ? 'IDLE' : active.kind === 'capture' ? 'LIVE CAPTURE' : active.kind.toUpperCase();
  element('loading-banner').hidden = !loading && !operation;
  element<HTMLButtonElement>('cancel-operation').hidden = !loading && operation !== 'benchmark';
  stage.setAttribute('aria-busy', String(loading || !!operation));
  element('export-note').textContent = renderer && canvas ? `PNG · ${canvas.width} × ${canvas.height} · saved on this device` : 'Load media to export a still frame.';
  updateTransport();
}
function diagnostics() {
  element('metric-p95').textContent = samples.length ? `${percentile(samples, 0.95).toFixed(1)} ms` : '—';
  element('metric-count').textContent = String(samples.length);
  element('metric-backend').textContent = renderer?.label.replace('Spatial · ', '') ?? 'Original';
  element<HTMLButtonElement>('report').disabled = samples.length === 0;
}
function cancelLoad() {
  loadController?.abort(); loadController = null; loading = false;
}
function beginLoad(message: string): AbortSignal {
  cancelLoad(); cancelled = true;
  loadController = new AbortController(); loading = true;
  element('loading-text').textContent = message; controls();
  return loadController.signal;
}
function isCurrent(signal: AbortSignal) { return !signal.aborted && signal === loadController?.signal; }
function finishLoad(signal: AbortSignal) {
  if (!isCurrent(signal)) return;
  loadController = null; loading = false; controls();
}
function cleanup() {
  generation++; cancelled = true; operation = null;
  loop?.stop(); loop = null; mediaEvents?.abort(); mediaEvents = null;
  const media = active, old = renderer, pending = inFlight;
  // Stop audio and capture immediately; retain media surfaces until their GPU consumer completes.
  const video = currentVideo(); video?.pause();
  if (video?.srcObject instanceof MediaStream) video.srcObject.getTracks().forEach(track => track.stop());
  renderer = null; active = null; canvas = null; inFlight = null; dirty = false;
  if (pending) void pending.catch(() => {}).finally(() => { old?.release(); media?.dispose(); });
  else { old?.release(); media?.dispose(); }
  mount.replaceChildren(); element('source-slot').replaceChildren(); samples = []; diagnostics();
  element('dimensions').textContent = '—'; element('input-dimensions').textContent = 'No media loaded';
  element('backend-pill').textContent = 'No active processing';
}
function fallback(error: unknown) {
  loop?.stop(); loop = null;
  const old = renderer, pending = inFlight; renderer = null; canvas = null;
  if (pending) void pending.catch(() => {}).finally(() => old?.release()); else old?.release();
  const source = active?.source;
  if (source instanceof HTMLElement) { mount.replaceChildren(source); source.hidden = false; }
  else { mount.replaceChildren(); element('empty').hidden = false; }
  element('backend-pill').textContent = 'Original · enhancement unavailable';
  if (active) { const [w, h] = dimensions(active.source); element('dimensions').textContent = `${w} × ${h}`; }
  controls(); diagnostics();
  status(`${error instanceof Error ? error.message : 'This source cannot be processed.'} Original playback is preserved.`, true);
}
async function draw(override?: Settings): Promise<number> {
  if (!renderer || !active) return 0;
  if (inFlight) { dirty = true; return inFlight; }
  const epoch = generation;
  const task = renderer.render(active.source, override ?? config); inFlight = task;
  try {
    const ms = await task;
    if (epoch !== generation) return ms;
    if (!override) { samples.push(ms); if (samples.length > 600) samples.shift(); diagnostics(); }
    if (canvas) {
      element('dimensions').textContent = `${canvas.width} × ${canvas.height}`;
      element('export-note').textContent = `PNG · ${canvas.width} × ${canvas.height} · saved on this device`;
    }
    return ms;
  } finally {
    if (epoch === generation) {
      inFlight = null;
      if (dirty && !operation) { dirty = false; queueMicrotask(redraw); }
    }
  }
}
function redraw() {
  if (operation) { dirty = true; return; }
  const epoch = generation;
  void draw().catch(error => { if (epoch === generation) fallback(error); });
}
function apply(next: Settings, save = true) {
  config = settings(next);
  if (config.split > 0 && config.split < 1) lastSplit = config.split;
  controls(); if (save) persist(); redraw();
}
function setView(mode: string) {
  if (!renderer || operation || loading) return;
  if (mode === 'original') apply({ ...config, enabled: false });
  else apply({ ...config, enabled: true, split: mode === 'enhanced' ? 0 : lastSplit });
}
async function attach(media: Media, signal: AbortSignal) {
  if (!isCurrent(signal)) { media.dispose(); return; }
  signal.addEventListener('abort', media.dispose, { once: true });
  // Prepare shaders before replacing the working source. Cancelling initialization preserves it.
  let prepared: Awaited<ReturnType<typeof createRenderer>> | null = null;
  let preparationError: unknown = null;
  try { prepared = await createRenderer(document.createElement('div')); } catch (error) { preparationError = error; }
  signal.removeEventListener('abort', media.dispose);
  if (!isCurrent(signal)) { prepared?.renderer.release(); media.dispose(); return; }
  cleanup(); const epoch = generation; active = media;
  const [width, height] = dimensions(media.source);
  element('source-name').textContent = media.label; element('source-name').title = media.label;
  element('input-dimensions').textContent = `${width} × ${height} source`;
  stage.style.setProperty('--source-ratio', String(width / height));
  element('empty').hidden = true;
  const video = currentVideo();
  if (video) {
    video.id = 'source-video'; video.setAttribute('aria-label', 'Original video playback'); video.hidden = true;
    element('source-slot').replaceChildren(video);
    mediaEvents = new AbortController(); const options = { signal: mediaEvents.signal };
    for (const event of ['play', 'pause', 'timeupdate', 'durationchange', 'volumechange', 'ratechange', 'ended']) video.addEventListener(event, updateTransport, options);
    video.addEventListener('error', () => { if (epoch === generation) fallback(new Error('Video playback was interrupted. Try another file or codec.')); }, options);
    if (media.kind === 'capture' && video.srcObject instanceof MediaStream) {
      video.srcObject.getVideoTracks()[0]?.addEventListener('ended', () => { if (epoch === generation) stop(); }, options);
    }
    if (media.audio) { video.volume = videoVolume; video.muted = videoMuted; video.playbackRate = videoRate; }
  }
  controls();
  try {
    if (!prepared) throw preparationError ?? new Error('No GPU backend is available.');
    renderer = prepared.renderer; canvas = prepared.canvas; mount.replaceChildren(canvas);
    canvas.setAttribute('aria-label', 'Processed media preview');
    element('backend-pill').textContent = renderer.label;
    controls(); await draw();
    if (epoch !== generation) return;
    if (video) {
      loop = new FrameLoop(video, draw, error => { if (epoch === generation) fallback(error); }); loop.start();
      try { await video.play(); } catch { status('Video ready. Press Play to begin. Audio is controlled below the preview.'); return; }
    }
    status(media.kind === 'capture' ? 'Window sharing is active. Audio stays in the source app. Stop session ends capture.' : 'Ready. Drag the divider to compare. Your file stays on this device.');
  } catch (error) { if (epoch === generation) fallback(error); }
  finally { if (epoch === generation) { finishLoad(signal); controls(); } }
}
async function openFile(file: File) {
  try { fileType(file); } catch (error) { status((error as Error).message, true); return; }
  const signal = beginLoad('Opening your file. Your current preview stays available until it is ready…');
  try { await attach(await loadFile(file, signal), signal); }
  catch (error) { if (isCurrent(signal)) status((error as Error).message, true); }
  finally { finishLoad(signal); }
}
async function demo() {
  const signal = beginLoad('Preparing the local detail study…');
  const source = fixture();
  await attach({ source, label: 'Synthetic detail study · local demo', kind: 'demo', audio: false, dispose: () => source.remove() }, signal);
}
async function shareWindow() {
  if (!navigator.mediaDevices?.getDisplayMedia) { status('Window sharing is unavailable. Use Chrome or Edge on a secure page, or open a local file.', true); return; }
  // Preserve transient activation; never begin capturing without the system picker.
  const pending = navigator.mediaDevices.getDisplayMedia({ video: { frameRate: { max: 30 } }, audio: false,
    selfBrowserSurface: 'exclude', monitorTypeSurfaces: 'exclude', surfaceSwitching: 'exclude' } as DisplayMediaStreamOptions);
  const signal = beginLoad('Choose a different window in the browser picker. Cancel keeps your current media.');
  try {
    const acquired = await pending;
    if (!isCurrent(signal)) { acquired.getTracks().forEach(track => track.stop()); return; }
    if (acquired.getVideoTracks()[0]?.getSettings().displaySurface === 'monitor') {
      acquired.getTracks().forEach(track => track.stop()); throw new Error('Monitor capture is disabled here to prevent feedback. Use the Windows companion.');
    }
    await attach(await loadStream(acquired, signal, 'Shared window · capture active'), signal);
  } catch (error) {
    if (isCurrent(signal)) status(error instanceof DOMException && error.name === 'NotAllowedError'
      ? 'Window sharing cancelled or not permitted. Your current media is unchanged.' : (error as Error).message, true);
  } finally { finishLoad(signal); }
}
function stop() {
  cancelLoad(); cleanup();
  element('empty').hidden = false; element('empty-text').textContent = 'Open a local file or try the demo. Nothing is recorded or uploaded.';
  element('source-name').textContent = 'No active source'; element('source-name').title = '';
  stage.style.setProperty('--source-ratio', String(16 / 9));
  controls(); status('Session stopped. No browser capture is active.');
}
function save(blob: Blob, name: string) {
  const url = URL.createObjectURL(blob); const a = document.createElement('a'); a.href = url; a.download = name; a.click();
  setTimeout(() => URL.revokeObjectURL(url), 2000);
}
async function exportFrame() {
  if (!renderer || !active || !canvas || operation || loading) return;
  const epoch = generation, output = canvas, media = active, wasLoop = loop;
  const view = element<HTMLSelectElement>('export-view').value;
  const exportConfig = view === 'original' ? { ...config, enabled: false } : view === 'comparison' ? config : { ...config, enabled: true, split: 0 };
  wasLoop?.stop(); operation = 'export'; element('loading-text').textContent = 'Saving a local PNG…'; controls();
  try {
    if (inFlight) await inFlight;
    if (epoch !== generation) return;
    await draw(exportConfig);
    if (epoch !== generation) return;
    const blob = await new Promise<Blob | null>(resolve => output.toBlob(resolve, 'image/png'));
    if (!blob) throw new Error('PNG export was unavailable.');
    if (epoch === generation) { save(blob, exportName(media.kind === 'image' || media.kind === 'video' ? media.label : 'shiny-engine', view)); status(`${view[0]!.toUpperCase()}${view.slice(1)} PNG saved. The source file is unchanged.`); }
  } catch (error) { if (epoch === generation) status((error as Error).message, true); }
  finally {
    if (epoch === generation) {
      operation = null; dirty = false; controls();
      await draw().catch(error => { if (epoch === generation) fallback(error); });
      if (epoch === generation && renderer) wasLoop?.start();
    }
  }
}
async function benchmark() {
  if (!active || !renderer || operation || loading || currentVideo()) return;
  const epoch = generation; operation = 'benchmark'; cancelled = false;
  if (inFlight) await inFlight.catch(() => {});
  if (epoch !== generation) return;
  samples = []; dirty = false; controls();
  try {
    for (let i = 0; i < 60 && epoch === generation && !cancelled; i++) {
      element('loading-text').textContent = `Measuring frame ${i + 1} of 60…`; await draw();
    }
    if (epoch === generation) status(cancelled ? `Measurement cancelled after ${samples.length} frames. Your preview is unchanged.` : `60-frame diagnostic complete. p95 ${percentile(samples, 0.95).toFixed(1)} ms; not a real-time certification.`);
  } catch (error) { if (epoch === generation) fallback(error); }
  finally { if (epoch === generation) { operation = null; controls(); if (dirty) { dirty = false; redraw(); } } }
}
function updateTransport() {
  const video = currentVideo();
  element('transport').hidden = !video || !renderer;
  if (!video) return;
  const live = active?.kind === 'capture', duration = video.duration;
  const seek = element<HTMLInputElement>('seek');
  element('seek-controls').hidden = live;
  seek.disabled = !Number.isFinite(duration) || duration <= 0; seek.max = String(Number.isFinite(duration) ? duration : 0);
  if (document.activeElement !== seek) seek.value = String(video.currentTime);
  seek.setAttribute('aria-valuetext', `${formatTime(video.currentTime)} of ${formatTime(duration)}`);
  element('time').textContent = live ? 'LIVE' : `${formatTime(video.currentTime)} / ${formatTime(duration)}`;
  element('play').textContent = video.ended ? 'Replay' : video.paused ? 'Play' : 'Pause';
  element('play').setAttribute('aria-label', video.ended ? 'Replay video' : video.paused ? 'Play video' : 'Pause video');
  element('audio-controls').hidden = !active?.audio; element('rate-control').hidden = live;
  element('mute').textContent = video.muted || video.volume === 0 ? 'Unmute' : 'Mute';
  element('mute').setAttribute('aria-pressed', String(video.muted));
  element<HTMLInputElement>('volume').value = String(Math.round(video.volume * 100));
  element<HTMLSelectElement>('rate').value = String(video.playbackRate);
}
async function playPause() {
  const video = currentVideo(); if (!video) return;
  if (video.paused || video.ended) { try { await video.play(); } catch { status('Playback could not start. Try re-opening this file.', true); } }
  else video.pause();
}
element('play').addEventListener('click', () => { void playPause(); });
element<HTMLInputElement>('seek').addEventListener('input', event => {
  const video = currentVideo(); if (!video || !Number.isFinite(video.duration)) return;
  video.currentTime = Math.max(0, Math.min(video.duration, Number((event.target as HTMLInputElement).value))); updateTransport();
});
element('mute').addEventListener('click', () => { const video = currentVideo(); if (video) { videoMuted = video.muted = !video.muted; updateTransport(); } });
element<HTMLInputElement>('volume').addEventListener('input', event => {
  const video = currentVideo(); if (video) { videoVolume = video.volume = Number((event.target as HTMLInputElement).value) / 100; videoMuted = video.muted = false; updateTransport(); }
});
element<HTMLSelectElement>('rate').addEventListener('change', event => {
  const video = currentVideo(); if (video && active?.kind !== 'capture') { videoRate = video.playbackRate = Number((event.target as HTMLSelectElement).value); }
});

// A real draggable divider; the range input below remains available to all input methods.
function moveDivider(clientX: number) {
  const rect = stage.getBoundingClientRect();
  apply({ ...config, enabled: true, split: Math.round(Math.max(0, Math.min(1, (clientX - rect.left) / rect.width)) * 100) / 100 }, false);
}
const divider = element<HTMLButtonElement>('comparison-line');
divider.addEventListener('pointerdown', event => {
  if (event.button !== 0 || operation || loading) return;
  dragging = true; divider.setPointerCapture(event.pointerId); moveDivider(event.clientX); event.preventDefault();
});
divider.addEventListener('pointermove', event => { if (dragging) moveDivider(event.clientX); });
for (const event of ['pointerup', 'pointercancel', 'lostpointercapture']) divider.addEventListener(event, () => { if (dragging) { dragging = false; persist(); } });
divider.addEventListener('keydown', event => {
  if (!['ArrowLeft', 'ArrowRight', 'Home', 'End'].includes(event.key)) return;
  event.preventDefault();
  const split = event.key === 'Home' ? 0 : event.key === 'End' ? 1 : Math.min(1, Math.max(0, config.split + (event.key === 'ArrowLeft' ? -1 : 1) * (event.shiftKey ? 0.1 : 0.01)));
  apply({ ...config, enabled: true, split });
});
for (const [key, input] of Object.entries(sliders)) input.addEventListener('input', () => apply({ ...config, [key]: Number(input.value) / (key === 'scale' ? 1 : 100) }));
for (const button of document.querySelectorAll<HTMLButtonElement>('[data-preset]')) button.addEventListener('click', () => {
  const key = button.dataset.preset as keyof typeof PRESETS; apply({ ...config, ...PRESETS[key], enabled: true });
});
for (const button of document.querySelectorAll<HTMLButtonElement>('[data-view]')) button.addEventListener('click', () => setView(button.dataset.view!));
element('reset').addEventListener('click', () => { apply({ ...DEFAULTS }); status('Adjustments reset to Balanced. Your media is unchanged.'); });
element('open-file').addEventListener('click', () => fileInput.click());
element('empty-open').addEventListener('click', () => fileInput.click());
fileInput.addEventListener('change', () => { const file = fileInput.files?.[0]; fileInput.value = ''; if (file) void openFile(file); });
element('share').addEventListener('click', () => { void shareWindow(); });
element('sample').addEventListener('click', () => { void demo(); });
element('empty-demo').addEventListener('click', () => { void demo(); });
element('stop').addEventListener('click', stop);
element('export').addEventListener('click', () => { void exportFrame(); });
element('benchmark').addEventListener('click', () => { void benchmark(); });
element('cancel-operation').addEventListener('click', () => {
  if (loading) { cancelLoad(); controls(); status('Loading cancelled. Your current media is unchanged. Close the browser picker if it is still open.'); }
  else if (operation === 'benchmark') { cancelled = true; element('loading-text').textContent = 'Finishing the current frame…'; }
});
element('report').addEventListener('click', () => {
  save(new Blob([JSON.stringify({ schema: 1, product: 'shiny-engine', version: '0.2.0', timestamp: new Date().toISOString(),
    backend: renderer?.label ?? 'original', settings: config,
    scope: 'CPU submission to GPU completion; includes browser scheduling. No capture/display latency certification.',
    samples: samples.length, medianMs: percentile(samples, 0.5), p95Ms: percentile(samples, 0.95), p99Ms: percentile(samples, 0.99),
    output: canvas ? { width: canvas.width, height: canvas.height } : null, certification: 'unverified', sourceNamesIncluded: false,
  }, null, 2)], { type: 'application/json' }), 'shiny-engine-diagnostics.json');
});
async function focusView() {
  try {
    if (document.fullscreenElement) await document.exitFullscreen();
    else if (element('lab').requestFullscreen) await element('lab').requestFullscreen();
    else throw new Error('Fullscreen is unavailable in this browser.');
  } catch { status('Fullscreen is unavailable here. The preview and controls remain usable.', true); }
}
element('focus').addEventListener('click', () => { void focusView(); });
document.addEventListener('fullscreenchange', () => {
  element('focus').textContent = document.fullscreenElement ? 'Exit fullscreen' : 'Fullscreen';
  element('focus').setAttribute('aria-pressed', String(!!document.fullscreenElement));
});
element('help').addEventListener('click', () => dialog.showModal());
element('close-help').addEventListener('click', () => dialog.close());
document.addEventListener('keydown', event => {
  if (event.defaultPrevented || event.repeat || event.ctrlKey || event.metaKey || event.altKey) return;
  if (dialog.open) return;
  if (event.key === 'Escape') {
    if (document.fullscreenElement) return;
    if (loading) { element('cancel-operation').click(); return; }
    if (operation === 'benchmark') { element('cancel-operation').click(); return; }
    stop(); return;
  }
  if (event.target instanceof Element && event.target.closest('input,select,textarea,[contenteditable=true]')) return;
  if (event.key.toLowerCase() === 'b') { setView(config.enabled ? 'original' : 'split'); event.preventDefault(); }
  if (event.key.toLowerCase() === 'c') { setView('split'); event.preventDefault(); }
  if (event.key.toLowerCase() === 'e') { void exportFrame(); event.preventDefault(); }
  if (event.key === '?') { dialog.showModal(); event.preventDefault(); }
  if (event.key === ' ' && !(event.target instanceof Element && event.target.closest('button,a,summary,video'))) { event.preventDefault(); void playPause(); }
});
let dragDepth = 0;
document.addEventListener('dragenter', event => {
  if (!event.dataTransfer?.types.includes('Files')) return;
  event.preventDefault(); dragDepth++; element('drop-overlay').hidden = false;
});
document.addEventListener('dragover', event => { if (event.dataTransfer?.types.includes('Files')) event.preventDefault(); });
document.addEventListener('dragleave', () => { dragDepth = Math.max(0, dragDepth - 1); if (!dragDepth) element('drop-overlay').hidden = true; });
document.addEventListener('drop', event => {
  event.preventDefault(); dragDepth = 0; element('drop-overlay').hidden = true;
  const files = event.dataTransfer?.files;
  if (files?.length === 1) void openFile(files[0]!);
  else if (files?.length) status('Open one file at a time. Your current media is unchanged.', true);
});

async function native(command: Command) {
  const response = await request({ type: 'native', payload: { v: 1, id: crypto.randomUUID(), command, ...(nativeSession ? { session: nativeSession } : {}) } });
  if (!record(response.result)) throw new Error('Invalid companion response.');
  if (response.result.ok !== true) throw new Error(String(response.result.error ?? 'Native operation failed.'));
  return response.result;
}
function nativeStatus(text: string) { element('native-status').textContent = text; }
function nativeControls() {
  element<HTMLButtonElement>('connect-native').disabled = nativeBusy || !browser?.runtime?.id;
  element<HTMLButtonElement>('pick-native').disabled = nativeBusy || !nativeConnected || !!nativeSession;
  element<HTMLButtonElement>('stop-native').disabled = nativeBusy || !nativeSession;
}
async function nativeAction(task: () => Promise<void>) {
  if (nativeBusy) return; nativeBusy = true; nativeControls();
  try { await task(); } catch (error) { nativeStatus(`${(error as Error).message} Check the setup instructions below. Ctrl+Shift+F10 stops the native preview.`); }
  finally { nativeBusy = false; nativeControls(); }
}
element('connect-native').addEventListener('click', () => {
  // Request the optional permission directly within the user gesture.
  if (!browser?.runtime?.id) return;
  const permission = browser.permissions.request({ permissions: ['nativeMessaging'] });
  void nativeAction(async () => {
    nativeStatus('Connecting to the installed companion…');
    if (!await permission) throw new Error('Native permission was not granted.');
    const result = await native('getCapabilities'); nativeConnected = true;
    nativeStatus(`Companion connected. ${String(result.description ?? 'Choose a source to begin.')}`);
  });
});
element('pick-native').addEventListener('click', () => { void nativeAction(async () => {
  nativeStatus('Choose a source in the Windows picker…');
  const result = await native('selectSource');
  if (typeof result.session !== 'string') throw new Error('No source selected.');
  nativeSession = result.session; await native('startSession');
  nativeStatus('Native capture active in a separate preview. Stop native or Ctrl+Shift+F10 ends capture.');
}); });
element('stop-native').addEventListener('click', () => { void nativeAction(async () => {
  await native('stopSession'); nativeSession = null; nativeStatus('Native capture stopped.');
}); });
if (!browser?.runtime?.id) nativeStatus('Open Media Lab from the installed extension to connect. Local files work here without installation.');
else { element('extension-id').textContent = browser.runtime.id; element('extension-info').hidden = false; }
element('copy-id').addEventListener('click', () => {
  if (!browser?.runtime?.id) return;
  if (!navigator.clipboard?.writeText) { nativeStatus('Copy the extension ID shown in the setup instructions.'); return; }
  void navigator.clipboard.writeText(browser.runtime.id).then(() => nativeStatus('Extension ID copied. Use it with install.ps1.')).catch(() => nativeStatus('Copy the extension ID shown in the setup instructions.'));
});
window.addEventListener('pagehide', () => { cancelLoad(); cleanup(); if (nativeSession) void native('stopSession').catch(() => {}); });
controls(); nativeControls();
if (new URLSearchParams(location.search).has('capture') && browser?.runtime?.id) {
  const signal = beginLoad('Opening your authorized tab capture…');
  void request({ type: 'capture-token' }).then(async result => {
    if (!isCurrent(signal)) return;
    if (typeof result.token !== 'string') throw new Error('Capture authorization expired. Start again from the extension.');
    const acquired = await navigator.mediaDevices.getUserMedia({ audio: false,
      video: { mandatory: { chromeMediaSource: 'tab', chromeMediaSourceId: result.token } } as MediaTrackConstraints });
    if (!isCurrent(signal)) { acquired.getTracks().forEach(track => track.stop()); return; }
    await attach(await loadStream(acquired, signal, 'Captured tab · separate viewer'), signal);
  }).catch(error => { if (isCurrent(signal)) status(error.message, true); }).finally(() => finishLoad(signal));
} else { void demo(); }
