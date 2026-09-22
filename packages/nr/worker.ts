import { NeuralEngine, controls, probe, type Controls } from './engine.js';
import type { VerifiedModel } from './policy.js';
const scope = globalThis as unknown as { onmessage: ((event: MessageEvent) => void) | null; postMessage(message: unknown): void; };
let engine: NeuralEngine | null = null, busy = false;
let output: OffscreenCanvas | null = null;
const fail = (id: number, error: unknown) => scope.postMessage({ id, ok: false, error: error instanceof Error ? error.message : 'Neural worker failed.' });
scope.onmessage = event => {
  const data = event.data as { id: number; type: string; canvas?: OffscreenCanvas; model?: VerifiedModel; width: number; height: number; frame?: VideoFrame | ImageBitmap; controls: Controls };
  if (!data || !Number.isSafeInteger(data.id) || data.id < 1) { data?.frame?.close?.(); return; }
  if (busy) { data.frame?.close?.(); fail(data.id, new Error('A frame is already in flight.')); return; }
  busy = true;
  void (async () => {
    try {
      let result: unknown;
      if (data.type === 'probe') result = await probe();
      else if (data.type === 'prepare') {
        if (!data.canvas || !data.model || engine) throw new Error('Invalid preparation request.');
        output = data.canvas;
        engine = await NeuralEngine.create(data.canvas, data.model, data.width, data.height,
          text => scope.postMessage({ id: data.id, progress: text })); result = engine.info;
      } else if (data.type === 'frame') {
        if (!engine || !data.frame) throw new Error('The approved neural backend is not prepared.');
        const ms = await engine.process(data.frame, controls(data.controls)); result = { ms };
      } else if (data.type === 'present') {
        if (!engine) throw new Error('Backend is not prepared.'); await engine.present(controls(data.controls)); result = {};
      } else if (data.type === 'export') {
        if (!engine || !output) throw new Error('No neural output is available.');
        result = await output.convertToBlob({ type: 'image/png' });
      } else throw new Error('Unknown neural command.');
      scope.postMessage({ id: data.id, ok: true, result });
    } catch (error) { fail(data.id, error); }
    finally { data.frame?.close?.(); busy = false; }
  })();
};
