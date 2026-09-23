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

The adapter's `exportPNG` re-presents retained snapshot/head buffers and calls canvas serialization in the same JavaScript task as submission. It does not await queue completion before asking HTMLCanvasElement/OffscreenCanvas to copy their bitmap. Serialization and GPU completion are then awaited together. Exports no longer depend on an expired presentation texture.

The regression test decodes actual exported PNGs and checks original/enhanced values, alpha, VideoFrame input and OffscreenCanvas output. Input feature buffers are read back in tests only; production reads only a four-byte validation flag. Synthetic residuals are not trained-model output. Check the exact revision's CI result.

## Video-frame alpha normalization

The transparent VideoFrame fixture exposed premultiplied RGB when sampled directly as an external texture. The frame bridge uses `copyExternalImageToTexture` with explicit `premultipliedAlpha: false` and `colorSpace: srgb` for both ImageBitmap and VideoFrame. Copy dimensions use displayWidth/displayHeight for video, not coded allocation size. The reusable GPU texture contains normalized SDR input before resampling/model preprocessing. This is an explicit GPU copy/conversion, not a zero-copy claim. Tests compare the same semitransparent source through both input types and actual exported PNG bytes.

Primary API references: WebGPU canvas expiry and color conversion at `https://gpuweb.github.io/gpuweb/`, and canvas serialization at `https://html.spec.whatwg.org/multipage/canvas.html`.
