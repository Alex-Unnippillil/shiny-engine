# Benchmark method

The lab reports wall-clock time from CPU render submission through GPU completion, including asynchronous browser scheduling. It does not expose hardware GPU timestamp queries. Results from WebGL2 software graphics in CI are functional evidence only.

The built-in diagnostic processes 60 still frames of the currently selected image. It retains at most 600 timing samples and exports p50/p95/p99, current configuration, dimensions, backend and a timestamp. It omits source filenames, browsing URLs and media pixels. `certification` remains `unverified`.

For hardware validation, record exact source commit, OS, browser, driver, GPU, power mode, source dimensions/codec and display geometry/color. Measure cold initialization separately from warm processing. Keep capture, conversion, copying/interop, filter/model, composition, presentation and complete capture-to-display latency separate. Include sustained 30-minute measurements, missed deadlines, thermal/power behavior, A/V skew and allocation counts across 100 lifecycle changes.

The plan's 25 ms processing p95 / 30 fps and 50 ms native capture-to-present goals are proposed acceptance gates, not achieved results. The current bounded scheduler reduces browser processing to 15 fps when completion is slow and never accumulates a queue of old frames. That behavior prevents unbounded backlog; it is not proof of smooth full-desktop interaction.

A comparison image must use identical source media and scale, preserve the original, and identify spatial versus generative processing. Review text, faces, fine lines, dark scenes, compression, alpha, motion and scene changes. Never treat a nonconstant neural output as parity or recoverable ground truth.
