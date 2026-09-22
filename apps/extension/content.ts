// A classic injected script. Dynamic module imports happen only after explicit activation.
(() => {
  const scope = globalThis as unknown as { __shinyInline?: boolean; chrome: import('../../packages/contracts/browser.js').BrowserAPI };
  if (scope.__shinyInline) return; scope.__shinyInline = true;
  const api = scope.chrome;
  let release: (() => void) | null = null, epoch = 0;
  api.runtime.onMessage.addListener((message, sender, reply) => {
    if (sender.id !== api.runtime.id || !message || typeof message !== 'object') return false;
    const data = message as Record<string, unknown>;
    if (data.type !== 'shiny-inline') return false;
    if (data.command === 'stop') { epoch++; release?.(); release = null; reply({ ok: true, message: 'Original video restored.' }); return false; }
    if (data.command !== 'start') return false;
    const requestEpoch = ++epoch; release?.(); release = null;
    void (async () => {
      const candidates = Array.from(document.querySelectorAll('video')).filter(v => v.controls && v.readyState >= 2 && v.videoWidth > 0 && v.getBoundingClientRect().width > 100);
      const video = candidates.sort((a, b) => b.clientWidth * b.clientHeight - a.clientWidth * a.clientHeight)[0];
      if (!video) throw new Error('No supported top-frame video with native controls. Use separate tab capture for this player.');
      if (video.mediaKeys || getComputedStyle(video).transform !== 'none') throw new Error('Protected or transformed video is unsupported. The original is unchanged.');
      const captionsVisible = () => Array.from(video.textTracks).some(track => track.mode === 'showing');
      if (captionsVisible()) throw new Error('Native captions are visible. Use the separate viewer to preserve them.');
      const parent = video.parentElement;
      if (!parent || parent === document.body) throw new Error('This player layout needs the separate viewer.');
      const [{ createRenderer }, { FrameLoop }, { DEFAULTS }] = await Promise.all([
        import(api.runtime.getURL('packages/gpu-web/renderer.js')) as Promise<typeof import('../../packages/gpu-web/renderer.js')>,
        import(api.runtime.getURL('packages/gpu-web/scheduler.js')) as Promise<typeof import('../../packages/gpu-web/scheduler.js')>,
        import(api.runtime.getURL('packages/contracts/index.js')) as Promise<typeof import('../../packages/contracts/index.js')>,
      ]);
      const holder = document.createElement('div');
      const { renderer, canvas } = await createRenderer(holder);
      if (requestEpoch !== epoch) { renderer.release(); return; }
      const originalPosition = parent.style.position;
      if (getComputedStyle(parent).position === 'static') parent.style.position = 'relative';
      canvas.style.cssText = 'position:absolute;pointer-events:none;z-index:1;object-fit:contain;clip-path:inset(0 0 54px 0)';
      const stopButton = document.createElement('button'); stopButton.textContent = 'Stop enhancement';
      stopButton.style.cssText = 'position:absolute;right:12px;top:12px;z-index:2;border:1px solid #749386;background:#12251f;color:#d9ffed;border-radius:5px;padding:7px 10px;font:12px system-ui;cursor:pointer';
      stopButton.setAttribute('aria-label', 'Stop Shiny Engine enhancement');
      let stopped = false;
      const cleanup = () => {
        if (stopped) return; stopped = true; loop.stop(); resize.disconnect(); observer.disconnect();
        canvas.remove(); stopButton.remove(); parent.style.position = originalPosition;
        document.removeEventListener('fullscreenchange', fullscreen); window.removeEventListener('pagehide', cleanup);
        video.removeEventListener('emptied', cleanup); video.removeEventListener('error', cleanup);
        video.textTracks.removeEventListener('change', captions); video.textTracks.removeEventListener('addtrack', captions);
        renderer.release();
      };
      const failed = () => cleanup();
      const captions = () => { if (captionsVisible()) cleanup(); };
      const loop = new FrameLoop(video, () => renderer.render(video, { ...DEFAULTS, split: 0 }), failed);
      const place = () => {
        const vs = getComputedStyle(video);
        if (!video.isConnected || vs.transform !== 'none') { cleanup(); return; }
        // Limit to contained native-player layouts. Preserve the native control strip.
        canvas.style.objectFit = vs.objectFit; canvas.style.objectPosition = vs.objectPosition;
        canvas.style.left = `${video.offsetLeft}px`; canvas.style.top = `${video.offsetTop}px`;
        canvas.style.width = `${video.offsetWidth}px`; canvas.style.height = `${video.offsetHeight}px`;
      };
      const resize = new ResizeObserver(place);
      const observer = new MutationObserver(() => { if (!video.isConnected) cleanup(); });
      const fullscreen = () => { if (document.fullscreenElement === video) cleanup(); else place(); };
      try {
        await renderer.render(video, { ...DEFAULTS, split: 0 }); // Access check before covering any pixels.
        if (requestEpoch !== epoch) { cleanup(); return; }
        if (!video.isConnected || video.offsetParent !== parent) throw new Error('This player positioning is unsupported. Use the separate viewer.');
        if (captionsVisible()) throw new Error('Captions became visible. The original is unchanged.');
        parent.append(canvas, stopButton); place(); resize.observe(video); observer.observe(document.documentElement, { subtree: true, childList: true });
        stopButton.addEventListener('click', cleanup); document.addEventListener('fullscreenchange', fullscreen);
        window.addEventListener('pagehide', cleanup, { once: true }); video.addEventListener('emptied', cleanup); video.addEventListener('error', cleanup);
        video.textTracks.addEventListener('change', captions); video.textTracks.addEventListener('addtrack', captions);
        release = cleanup; loop.start(); reply({ ok: true, message: `${renderer.label} active. Audio and native controls remain with the original player.` });
      } catch (error) { cleanup(); throw error; }
    })().catch(error => reply({ ok: false, error: error instanceof Error ? error.message : 'This video cannot be processed.' }));
    return true;
  });
})();
