/** Dedicated worker boundary. One request in flight; stop terminates the worker/context. */
export class NeuralClient {
  private worker: Worker;
  private sequence = 0;
  private pending: { id: number; resolve(value: unknown): void; reject(error: Error): void; timer: ReturnType<typeof setTimeout> } | null = null;
  private stopped = false;
  constructor(private progress: (text: string) => void = () => {}) {
    this.worker = new Worker(new URL('./worker.js', import.meta.url), { type: 'module', name: 'shiny-neural-rendering' });
    this.worker.onmessage = event => {
      const data = event.data as { id: number; progress?: string; ok?: boolean; result?: unknown; error?: string };
      if (!this.pending || data.id !== this.pending.id) return;
      if (typeof data.progress === 'string') { this.progress(data.progress); return; }
      const pending = this.pending; this.pending = null; clearTimeout(pending.timer);
      if (data.ok === true) pending.resolve(data.result); else pending.reject(new Error(data.error || 'Worker request failed.'));
    };
    this.worker.onerror = () => this.stop('The neural worker failed. Original playback remains available.');
  }
  call(type: string, payload: object = {}, transfers: Transferable[] = [], timeout = 30_000): Promise<unknown> {
    if (this.stopped) return Promise.reject(new Error('Neural session stopped.'));
    if (this.pending) return Promise.reject(new Error('Neural worker is busy.'));
    const id = ++this.sequence;
    return new Promise((resolve, reject) => {
      const timer = setTimeout(() => this.stop('Neural processing timed out; the worker was released.'), timeout);
      this.pending = { id, resolve, reject, timer };
      try { this.worker.postMessage({ ...payload, type, id }, transfers); }
      catch (error) { clearTimeout(timer); this.pending = null; reject(error); }
    });
  }
  stop(message = 'Neural session stopped.') {
    if (this.stopped) return; this.stopped = true; this.worker.terminate();
    if (this.pending) { clearTimeout(this.pending.timer); this.pending.reject(new Error(message)); this.pending = null; }
  }
}
