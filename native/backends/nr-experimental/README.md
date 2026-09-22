# Experimental NR backend — blocked, not implemented

This directory deliberately contains no fake renderer, DLL loader or model download.
The baseline build never loads or executes NVIDIA assets. `models/registry.json`
reports this backend unavailable. Shiny Engine is not an implementation of DLSS 5.

Required before implementation/activation:

1. Establish lawful access/use/redistribution of the exact model and dependencies;
   a user-selected file is not proof of permission.
2. Pin and audit the chosen OpenDLSS-NR revision and preserve required notices.
3. Prove D3D11-to-Vulkan adapter matching, shared surfaces and synchronization.
4. Reproduce independent still-frame parity fixtures and record missing evidence.
5. Evaluate optical flow, disocclusions, temporal resets and full pipeline latency.

Do not bypass any gate by returning a sharpened image under a neural backend name.
The original research plan is preserved in `docs/RESEARCH_AND_IMPLEMENTATION_PLAN.md`.
