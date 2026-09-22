# Model and dependency provenance

There are **no model weights, inference runtimes or proprietary DLLs** in this release. All active filters are original five-tap spatial implementations in WGSL, GLSL and HLSL. They are not NVIDIA Image Scaling or DLSS.

`models/registry.json` records backend identity and disabled research entries. The original implementation is MIT. TypeScript 5.8.3 is development-only, pinned with npm integrity; Playwright 1.57.0 is a test dependency. Executable extension assets and shaders are local. Windows SDK/C++/WinRT/Direct3D are platform dependencies, not bundled SDK source. See THIRD_PARTY_NOTICES.md.

Before enabling any neural backend, register the exact upstream revision, dependency notices, immutable model hash, model provenance, permitted runtime use, conversion/derivative rights, redistribution terms, geometry/color requirements, supported operators, independent correctness fixtures and physical-hardware evidence. A user-provided DLL is not, by itself, a rights clearance. No generic DLL loader, automatic weight downloader or arbitrary model URL is exposed.

The preserved research plan names OpenDLSS-NR as a potential MIT-code implementation reference. It separately excludes NVIDIA model rights. No source or runtime from NeuralScreen is included. Research links do not imply incorporated code, permission, compatibility or NVIDIA endorsement.
