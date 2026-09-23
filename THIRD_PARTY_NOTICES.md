# Notices and provenance

The original application source, spatial WGSL/GLSL/HLSL shaders, UI, icon and generated spatial test fixture are Shiny Engine implementation under MIT. Spatial filters are not NVIDIA Image Scaling, Anime4K, DLSS or neural models.

There are no runtime npm dependencies or remotely loaded executable assets. TypeScript 5.8.3 is a development-only Apache-2.0 dependency pinned with registry integrity. Playwright is Apache-2.0 and test-only. Node and Python are build/test prerequisites. Windows SDK, C++/WinRT and Direct3D are platform dependencies under their respective Microsoft terms; SDK source is not redistributed here.

The original research plan is preserved as a reference document. Its links do not imply that those codebases or assets are included. No NeuralScreen, NijiLucid, Anime4K or Real-ESRGAN code or model data is bundled. Shiny Engine is independent and not NVIDIA-endorsed.

## OpenDLSS-NR reference (0.3.0)

The NR lab bundles MIT-licensed JavaScript/WGSL from maanHimself/OpenDLSS-NR, pinned to `9d08f4184bbcb9d858e2fb7a7834ec0837a9d2f1`. Copyright (c) 2026 maan. Source is a pinned Git submodule. Per-file SHA-256 values are recorded in `models/nr-reference-lock.json`. The upstream LICENSE and NOTICE are copied into each build's `vendor/opendlss/` directory.

Build-time modifications replace URL-based model/shader loading with verified local model data and bundled shader strings. Graph arithmetic is not replaced with spatial filtering. The separately identifiable SDR texture preprocessing/composition wrapper is in `packages/nr/engine.ts`; it adds periodic mirror padding and has not established NVIDIA-output parity.

No NVIDIA software, weights, trademark rights or IP rights are granted by the upstream MIT license. No trained-model assets or proprietary DLLs are bundled. The default reviewed-model register is empty, so inference is not enabled in this release.
