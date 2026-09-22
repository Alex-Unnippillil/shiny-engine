# Security and privacy boundaries

## Assets and trust

A captured window may contain sensitive information. Browser pages are untrusted; extension pages are privileged; the native companion runs with the installing user's permissions. The application never needs administrator access, remote inference, arbitrary shell execution or a public network listener.

The extension starts with activeTab/scripting/storage permissions. Tab capture and native messaging are optional and requested for the corresponding feature. Only allowlisted extension-origin UI pages may route privileged messages. Page content cannot select arbitrary native windows, paths, commands or DLLs. Native envelopes reject unknown keys/commands, invalid ranges and unauthorized session IDs. The host's exact extension origin is pinned by its per-user installation.

Native messaging is a bounded JSON control channel (64 KiB input limit in this host), not media transport. There are at most eight pending extension requests with finite timeouts. The companion uses system source selection and unpredictable opaque session IDs. Every new selection or stop invalidates the previous session.

## Capture and data handling

Capture requires an explicit user action. The browser presents its consent picker or a toolbar-authorized tab capture. A separate viewer avoids injecting captured output into its own source. Browser monitor capture is disabled. The native output requests Windows capture exclusion and refuses initialization if that call fails. Real feedback prevention remains a hardware/platform validation requirement; capture exclusion is not a DRM guarantee.

The native capture window remains visible with a stop button and Ctrl+Shift+F10 emergency stop. Pausing retains capture and is labeled accordingly. Original applications keep their own input and audio. There is no click/keyboard injection or protection bypass. Browser stop/page closure releases browser tracks; close/stop the native preview independently when finished.

Files and frames are processed locally. No runtime fetch, analytics, uploads, camera access or frame recording is implemented. PNG export and diagnostic downloads are explicit user actions. Settings may be stored locally. Diagnostic reports omit media names, URLs and frame content.

## Supply chain and operational limits

All extension executable assets are bundled; no eval, remote scripts or WASM loading. npm has one pinned development dependency and no runtime dependencies. CI has read-only repository permissions and third-party actions are commit-pinned. Browser test fixtures are synthetic, not personal desktop captures.

The native build is unsigned alpha software. Do not disable endpoint controls to run it, expose CI runners to personal screens, or claim install/runtime safety based only on compilation. Audit native installer/uninstaller changes, kernel/driver boundaries, capture lifecycle, memory bounds, local hostile-user scenarios and future update mechanisms before a signed production release.
