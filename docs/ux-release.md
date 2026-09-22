# Media Lab UX — 0.2.0 alpha

## Changes

- Source selection above the preview; compact responsive layout; bounded portrait preview; useful empty and loading states.
- Pointer/touch/keyboard comparison divider, original/split/enhanced views, fullscreen and keyboard help.
- Single processed video preview, play/pause/replay, seeking, speed, volume and mute. Native video controls are retained as a fallback.
- Candidate media is decoded before source replacement. Failed decoding, denied capture and cancelled loading preserve the current preview. Superseded captures are stopped and stale load results are ignored.
- Presets, custom settings, comparison and output scale round-trip through optional local storage. Demo and adjustment reset are independent actions.
- Explicit original/enhanced/current-view PNG export and cancellable 60-frame diagnostics.
- Context-aware extension popup and in-app companion setup guidance. No additional browser permissions.

## Validation contract

`npm run check` covers strict TypeScript, protocol/settings/provenance checks and UX helpers. `npm run test:browser` exercises real Chromium software graphics, image/video decoding, actual PNG bytes, paused scheduling, loading races, cancellation, help focus and responsive layouts. It installs the actual MV3 extension and opens its viewer. CI retains the exact source snapshot, reports, screenshots and installable packages.

The execution environment for development may prohibit local browser navigation; GitHub Actions runs the browser suite on a regular hosted runner without changing browser policy or weakening CSP. See the successful run on the exact merged commit, not a previous passing revision.

Software-rendered Chromium behavior is not physical GPU performance certification. Windows capture, native pairing, actual permission pickers/audio, DRM, long-session timing, and third-party player compatibility require separate platform tests. Native Windows code is unchanged; its compile/protocol/HLSL checks still gate this PR.

## Privacy and scope

No files, frames, filenames or browsing URLs are uploaded. Only adjustments persist in the browser; diagnostic reports omit source names. PNG filenames use the explicitly selected local media name after sanitization. Capture still requires an explicit user action. The update adds no neural inference, proprietary model, HDR or click-through desktop overlay.
