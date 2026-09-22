import { APPROVALS } from '../../packages/nr/approvals.js';
import { NeuralClient } from '../../packages/nr/client.js';
import { approvals, inspectModel, verifyModel, processingSize, UPSTREAM, type Inspection } from '../../packages/nr/policy.js';
import { percentile } from '../../packages/contracts/index.js';
import { FrameLoop } from '../../packages/gpu-web/scheduler.js';
import type { Controls } from '../../packages/nr/engine.js';
const el = <T extends HTMLElement = HTMLElement>(id: string) => document.getElementById(id) as T;
const button = (id: string) => el<HTMLButtonElement>(id);
const trusted = approvals(APPROVALS);
let inspection: Inspection | null = null, client: NeuralClient | null = null, loop: FrameLoop | null = null;
let source: HTMLVideoElement | HTMLImageElement | null = null, sourceUrl: string | null = null, capture: MediaStream | null = null;
let active = false, prepared = false, epoch = 0, pending: Promise<number> | null = null, refreshing = false;
let loading: AbortController | null = null, sourceLoad = 0, modelLoad = 0;
let samples: number[] = [], completed = 0, elapsedMs = 0, size: { width: number; height: number } | null = null;
let probeClient: NeuralClient | null = null;
let hasOutput = false, settings: Controls = { tone: .5, structure: .5, split: .5 };
function status(message: string, error = false) { el('status').textContent = message; el('status').classList.toggle('error', error); }
function availability() {
  el('availability').textContent = trusted.length ? 'Approved model required' : 'Neural inference locked in this build';
  el('availability-detail').textContent = trusted.length ? 'Select a local model matching the reviewed manifest register. Source capture still requires consent.' :
    'The reviewed-model register is empty. The adapter is included, but no model is licensed or validated for activation in this release.';
}
function refresh() {
  button('prepare').disabled = !inspection?.approval || !source || !!loading || prepared;
  button('live').disabled = !prepared || !!loading;
  button('live').textContent = active ? 'Pause neural preview' : 'Start live preview';
  button('export').disabled = !hasOutput || !!pending || !!loading;
  button('stop').disabled = !source && !loading && !prepared && !probeClient;
  button('unload').disabled = !inspection && !loading;
  el<HTMLInputElement>('split').disabled = !hasOutput || !!loading;
  el<HTMLSelectElement>('edge').disabled = prepared || !!loading;
  el('gate-reason').textContent = !inspection?.approval ? 'No reviewed model matches this selection; inference cannot start.' :
    !source ? 'Open a source first.' : prepared ? 'Worker ready. Video processing is source-driven and one frame at a time.' : 'Prepare the backend, then start the live preview.';
  el('frames').textContent = String(completed);
  el('fps').textContent = completed && elapsedMs ? `${(completed / (elapsedMs / 1000)).toFixed(1)} fps` : '—';
  el('p95').textContent = samples.length ? `${percentile(samples, .95).toFixed(1)} ms` : '—';
  el('resolution').textContent = size ? `${size.width} × ${size.height}` : '—';
}
function showLoading(text: string, controller: AbortController) { loading = controller; el('loading').hidden = false; el('loading-text').textContent = text; refresh(); }
function endLoading(controller: AbortController) { if (loading === controller) { loading = null; el('loading').hidden = true; refresh(); } }
function stopInference() {
  epoch++; active = false; prepared = false; hasOutput = false; loop?.stop(); loop = null; client?.stop(); client = null;
  pending = null; refreshing = false; el('output-mount').querySelector('canvas')?.remove(); el('output-empty').hidden = false;
  el('output-state').textContent = 'NOT RUNNING'; refresh();
}
function releaseSource() {
  sourceLoad++;
  capture?.getTracks().forEach(t => t.stop()); capture = null;
  if (source instanceof HTMLVideoElement) { source.pause(); source.srcObject = null; source.removeAttribute('src'); source.load(); }
  source?.remove(); source = null; if (sourceUrl) URL.revokeObjectURL(sourceUrl); sourceUrl = null;
  el('empty').hidden = false; el('source-state').textContent = 'NO SOURCE'; el('source-info').textContent = 'No capture is active.';
}
function stopAll() {
  loading?.abort(); probeClient?.stop(); probeClient = null; loading = null; sourceLoad++; modelLoad++; el('loading').hidden = true; stopInference(); releaseSource(); refresh();
  status('Stopped. Capture tracks and the neural worker have been released.');
}
function sourceDimensions() {
  return source instanceof HTMLVideoElement ? [source.videoWidth, source.videoHeight] : source ? [source.naturalWidth, source.naturalHeight] : [0, 0];
}
function installSource(candidate: HTMLVideoElement | HTMLImageElement, label: string, url: string | null, stream: MediaStream | null) {
  const width = candidate instanceof HTMLVideoElement ? candidate.videoWidth : candidate.naturalWidth;
  const height = candidate instanceof HTMLVideoElement ? candidate.videoHeight : candidate.naturalHeight;
  if (!width || !height || width > 8192 || height > 8192 || width * height > 33554432) throw new Error('Source dimensions exceed this lab’s supported range.');
  stopInference(); releaseSource(); source = candidate; sourceUrl = url; capture = stream;
  el('empty').hidden = true; el('source-stage').append(candidate); el('source-state').textContent = stream ? 'CAPTURE ACTIVE' : 'LOCAL FILE';
  el('source-info').textContent = `${label} · ${width} × ${height}. Original source; not neural output.`;
  stream?.getVideoTracks()[0]?.addEventListener('ended', () => { if (capture === stream) stopAll(); }, { once: true });
  refresh(); status('Original source ready. An approved model is required for the neural preview.');
}
function decodedVideo(video: HTMLVideoElement, signal: AbortSignal): Promise<void> {
  return new Promise((resolve, reject) => {
    let timer: ReturnType<typeof setTimeout>;
    const finish = (error?: Error) => { clearTimeout(timer); video.removeEventListener('loadeddata', ready); video.removeEventListener('error', failed); signal.removeEventListener('abort', abort); error ? reject(error) : resolve(); };
    const ready = () => finish(), failed = () => finish(new Error('This browser cannot decode the selected video.')), abort = () => finish(new DOMException('Loading cancelled.', 'AbortError'));
    video.addEventListener('loadeddata', ready, { once: true }); video.addEventListener('error', failed, { once: true }); signal.addEventListener('abort', abort, { once: true });
    timer = setTimeout(() => finish(new Error('Video did not become ready.')), 15_000);
    if (signal.aborted) abort(); else if (video.readyState >= 2) ready();
  });
}
async function openFile(file: File) {
  if (!/^(image\/(png|jpeg|webp|avif)|video\/(mp4|webm|ogg))$/.test(file.type) || file.size > (file.type.startsWith('image/') ? 50 * 1024 ** 2 : 2 * 1024 ** 3)) {
    status('Choose a supported image below 50 MiB or video below 2 GiB. Current source preserved.', true); return;
  }
  loading?.abort(); const controller = new AbortController(), id = ++sourceLoad; showLoading('Decoding local media…', controller);
  const url = URL.createObjectURL(file); let committed = false;
  const candidate = file.type.startsWith('image/') ? new Image() : document.createElement('video');
  try {
    candidate.src = url;
    if (candidate instanceof HTMLImageElement) { candidate.alt = 'User-selected original image'; await candidate.decode(); }
    else { candidate.controls = true; candidate.preload = 'auto'; candidate.playsInline = true; await decodedVideo(candidate, controller.signal); }
    controller.signal.throwIfAborted(); if (id !== sourceLoad) return;
    const width = candidate instanceof HTMLVideoElement ? candidate.videoWidth : candidate.naturalWidth;
    const height = candidate instanceof HTMLVideoElement ? candidate.videoHeight : candidate.naturalHeight;
    if (!width || !height || width > 8192 || height > 8192 || width * height > 33554432) throw new Error('Source dimensions exceed this lab’s supported range.');
    installSource(candidate, file.name, url, null); committed = true;
  } catch (error) { if (!controller.signal.aborted && id === sourceLoad) status(error instanceof Error ? error.message : 'Media load failed.', true); }
  finally {
    if (!committed) { if (candidate instanceof HTMLVideoElement) { candidate.pause(); candidate.removeAttribute('src'); candidate.load(); } URL.revokeObjectURL(url); }
    endLoading(controller);
  }
}
async function shareWindow() {
  if (!navigator.mediaDevices?.getDisplayMedia) { status('Window sharing is unavailable. Use the file picker instead.', true); return; }
  loading?.abort(); const controller = new AbortController(), id = ++sourceLoad;
  const acquiring = navigator.mediaDevices.getDisplayMedia({ video: { frameRate: { max: 30 } }, audio: false,
    selfBrowserSurface: 'exclude', monitorTypeSurfaces: 'exclude', surfaceSwitching: 'exclude' } as DisplayMediaStreamOptions);
  showLoading('Choose another window. Do not select this viewer.', controller);
  let acquired: MediaStream | null = null, committed = false; const candidate = document.createElement('video');
  try {
    acquired = await acquiring; controller.signal.throwIfAborted();
    if (id !== sourceLoad) return;
    if (acquired.getVideoTracks()[0]?.getSettings().displaySurface === 'monitor') throw new Error('Monitor capture is not supported here because it can create feedback. Choose another window.');
    candidate.muted = true; candidate.playsInline = true; candidate.srcObject = acquired;
    await candidate.play(); await decodedVideo(candidate, controller.signal); controller.signal.throwIfAborted();
    if (id !== sourceLoad) return;
    installSource(candidate, 'Shared window · audio remains with the original application', null, acquired); committed = true;
  } catch (error) { if (!controller.signal.aborted && id === sourceLoad) status(error instanceof Error ? `${error.message} Current source preserved.` : 'Capture was not started.', true); }
  finally { if (!committed) { acquired?.getTracks().forEach(t => t.stop()); candidate.pause(); candidate.srcObject = null; } endLoading(controller); }
}
async function inspect(files: File[]) {
  loading?.abort(); const controller = new AbortController(), id = ++modelLoad; showLoading('Inspecting model metadata; no inference…', controller);
  try {
    const result = await inspectModel(files, APPROVALS, controller.signal); if (id !== modelLoad) return;
    stopInference(); inspection = result; el('model-info').hidden = false;
    el('model-hash').textContent = result.manifestSha256; el('model-size').textContent = `${(result.totalBytes / 1024 ** 2).toFixed(1)} MiB · ${result.manifest.stages.length} stages`;
    el('model-approval').textContent = result.approval ? result.approval.reviewId : 'Not approved in this build';
    el('model-status').textContent = result.approval ? 'Manifest matches the reviewed register. Stage contents will be SHA-256 verified before GPU loading.' :
      'Metadata inspected only. No weights were loaded into a GPU. A file selection does not grant model rights or establish correctness.';
    status(result.approval ? 'Model manifest approved; prepare when a source is ready.' : 'Model inspection complete. Neural inference remains locked.');
  } catch (error) { if (!controller.signal.aborted && id === modelLoad) { el('model-status').textContent = error instanceof Error ? error.message : 'Model inspection failed.'; status('Model rejected. Original playback remains available.', true); } }
  finally { endLoading(controller); refresh(); }
}
async function prepare() {
  if (!source || !inspection?.approval || loading) return;
  stopInference(); const generation = epoch, controller = new AbortController();
  const [width, height] = sourceDimensions();
  try {
    size = processingSize(width!, height!, Number(el<HTMLSelectElement>('edge').value));
    showLoading('Verifying local model data…', controller);
    const verified = await verifyModel(inspection, APPROVALS, controller.signal, (loaded, total) => {
      if (generation === epoch) el('loading-text').textContent = `Verifying model data ${(loaded / total * 100).toFixed(0)}%…`;
    });
    controller.signal.throwIfAborted(); if (generation !== epoch) return;
    const canvas = document.createElement('canvas'); canvas.setAttribute('aria-label', 'Experimental neural output'); canvas.hidden = true;
    if (!canvas.transferControlToOffscreen) throw new Error('This browser cannot transfer the preview into a worker.');
    const offscreen = canvas.transferControlToOffscreen();
    el('output-mount').append(canvas);
    const worker = new NeuralClient(text => { if (generation === epoch) el('loading-text').textContent = text; }); client = worker;
    controller.signal.addEventListener('abort', () => worker.stop('Preparation cancelled.'), { once: true });
    const transfers: Transferable[] = [offscreen, verified.manifestBytes, ...Array.from(verified.stages.values(), bytes => bytes.buffer as ArrayBuffer)];
    await worker.call('prepare', { canvas: offscreen, model: verified, ...size }, transfers, 120_000);
    controller.signal.throwIfAborted(); if (generation !== epoch) return;
    prepared = true; samples = []; completed = 0; elapsedMs = 0; el('output-state').textContent = 'PREPARED · NOT RUNNING';
    status('Neural graph prepared. Start explicitly; no temporal feedback or real-time guarantee.');
  } catch (error) { if (generation === epoch) { stopInference(); status(error instanceof Error ? error.message : 'Neural preparation failed.', true); } }
  finally { endLoading(controller); refresh(); }
}
async function operation(run: (worker: NeuralClient, generation: number) => Promise<number>): Promise<number> {
  if (!client || !prepared) return 0;
  if (pending) { refreshing = true; return pending; }
  const worker = client, generation = epoch;
  const task = run(worker, generation); pending = task; refresh();
  try { return await task; }
  catch (error) { if (generation === epoch) throw error; return 0; }
  finally {
    if (generation === epoch) {
      pending = null; refresh();
      if (refreshing) { refreshing = false; queueMicrotask(() => { if (generation === epoch) void draw().catch(failure); }); }
    }
  }
}
async function draw(): Promise<number> {
  if (!source) return 0;
  const current = source;
  return operation(async (worker, generation) => {
    const began = performance.now();
    const frame = current instanceof HTMLVideoElement && typeof VideoFrame !== 'undefined' ? new VideoFrame(current) : await createImageBitmap(current);
    if (generation !== epoch) { frame.close(); return 0; }
    try {
      const result = await worker.call('frame', { frame, controls: { ...settings } }, [frame]) as { ms: number };
      if (generation !== epoch) return 0;
      if (!Number.isFinite(result.ms) || result.ms < 0) throw new Error('Invalid timing returned by the worker.');
      completed++; elapsedMs += performance.now() - began; samples.push(result.ms); if (samples.length > 600) samples.shift();
      hasOutput = true; el('output-empty').hidden = true; el('output-mount').querySelector('canvas')!.hidden = false;
      el('output-state').textContent = active ? 'LIVE · INDEPENDENT FRAMES' : 'PAUSED · LAST RESULT';
      refresh(); return result.ms;
    } finally { frame.close(); }
  });
}
function failure(error: unknown) { stopInference(); status(error instanceof Error ? `${error.message} Original source retained.` : 'Neural output stopped. Original source retained.', true); }
function live() {
  if (!prepared || !source) return;
  if (active) { active = false; loop?.stop(); loop = null; el('output-state').textContent = 'PAUSED · LAST RESULT'; status('Neural preview paused. Original source controls remain available.'); refresh(); return; }
  active = true;
  if (source instanceof HTMLVideoElement) {
    loop = new FrameLoop(source, draw, failure); loop.start();
    if (source.paused) void source.play().catch(() => status('Use Play on the original video to start its timeline.'));
  } else { const generation = epoch; void draw().then(() => { if (generation === epoch) { active = false; el('output-state').textContent = 'STILL FRAME'; refresh(); } }).catch(failure); }
  status('Neural processing started. Throughput is measured below; each frame has no temporal history.'); refresh();
}
function save(blob: Blob, name: string) { const url = URL.createObjectURL(blob), a = document.createElement('a'); a.href = url; a.download = name; a.click(); setTimeout(() => URL.revokeObjectURL(url), 2000); }
button('probe').onclick = () => {
  button('probe').disabled = true; const worker = new NeuralClient(); probeClient = worker; refresh();
  void worker.call('probe').then(result => { const value = result as { available: boolean; reason: string; workgroupKiB?: number }; el('gpu-status').textContent = `${value.available ? 'Candidate adapter. ' : 'Unavailable. '}${value.reason}${value.workgroupKiB ? ` Workgroup storage: ${value.workgroupKiB} KiB.` : ''}`; })
    .catch(error => { el('gpu-status').textContent = error.message; }).finally(() => { worker.stop(); if (probeClient === worker) probeClient = null; button('probe').disabled = false; refresh(); });
};
el<HTMLInputElement>('media-file').onchange = event => { const target = event.target as HTMLInputElement; const file = target.files?.[0]; target.value = ''; if (file) void openFile(file); };
el<HTMLInputElement>('model-files').onchange = event => { const target = event.target as HTMLInputElement; const files = Array.from(target.files || []); target.value = ''; if (files.length) void inspect(files); };
button('share').onclick = () => { void shareWindow(); }; button('stop').onclick = stopAll; button('prepare').onclick = () => { void prepare(); }; button('live').onclick = live;
button('cancel').onclick = () => { const previous = loading; previous?.abort(); sourceLoad++; modelLoad++; if (previous) endLoading(previous); status('Operation cancelled. Existing source retained.'); };
button('unload').onclick = () => { loading?.abort(); loading = null; modelLoad++; el('loading').hidden = true; stopInference(); inspection = null; el('model-info').hidden = true; el('model-status').textContent = 'Model selection released. No model data is stored.'; refresh(); };
for (const key of ['tone', 'structure', 'split'] as const) el<HTMLInputElement>(key).oninput = () => {
  settings = { ...settings, [key]: Number(el<HTMLInputElement>(key).value) / 100 };
  el('tone-value').textContent = `${Math.round(settings.tone * 100)}%`; el('structure-value').textContent = `${Math.round(settings.structure * 100)}%`;
  el('split-value').textContent = `${Math.round(settings.split * 100)} / ${100 - Math.round(settings.split * 100)}`;
  if (!prepared || !hasOutput) return;
  if (pending) { refreshing = true; return; }
  if (key === 'split') { void operation(async worker => { await worker.call('present', { controls: { ...settings } }); return 0; }).catch(failure); }
  else void draw().catch(failure);
};
button('export').onclick = () => {
  if (!client || !hasOutput || pending) return;
  active = false; loop?.stop(); loop = null; el('output-state').textContent = 'PAUSED · LAST RESULT';
  void operation(async (worker, generation) => {
    const result = await worker.call('export');
    if (generation === epoch && result instanceof Blob) { save(result, 'shiny-neural-comparison.png'); status('Current comparison exported locally. No video recording.'); }
    return 0;
  }).catch(error => status(error instanceof Error ? error.message : 'Export failed.', true));
};
button('report').onclick = () => save(new Blob([JSON.stringify({ schema: 1, version: '0.3.0', upstream: UPSTREAM,
  backend: prepared ? 'opendlss-nr-webgpu-experimental' : 'none', manifestSha256: inspection?.manifestSha256 ?? null,
  approved: !!inspection?.approval, processingSize: size, frames: completed, p95CompletionMs: samples.length ? percentile(samples, .95) : null,
  temporalMode: 'independent-frames', physicalGpuCertified: false, modelInferenceValidatedHere: false, sourceNamesIncluded: false,
  busyThroughputFps: elapsedMs ? completed * 1000 / elapsedMs : null,
  scope: 'Worker frame completion samples exclude source-frame creation. Busy throughput includes creation/transfer; it is not displayed FPS or capture-to-display latency.', settings,
}, null, 2)], { type: 'application/json' }), 'shiny-neural-diagnostics.json');
document.addEventListener('keydown', event => { if (event.key === 'Escape') stopAll(); });
window.addEventListener('pagehide', stopAll);
availability(); refresh();
