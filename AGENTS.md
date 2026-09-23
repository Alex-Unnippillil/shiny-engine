# Contributor instructions

Read docs/RESEARCH_AND_IMPLEMENTATION_PLAN.md and docs/implementation-status.md before changing scope.

Preserve original playback and an immediate stop path. Keep capture explicitly consented, model backends honestly labeled, native commands strictly allowlisted and all frame processing local. Never bundle proprietary/leaked assets, claim DLSS functionality for spatial filters, add arbitrary DLL/path/shell RPC, weaken the application CSP to make tests pass, or circumvent DRM/capture/administrator policy.

Run npm run check, npm run test:browser and Windows CI compilation, protocol, HLSL, VLC, model-intake and installer tests. Software rendering is not a physical-GPU test. Tie claims to evidence; add fixtures for errors, disposal, geometry, source changes and alpha. No fake enabled states.

One integrator owns shared contracts, lockfiles, manifests and workflows. Work on a feature branch and merge only after exact-head required checks pass. Preserve the original research plan.

## Explicit local research mode

The owner requested educational local-model experimentation. Native Local research is distinct from curated approval: require explicit session acknowledgment bound to a validated fingerprint and strict data bounds/hashes. Never add user data to production approvals or label output reviewed. Models and NVIDIA runtimes remain unbundled. Preserve primary playback and cancellation. Real trained-model, hardware, temporal and vendor-parity claims require separate evidence. CI data fixtures must be clearly marked synthetic and untrained.
