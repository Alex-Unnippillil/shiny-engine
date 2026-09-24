# Main workspace — 0.10

The main player now shares the Libraries / Video studio theme. This update changes interaction and presentation, not the processing or approval model.

## Start, watch, review

The welcome screen offers Open media / Network stream after libVLC loads. Without a compatible runtime, its primary action is Locate VLC. Empty playback and media-dependent studio/research commands remain disabled; local file selection and runtime guidance stay visible.

The header badge comes from the primary libVLC state: READY, OPENING, PLAYING, PAUSED, SOURCE ERROR or VLC REQUIRED. It does not describe any independent enhancement worker. The source title identifies the selected queue item; diagnostics retain separate privacy rules.

**Queue** has displayed-title filtering and native list navigation. Words match regardless of their order; matching is case-insensitive according to the process's wide-character locale. It does not search source paths or credential-bearing URLs. Arrow keys and type-ahead stay with the list; Enter plays the highlighted row and Delete removes it. Removing a filtered row resolves its actual source index. Reordering is disabled while the filter is nonempty so hidden items are not moved implicitly.

**Adjustments** holds the existing live VLC filters. Switching views does not change playback, the queue, filter values or enhancement approval. At compact widths the sidebar hides; queue selection remains in Playback → Choose queued item. Resize wider for the full filter and adjustment controls. Focus moves to Quick actions if a sidebar control becomes hidden.

## Quick actions

Use the header button or Ctrl+K. Search only filters a fixed list of existing UI commands. It cannot run typed code, shell commands or arbitrary DLL/path requests. An unavailable command explains the missing media prerequisite and cannot be executed. Enter runs the selection, arrows navigate and Escape cancels. The owner is restored after closure; cancellation neither stops playback nor grants model/library consent.

Ctrl+O / Ctrl+N remain global within the main workspace; Ctrl+F reveals the queue view and focuses its filter when visible. Focused native controls retain ordinary text/slider/list keys. In a nonempty queue filter, Escape clears the query first; otherwise Escape leaves fullscreen or stops primary playback. F1 shows help. The README has the full shortcut table.

## Visuals and accessibility boundary

The main player uses the shared Win32 theme, readable owner-drawn queue cards with native string counterparts, visible focus/hover states, system high-contrast colors and monitor DPI scaling. Native menus and file pickers are retained. The playback sliders are themed without replacing their native input controls. These implementation choices are not a claim of completed screen-reader or all-display certification.

Actual screenshot files are published with the exact release: player-welcome.png, player-workspace.png, player-adjustments.png, player-quick-actions.png and player-workspace-compact.png. They show synthetic test media, not evidence of photographic enhancement. The README links to these versioned assets and includes Mermaid architecture and package-selection diagrams.

## Unchanged boundaries

Main playback/audio, independent CPU SDR Video studio, imported-output comparison and optional neural-model research remain distinct. No new NVIDIA DLSS/Streamline adapter, primary-output enhancement, audio-synchronized neural rendering, HDR, updater or signing is added. Package leases, build-pinned approvals, worker probes, model-data validation and session acknowledgment remain in force. See library-manager-guide.md and research-mode.md.
