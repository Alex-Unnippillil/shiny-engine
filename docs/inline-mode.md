# Supported inline video

Inline enhancement is manually enabled from the extension popup. The initial adapter selects a visible, readable top-frame HTML5 video with native controls and a contained positioning context. It deliberately does not enumerate cross-origin frames or claim compatibility with custom players.

The original video remains responsible for playback and audio. The spatial canvas does not accept pointer input and leaves a bottom strip for native playback controls. Its object-fit/object-position follow the source. A visible in-page stop button and the popup restore action remove the canvas and restore the parent's previous inline positioning style.

Active native text tracks are a fallback condition: the adapter refuses to cover visible captions and stops if captions are enabled during a session. Use the separate capture viewer where permitted. The adapter also stops on source removal/error, native-video fullscreen, page closure and graphics failure. Protected media, unsupported positioning, transformed video and inaccessible pixels are not forced through.

These constraints are an explicit initial support boundary. Third-party player layouts, dynamic ad transitions, tracks, fullscreen and cross-origin behavior require per-site manual/browser fixtures before being listed as production-supported.
