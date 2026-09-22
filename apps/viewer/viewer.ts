import { DEFAULTS, PRESETS, settings, percentile, record, type Settings, type Command } from '../../packages/contracts/index.js';
import { browser, request } from '../../packages/contracts/browser.js';
import { createRenderer, dimensions, type Renderer, type Source } from '../../packages/gpu-web/renderer.js';
import { FrameLoop } from '../../packages/gpu-web/scheduler.js';
function element<T extends HTMLElement>(id: string): T {
  const found = document.getElementById(id);
  if (!found) throw new Error(`Missing application element: ${id}`);
  return found as T;
}
const mount = element('canvas-mount'), stage = element('stage');
const video = element<HTMLVideoElement>('source-video');
const anchor = document.createComment('original playback controls'); video.before(anchor);
const sliders = { strength: element<HTMLInputElement>('strength'), denoise: element<HTMLInputElement>('denoise'),
  split: element<HTMLInputElement>('split'), scale: element<HTMLSelectElement>('scale') };
let config: Settings = { ...DEFAULTS };
let renderer: Renderer | null = null, canvas: HTMLCanvasElement | null = null, source: Source | null = null;
let loop: FrameLoop | null = null, stream: MediaStream | null = null, blobUrl: string | null = null;
let generation = 0, inFlight: Promise<number> | null = null, dirty = false, measuring = false;
let samples: number[] = [], nativeSession: string | null = null;
function status(message: string, error = false) {
  element('status').textContent = message; element('status').classList.toggle('error', error);
}
function controls() {
  sliders.strength.value = String(Math.round(config.strength * 100));
  sliders.denoise.value = String(Math.round(config.denoise * 100));
  sliders.split.value = String(Math.round(config.split * 100)); sliders.scale.value = String(config.scale);
  element('strength-value').textContent = `${Math.round(config.strength * 100)}%`;
  element('denoise-value').textContent = `${Math.round(config.denoise * 100)}%`;
  element('split-value').textContent = `${Math.round(config.split * 100)} / ${100 - Math.round(config.split * 100)}`;
  element('comparison-line').style.left = `${config.split * 100}%`;
  element('comparison-line').hidden = !config.enabled || config.split === 0 || config.split === 1 || !renderer;
  element('original-tag').hidden = config.enabled && config.split === 0;
  element('enhanced-tag').hidden = !config.enabled || config.split === 1 || !renderer;
  element('bypass').setAttribute('aria-pressed', String(!config.enabled));
  element('bypass').textContent = config.enabled ? 'Original only' : 'Show enhancement';
}
function diagnostics() {
  element('metric-p95').textContent = samples.length ? `${percentile(samples, 0.95).toFixed(1)} ms` : '—';
  element('metric-count').textContent = String(samples.length);
  element('metric-backend').textContent = renderer?.label.replace('Spatial · ', '') ?? 'Original';
}
function cleanup() {
  generation++; measuring = false; loop?.stop(); loop = null;
  stream?.getTracks().forEach(track => track.stop()); stream = null;
  video.pause(); video.muted = false; video.srcObject = null; video.removeAttribute('src'); video.load(); video.hidden = true; anchor.after(video);
  if (blobUrl) URL.revokeObjectURL(blobUrl); blobUrl = null;
  const old = renderer; renderer = null; source = null; canvas = null;
  if (inFlight) void inFlight.catch(() => {}).finally(() => old?.release()); else old?.release();
  inFlight = null; dirty = false; mount.replaceChildren(); samples = []; diagnostics();
  element<HTMLButtonElement>('stop').disabled = true;
  element<HTMLButtonElement>('export').disabled = true;
  element<HTMLButtonElement>('benchmark').disabled = false;
}
function fallback(error: unknown) {
  loop?.stop(); loop = null;
  const previous = renderer; renderer = null;
  if (inFlight) void inFlight.catch(() => {}).finally(() => previous?.release()); else previous?.release();
  if (source instanceof HTMLVideoElement || source instanceof HTMLImageElement || source instanceof HTMLCanvasElement) {
    mount.replaceChildren(source); source.hidden = false;
  } else { mount.replaceChildren(); element('empty').hidden = false; element('empty-text').textContent = 'No GPU backend is available. Local video playback remains available.'; }
  element('backend-pill').textContent = 'Original · enhancement unavailable';
  element<HTMLButtonElement>('export').disabled = true;
  controls(); diagnostics();
  status(error instanceof Error ? error.message : 'This source cannot be processed. The original was preserved.', true);
}
async function draw(): Promise<number> {
  if (!renderer || !source) return 0;
  if (inFlight) { dirty = true; return inFlight; }
  const epoch = generation;
  const current = renderer;
  const task = current.render(source, config); inFlight = task;
  try {
    const ms = await task;
    if (epoch !== generation) return ms;
    samples.push(ms); if (samples.length > 600) samples.shift(); diagnostics();
    if (canvas) element('dimensions').textContent = `${canvas.width} × ${canvas.height}`;
    return ms;
  } finally {
    if (epoch === generation) {
      inFlight = null;
      if (dirty && !measuring && (video.paused || source !== video)) {
        dirty = false; queueMicrotask(() => { void draw().catch(fallback); });
      }
    }
  }
}
async function attach(input: Source, label: string, epoch: number) {
  const [width, height] = dimensions(input);
  if (width < 1 || height < 1 || width * height > 33_554_432) throw new Error('Media must have valid dimensions below 33 megapixels.');
  if (epoch !== generation) return;
  source = input; element('source-name').textContent = label;
  stage.style.aspectRatio = String(width / height);
  stage.setAttribute('aria-busy', 'true'); element('empty').hidden = true;
  element<HTMLButtonElement>('stop').disabled = false;
  try {
    const detached = document.createElement('div');
    const result = await createRenderer(detached);
    if (epoch !== generation) { result.renderer.release(); return; }
    renderer = result.renderer; canvas = result.canvas; mount.replaceChildren(canvas);
    element('backend-pill').textContent = renderer.label;
    controls(); await draw();
    if (epoch !== generation) return;
    if (input === video) { loop = new FrameLoop(video, draw, fallback); loop.start(); }
    element<HTMLButtonElement>('export').disabled = false;
    status('Local processing active. Nothing is uploaded. B toggles the original; Escape stops the session.');
  } catch (error) { if (epoch === generation) fallback(error); }
  finally { if (epoch === generation) stage.setAttribute('aria-busy', 'false'); }
}
function fixture() {
  const c = document.createElement('canvas'); c.width = 960; c.height = 540;
  const ctx = c.getContext('2d')!;
  const sky = ctx.createLinearGradient(0, 0, 0, 540); sky.addColorStop(0, '#254453'); sky.addColorStop(1, '#16292c');
  ctx.fillStyle = sky; ctx.fillRect(0, 0, 960, 540);
  ctx.fillStyle = '#9fd8ba'; ctx.beginPath(); ctx.arc(724, 144, 58, 0, 2 * Math.PI); ctx.fill();
  for (let layer = 0; layer < 4; layer++) {
    ctx.fillStyle = ['#315965', '#29454c', '#1b383f', '#102a2e'][layer]!;
    ctx.beginPath(); ctx.moveTo(0, 330 + layer * 25);
    for (let x = 0; x <= 960; x += 30) ctx.lineTo(x, 260 + layer * 47 + Math.sin(x / 71 + layer) * 53 + Math.cos(x / 33) * 12);
    ctx.lineTo(960, 540); ctx.lineTo(0, 540); ctx.fill();
  }
  ctx.globalAlpha = 0.6; ctx.strokeStyle = '#83b3a8'; ctx.lineWidth = 1;
  for (let i = 0; i < 19; i++) { ctx.beginPath(); ctx.moveTo(500 - i * 24, 350 + i * 10); ctx.lineTo(905 - i * 8, 350 + i * 10); ctx.stroke(); }
  ctx.globalAlpha = 1; ctx.fillStyle = '#d5e6dd'; ctx.font = '11px monospace'; ctx.fillText('SYNTHETIC DETAIL STUDY / 001', 38, 43);
  ctx.font = '38px sans-serif'; ctx.fillText('Keep the character.', 38, 445);
  ctx.fillStyle = '#98b9b1'; ctx.font = '15px sans-serif'; ctx.fillText('Refine the detail.', 40, 478);
  // Deterministic generated fixture; not a claimed photographic restoration example.
  let seed = 71; const image = ctx.getImageData(0, 0, c.width, c.height);
  for (let i = 0; i < image.data.length; i += 4) {
    seed = (Math.imul(seed, 1664525) + 1013904223) >>> 0;
    const noise = ((seed >>> 24) / 255 - 0.5) * 16;
    for (let channel = 0; channel < 3; channel++) image.data[i + channel] = image.data[i + channel]! + noise;
  }
  ctx.putImageData(image, 0, 0); return c;
}
async function demo() { cleanup(); config = { ...DEFAULTS }; controls(); await attach(fixture(), 'Synthetic detail study · local demo', generation); }
function metadata(epoch: number): Promise<void> {
  return new Promise((resolve, reject) => {
    const timer = setTimeout(() => finish(new Error('Media did not load. Check its format and codec.')), 15_000);
    const loaded = () => finish(); const failed = () => finish(new Error('This browser cannot decode the selected video.'));
    const finish = (error?: Error) => {
      clearTimeout(timer); video.removeEventListener('loadedmetadata', loaded); video.removeEventListener('error', failed);
      if (epoch !== generation) reject(new Error('Source changed.')); else if (error) reject(error); else resolve();
    };
    video.addEventListener('loadedmetadata', loaded, { once: true }); video.addEventListener('error', failed, { once: true });
    if (video.readyState >= 1) finish();
  });
}
async function openFile(file: File) {
  if (!/^(image\/(png|jpeg|webp|avif)|video\/(mp4|webm|ogg))$/.test(file.type)) {
    status('Choose a PNG, JPEG, WebP, AVIF, MP4, WebM or Ogg file.', true); return;
  }
  if (file.size > 2 * 1024 ** 3 || (file.type.startsWith('image/') && file.size > 50 * 1024 ** 2)) {
    status('Choose an image below 50 MiB or a video below 2 GiB.', true); return;
  }
  cleanup(); const epoch = generation; blobUrl = URL.createObjectURL(file);
  try {
    if (file.type.startsWith('image/')) {
      const img = new Image(); img.src = blobUrl; await img.decode();
      if (epoch === generation) await attach(img, file.name, epoch);
    } else {
      video.src = blobUrl; video.hidden = false; await metadata(epoch);
      if (epoch !== generation) return;
      try { await video.play(); } catch { /* Native controls remain available if autoplay was refused. */ }
      await attach(video, file.name, epoch);
    }
  } catch (error) { if (epoch === generation) { status(error instanceof Error ? error.message : 'Media load failed.', true); stage.setAttribute('aria-busy', 'false'); } }
}
async function attachStream(acquired: MediaStream, epoch: number, label: string, playAudio: boolean) {
  if (epoch !== generation) { acquired.getTracks().forEach(track => track.stop()); return; }
  stream = acquired; video.srcObject = stream; video.muted = !playAudio; video.hidden = false;
  const track = stream.getVideoTracks()[0];
  if (!track) throw new Error('Capture supplied no video track.');
  track.addEventListener('ended', () => { if (epoch === generation) stop(); }, { once: true });
  await metadata(epoch); await video.play(); await attach(video, label, epoch);
}
async function shareWindow() {
  if (!navigator.mediaDevices?.getDisplayMedia) { status('Screen sharing is unavailable in this browser. Use Chrome or Edge on a secure page.', true); return; }
  // Invoke the picker before any awaited work to preserve the user activation.
  const pending = navigator.mediaDevices.getDisplayMedia({
    video: { frameRate: { max: 30 } }, audio: false,
    selfBrowserSurface: 'exclude', monitorTypeSurfaces: 'exclude', surfaceSwitching: 'exclude',
  } as DisplayMediaStreamOptions);
  cleanup(); const epoch = generation;
  try {
    const acquired = await pending;
    if (acquired.getVideoTracks()[0]?.getSettings().displaySurface === 'monitor') {
      acquired.getTracks().forEach(track => track.stop()); throw new Error('Monitor capture is disabled in the browser viewer to prevent feedback. Use the native companion.');
    }
    await attachStream(acquired, epoch, 'Shared window · capture active', false);
  } catch (error) { if (epoch === generation) { cleanup(); status(error instanceof Error ? error.message : 'Capture was not started.', true); } }
}
function stop() { cleanup(); element('empty').hidden = false; element('empty-text').textContent = 'Capture stopped and GPU resources released. Open a file or restart the demo.'; element('source-name').textContent = 'No active source'; stage.setAttribute('aria-busy', 'false'); controls(); status('Session stopped. No capture is active.'); }
function save(blob: Blob, name: string) {
  const url = URL.createObjectURL(blob); const a = document.createElement('a'); a.href = url; a.download = name; a.click();
  setTimeout(() => URL.revokeObjectURL(url), 2000);
}
async function exportFrame() {
  if (!renderer || !source || !canvas || measuring) return;
  const epoch = generation; const current = renderer; const currentSource = source; const output = canvas;
  const wasLoop = loop; wasLoop?.stop(); measuring = true;
  try {
    if (inFlight) await inFlight;
    if (epoch !== generation) return;
    await current.render(currentSource, { ...config, split: 0, enabled: true });
    const blob = await new Promise<Blob | null>(resolve => output.toBlob(resolve, 'image/png'));
    if (!blob) throw new Error('PNG export was unavailable.');
    if (epoch === generation) { save(blob, 'shiny-engine-enhanced.png'); status('Enhanced PNG exported locally. The source was not modified.'); }
  } catch (error) { status(error instanceof Error ? error.message : 'Export failed.', true); }
  finally { if (epoch === generation) { measuring = false; await draw().catch(fallback); wasLoop?.start(); } }
}
async function benchmark() {
  if (!source || !renderer || measuring) return;
  if (source === video) { status('Pause on a still image or use the demo for the 60-frame diagnostic.', true); return; }
  const epoch = generation; measuring = true; samples = [];
  const button = element<HTMLButtonElement>('benchmark'); button.disabled = true;
  try {
    for (let i = 0; i < 60 && epoch === generation; i++) { await draw(); status(`Measuring local still-frame completion ${i + 1}/60…`); }
    if (epoch === generation) status(`60-frame diagnostic complete. p95 ${percentile(samples, 0.95).toFixed(1)} ms on this session; not a real-time certification.`);
  } catch (error) { if (epoch === generation) fallback(error); }
  finally { if (epoch === generation) { measuring = false; button.disabled = false; } }
}
async function native(command: Command) {
  const payload = { v: 1, id: crypto.randomUUID(), command, ...(nativeSession ? { session: nativeSession } : {}) };
  const response = await request({ type: 'native', payload });
  if (!record(response.result)) throw new Error('Invalid companion response.');
  if (response.result.ok !== true) throw new Error(String(response.result.error ?? 'Native operation failed.'));
  return response.result;
}
function nativeStatus(text: string) { element('native-status').textContent = text; }
element('connect-native').addEventListener('click', () => {
  void (async () => {
    if (!browser?.runtime?.id) throw new Error('Open Media Lab from the installed extension to use native messaging.');
    if (!await browser.permissions.request({ permissions: ['nativeMessaging'] })) throw new Error('Native permission was not granted.');
    const result = await native('getCapabilities');
    element<HTMLButtonElement>('pick-native').disabled = false;
    nativeStatus(`Companion connected. ${String(result.description ?? 'Choose a source to begin.')}`);
  })().catch(error => nativeStatus(error.message));
});
element('pick-native').addEventListener('click', () => {
  element<HTMLButtonElement>('pick-native').disabled = true;
  void (async () => {
    const result = await native('selectSource');
    if (typeof result.session !== 'string') throw new Error('No source selected.');
    nativeSession = result.session; await native('startSession');
    element<HTMLButtonElement>('stop-native').disabled = false;
    nativeStatus('Native capture active in a separate preview. Use Stop native or Ctrl+Shift+F10 to stop.');
  })().catch(error => nativeStatus(error.message)).finally(() => { element<HTMLButtonElement>('pick-native').disabled = false; });
});
element('stop-native').addEventListener('click', () => {
  void native('stopSession').then(() => { nativeSession = null; element<HTMLButtonElement>('stop-native').disabled = true; nativeStatus('Native capture stopped.'); }).catch(error => nativeStatus(error.message));
});
for (const [key, input] of Object.entries(sliders)) input.addEventListener('input', () => {
  config = settings({ ...config, [key]: Number(input.value) / (key === 'scale' ? 1 : 100) });
  controls(); if (!measuring) void draw().catch(fallback);
  try { localStorage.setItem('shiny-settings-v1', JSON.stringify(config)); } catch { /* Storage is optional. */ }
});
for (const button of document.querySelectorAll<HTMLButtonElement>('[data-preset]')) button.addEventListener('click', () => {
  const key = button.dataset.preset as keyof typeof PRESETS; config = { ...config, ...PRESETS[key], enabled: true }; controls();
  document.querySelectorAll('[data-preset]').forEach(item => item.setAttribute('aria-pressed', String(item === button)));
  if (!measuring) void draw().catch(fallback);
});
element('bypass').addEventListener('click', () => { config.enabled = !config.enabled; controls(); if (!measuring) void draw().catch(fallback); });
element<HTMLInputElement>('file').addEventListener('change', event => {
  const input = event.target as HTMLInputElement; const file = input.files?.[0]; input.value = ''; if (file) void openFile(file);
});
element('share').addEventListener('click', () => { void shareWindow(); });
element('sample').addEventListener('click', () => { void demo(); });
element('stop').addEventListener('click', stop);
element('export').addEventListener('click', () => { void exportFrame(); });
element('benchmark').addEventListener('click', () => { void benchmark(); });
element('report').addEventListener('click', () => {
  save(new Blob([JSON.stringify({ schema: 1, product: 'shiny-engine', version: '0.1.0',
    timestamp: new Date().toISOString(), backend: renderer?.label ?? 'original', settings: config,
    scope: 'CPU submission to GPU completion; includes browser scheduling. No capture/display latency certification.',
    samples: samples.length, medianMs: percentile(samples, 0.5), p95Ms: percentile(samples, 0.95), p99Ms: percentile(samples, 0.99),
    output: canvas ? { width: canvas.width, height: canvas.height } : null,
    certification: 'unverified', sourceNamesIncluded: false,
  }, null, 2)], { type: 'application/json' }), 'shiny-engine-diagnostics.json');
});
video.addEventListener('seeked', () => { if (!measuring && source === video) void draw().catch(fallback); });
video.addEventListener('loadeddata', () => { if (!measuring && source === video) void draw().catch(fallback); });
document.addEventListener('keydown', event => {
  if (event.key === 'Escape') { stop(); return; }
  if (event.key.toLowerCase() === 'b' && !event.ctrlKey && !event.metaKey && !(event.target instanceof HTMLInputElement) && !(event.target instanceof HTMLSelectElement)) element('bypass').click();
});
document.addEventListener('dragover', event => { event.preventDefault(); });
document.addEventListener('drop', event => { event.preventDefault(); const file = event.dataTransfer?.files[0]; if (file) void openFile(file); });
window.addEventListener('pagehide', cleanup);
try { const stored = localStorage.getItem('shiny-settings-v1'); if (stored) config = settings(JSON.parse(stored)); } catch { /* Ignore corrupt or unavailable settings. */ }
controls();
if (new URLSearchParams(location.search).has('capture') && browser?.runtime?.id) {
  cleanup(); const epoch = generation;
  void request({ type: 'capture-token' }).then(async result => {
    if (typeof result.token !== 'string') throw new Error('Capture token unavailable.');
    const acquired = await navigator.mediaDevices.getUserMedia({
      audio: false,
      video: { mandatory: { chromeMediaSource: 'tab', chromeMediaSourceId: result.token }, } as MediaTrackConstraints,
    });
    await attachStream(acquired, epoch, 'Captured tab · separate viewer', false);
  }).catch(error => { cleanup(); status(error.message, true); });
} else { void attach(fixture(), 'Synthetic detail study · local demo', generation); }
