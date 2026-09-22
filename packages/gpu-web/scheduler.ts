/** Source-driven, serial execution. Paused/hidden videos do not spin a render loop. */
export class FrameLoop {
  private active = false;
  private pending = false;
  private handle = 0;
  private videoCallback = false;
  private last = -Infinity;
  private epoch = 0;
  private fps = 30;
  private events: AbortController | null = null;
  private refresh = false;
  constructor(private video: HTMLVideoElement, private render: () => Promise<number>,
    private failure: (error: unknown) => void, private sample: (ms: number) => void = () => {}) {}
  start() {
    if (this.active) return;
    this.active = true; this.last = -Infinity;
    this.events = new AbortController();
    const options = { signal: this.events.signal };
    const refresh = () => { this.refresh = true; this.cancel(); this.schedule(); };
    this.video.addEventListener('play', refresh, options);
    this.video.addEventListener('seeked', refresh, options);
    this.video.addEventListener('loadeddata', refresh, options);
    this.video.addEventListener('pause', refresh, options);
    document.addEventListener('visibilitychange', refresh, options);
    this.refresh = true; this.schedule();
  }
  stop() {
    this.active = false; this.epoch++; this.cancel(); this.events?.abort(); this.events = null;
  }
  private cancel() {
    if (!this.handle) return;
    if (this.videoCallback) this.video.cancelVideoFrameCallback(this.handle);
    else cancelAnimationFrame(this.handle);
    this.handle = 0;
  }
  private schedule() {
    if (!this.active || this.pending || this.handle || document.hidden) return;
    if (!this.refresh && (this.video.paused || this.video.ended)) return;
    const epoch = this.epoch;
    this.videoCallback = !this.refresh && typeof this.video.requestVideoFrameCallback === 'function';
    const next = () => { this.handle = 0; if (epoch === this.epoch) void this.tick(); };
    this.handle = this.videoCallback ? this.video.requestVideoFrameCallback(next) : requestAnimationFrame(next);
  }
  private async tick() {
    if (!this.active || this.pending || document.hidden) return;
    if (this.video.readyState < 2) { this.refresh = false; this.schedule(); return; }
    if (!this.refresh && performance.now() - this.last < 1000 / this.fps - 1) { this.schedule(); return; }
    this.refresh = false; this.pending = true; this.last = performance.now();
    const epoch = this.epoch;
    try {
      const ms = await this.render();
      if (epoch !== this.epoch) return;
      this.sample(ms);
      this.fps = ms > 28 ? 15 : 30;
    } catch (error) { if (epoch === this.epoch) { this.stop(); this.failure(error); } }
    finally { this.pending = false; this.schedule(); }
  }
}
