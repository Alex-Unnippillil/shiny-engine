/** Serial, source-driven execution. No frame queue or accumulating latency. */
export class FrameLoop {
  private active = false;
  private pending = false;
  private handle = 0;
  private videoCallback = false;
  private last = -Infinity;
  private epoch = 0;
  private fps = 30;
  constructor(private video: HTMLVideoElement, private render: () => Promise<number>,
    private failure: (error: unknown) => void, private sample: (ms: number) => void = () => {}) {}
  start() {
    if (this.active) return;
    this.active = true;
    this.schedule();
  }
  stop() {
    this.active = false;
    this.epoch++;
    if (this.videoCallback) this.video.cancelVideoFrameCallback(this.handle);
    else cancelAnimationFrame(this.handle);
  }
  private schedule() {
    if (!this.active || this.pending) return;
    const epoch = this.epoch;
    this.videoCallback = typeof this.video.requestVideoFrameCallback === 'function';
    const next = () => { if (epoch === this.epoch) void this.tick(); };
    this.handle = this.videoCallback ? this.video.requestVideoFrameCallback(next) : requestAnimationFrame(next);
  }
  private async tick() {
    if (!this.active || this.pending) return;
    if (document.hidden || this.video.readyState < 2 || performance.now() - this.last < 1000 / this.fps - 1) {
      this.schedule(); return;
    }
    this.pending = true;
    this.last = performance.now();
    const epoch = this.epoch;
    try {
      const ms = await this.render();
      if (epoch !== this.epoch) return;
      this.sample(ms);
      // Degrade pacing rather than retain old frames. This is not GPU certification.
      this.fps = ms > 28 ? 15 : 30;
    } catch (error) { if (epoch === this.epoch) { this.active = false; this.failure(error); } }
    finally { this.pending = false; this.schedule(); }
  }
}
