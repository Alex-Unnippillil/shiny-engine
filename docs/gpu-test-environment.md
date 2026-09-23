# GPU test environment and canvas snapshot lifetime

The browser suite pins Playwright 1.63.0 / Chromium 153. Linux CI runs the neural GPU test with a headed Xvfb surface and a shared SwiftShader Vulkan implementation for ANGLE and Dawn. This is a disposable trusted synthetic-test environment, not a request to disable user or administrator browser policy.

To reproduce that Linux CI invocation where permitted:

```sh
python -m pip install playwright==1.63.0
python -m playwright install --with-deps chromium
SHINY_GPU_HEADED=1 xvfb-run -a npm run test:browser
```

The ordinary `npm run check` does not require a display. Do not claim a physical GPU, trained-model inference, or full-network parity from this software-rendered suite.

## Diagnosed rather than ignored

The initial independent software-compositor configuration failed while creating WebGPU canvas shared images. A separate minimal diagnostic reproduced the failure before any neural code. A shared Vulkan/ANGLE configuration rendered correct GPU texture bytes. On the tested headed software stack, immediate canvas snapshots contained the rendered image while a later canvas read could return cleared bytes.

The adapter's `exportPNG` now re-presents the retained snapshot/head buffers and calls canvas serialization in the same JavaScript task as submission. It does not await queue completion before asking HTMLCanvasElement/OffscreenCanvas to copy their bitmap. The resulting serialization and GPU completion are then awaited together. This prevents an export from depending on an expired presentation texture.

The regression test decodes the actual exported PNGs and checks original/enhanced values, retained alpha, VideoFrame input and OffscreenCanvas output. Full input feature buffers are read back in tests only to verify conditioning and periodic padding; production reads only a four-byte validation flag.

Do not relax pixel assertions or report blank output as a passing inference test. The synthetic residual used by this suite is explicitly not trained-model output. See the exact revision's CI result, not an earlier test report.

Primary API references: WebGPU canvas rendering/automatic expiry at `https://gpuweb.github.io/gpuweb/`, and HTML canvas/OffscreenCanvas serialization at `https://html.spec.whatwg.org/multipage/canvas.html`.
