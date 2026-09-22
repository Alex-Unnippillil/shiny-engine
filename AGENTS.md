# Contributor instructions

Read docs/RESEARCH_AND_IMPLEMENTATION_PLAN.md and docs/implementation-status.md before changing scope.

Preserve original playback and an immediate stop path. Keep capture explicitly consented, model backends honestly labeled, native commands strictly allowlisted and all frame processing local. Never bundle proprietary/leaked assets, claim DLSS functionality for spatial filters, add arbitrary DLL/path/shell RPC, weaken the application CSP to make tests pass, or circumvent DRM/capture/administrator policy.

Run npm run check, npm run test:browser and the Windows CI compile/protocol/HLSL tests. Browser software rendering is not a physical-GPU test. Keep support claims tied to actual evidence. Add fixtures for source changes, disposal, errors, geometry and alpha. No fake enabled states for unimplemented backends.

One integrator owns shared contracts, lockfiles, manifests and workflows. Isolate browser UI, GPU processing, native platform and QA work in focused PRs. Do not change main directly for feature work. Merge only after the exact head's required checks pass; keep experimental native/model gates explicit.
