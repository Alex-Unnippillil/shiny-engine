import type { Source } from '../../packages/gpu-web/renderer.js';

export interface Media {
  source: Source;
  label: string;
  kind: 'image' | 'video' | 'capture' | 'demo';
  audio: boolean;
  dispose(): void;
}
const TYPES = new Set(['image/png', 'image/jpeg', 'image/webp', 'image/avif', 'video/mp4', 'video/webm', 'video/ogg']);
const EXTENSIONS: Record<string, string> = { png: 'image/png', jpg: 'image/jpeg', jpeg: 'image/jpeg', webp: 'image/webp', avif: 'image/avif', mp4: 'video/mp4', webm: 'video/webm', ogg: 'video/ogg', ogv: 'video/ogg' };
export function fileType(file: Pick<File, 'type' | 'name' | 'size'>): string {
  // Some OS drag/drop paths omit MIME. Do not replace an explicitly unsupported type.
  const type = file.type || EXTENSIONS[file.name.split('.').pop()?.toLowerCase() ?? ''];
  if (!type || !TYPES.has(type)) throw new Error('Choose a PNG, JPEG, WebP, AVIF, MP4, WebM or Ogg file. Your current media is unchanged.');
  if (!Number.isFinite(file.size) || file.size <= 0) throw new Error('This file is empty. Choose a different file.');
  if (file.size > (type.startsWith('image/') ? 50 * 1024 ** 2 : 2 * 1024 ** 3)) throw new Error('Choose an image below 50 MiB or a video below 2 GiB. Your current media is unchanged.');
  return type;
}
function abortError() { return new DOMException('Source loading cancelled.', 'AbortError'); }
export function validDimensions(width: number, height: number) {
  if (!Number.isFinite(width) || !Number.isFinite(height) || width < 1 || height < 1 || width * height > 33_554_432) {
    throw new Error('Media must have valid dimensions below 33 megapixels. Your current media is unchanged.');
  }
}
export function readyVideo(video: HTMLVideoElement, signal: AbortSignal): Promise<void> {
  return new Promise((resolve, reject) => {
    let complete = false;
    const finish = (error?: Error) => {
      if (complete) return; complete = true; clearTimeout(timer);
      video.removeEventListener('loadeddata', ready); video.removeEventListener('error', failed); signal.removeEventListener('abort', cancelled);
      if (error) reject(error); else resolve();
    };
    const ready = () => { if (video.readyState >= 2) finish(); };
    const failed = () => finish(new Error('This browser cannot decode that video. Try an MP4 (H.264) or WebM file. Your current media is unchanged.'));
    const cancelled = () => finish(abortError());
    const timer = setTimeout(() => finish(new Error('Video loading timed out. Try a smaller file or another codec. Your current media is unchanged.')), 15_000);
    video.addEventListener('loadeddata', ready); video.addEventListener('error', failed); signal.addEventListener('abort', cancelled, { once: true });
    if (signal.aborted) cancelled(); else ready();
  });
}
function releaseVideo(video: HTMLVideoElement) {
  video.pause(); video.srcObject = null; video.removeAttribute('src'); video.load(); video.remove();
}
export async function loadFile(file: File, signal: AbortSignal): Promise<Media> {
  const type = fileType(file);
  if (signal.aborted) throw abortError();
  const url = URL.createObjectURL(file);
  let video: HTMLVideoElement | null = null;
  let image: HTMLImageElement | null = null;
  let disposed = false;
  const dispose = () => {
    if (disposed) return; disposed = true;
    if (video) releaseVideo(video);
    if (image) { image.removeAttribute('src'); image.remove(); }
    URL.revokeObjectURL(url);
  };
  signal.addEventListener('abort', dispose, { once: true });
  try {
    let source: Source;
    if (type.startsWith('image/')) {
      image = new Image(); image.alt = 'Original selected image'; image.src = url;
      await image.decode(); source = image; validDimensions(image.naturalWidth, image.naturalHeight);
    } else {
      video = document.createElement('video'); video.controls = true; video.playsInline = true; video.preload = 'auto'; video.src = url;
      await readyVideo(video, signal); source = video; validDimensions(video.videoWidth, video.videoHeight);
    }
    if (signal.aborted) throw abortError();
    return { source, label: file.name, kind: type.startsWith('image/') ? 'image' : 'video', audio: !!video, dispose };
  } catch (error) {
    dispose();
    if (signal.aborted) throw abortError();
    if (image) throw new Error('That image could not be opened. It may be damaged or exceed the 33 megapixel limit. Your current media is unchanged.');
    throw error;
  } finally { signal.removeEventListener('abort', dispose); }
}
export async function loadStream(acquired: MediaStream, signal: AbortSignal, label: string): Promise<Media> {
  const video = document.createElement('video'); video.controls = true; video.playsInline = true; video.muted = true;
  let disposed = false;
  const dispose = () => { if (disposed) return; disposed = true; acquired.getTracks().forEach(track => track.stop()); releaseVideo(video); };
  signal.addEventListener('abort', dispose, { once: true });
  try {
    if (signal.aborted) throw abortError();
    if (!acquired.getVideoTracks().some(t => t.readyState === 'live')) throw new Error('Capture supplied no live video track.');
    video.srcObject = acquired;
    const ready = readyVideo(video, signal);
    // Muted playback is needed for the first frame of some captured streams.
    await Promise.all([video.play(), ready]);
    if (signal.aborted) throw abortError();
    validDimensions(video.videoWidth, video.videoHeight);
    return { source: video, label, kind: 'capture', audio: false, dispose };
  } catch (error) { dispose(); throw error; }
  finally { signal.removeEventListener('abort', dispose); }
}
