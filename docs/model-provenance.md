# Model and dependency provenance

There are **no trained-model weights or proprietary DLLs** in this release. Active filters remain original spatial WGSL, GLSL and HLSL; they are not NVIDIA Image Scaling or DLSS.

Version 0.3.0 includes the MIT OpenDLSS-NR browser graph as an experimental inference implementation, pinned by Git submodule and per-file SHA-256. `scripts/prepare-nr.mjs` bundles the selected source, license and notices and changes its URL-based loaders. The application owns a separate GPU preprocessing/composition wrapper. This code is not a proprietary NVIDIA runtime and has not established vendor-output equivalence.

`models/registry.json` records backend identity. `models/nr-approvals.json` is the empty reviewed model register: no exact trained model is permitted to activate in this build. Only reviewed metadata is compiled into the worker; a file picker, URL parameter, checkbox or storage setting cannot authorize a model. Local intake is data-only, with manifest and stage hash/bounds checks repeated in the worker.

Before enabling a neural backend, establish the exact upstream revision/notices, immutable model hash, provenance, permitted runtime use, conversion/derivative rights, redistribution terms, tensor/geometry/color requirements, supported operators and independent correctness fixtures. Record separate physical-hardware evidence before advertising performance. A user-provided DLL is not a rights clearance. No generic DLL loader, extraction path, weight downloader or arbitrary model URL is exposed.

TypeScript 5.8.3 is development-only with npm integrity; Playwright 1.63.0 is test-only and pins Chromium 153 for CI. Executable extension assets and shaders are bundled. Windows SDK/C++/WinRT/Direct3D are platform dependencies, not redistributed SDK source. No NeuralScreen source/runtime is included. See `THIRD_PARTY_NOTICES.md` and `neural-lab.md`.
