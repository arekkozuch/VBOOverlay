# Architecture

FlappedEar Telemetry is a native Qt 6 application. C++ owns telemetry, media, project, synchronization, and export behavior; QML presents the editor and the reusable telemetry scene.

```text
video + VBO/project
        |
        v
 AppController -- async probe/parse --> MediaProbe / VboParser / TrackGeometry
        |                                  |                 |
        |                                  +---- TelemetrySession
        v
 WidgetModel + SyncTransform ---> TelemetryRenderContext ---> TelemetryScene.qml
                                                    |                 |
                                             preview QML          offscreen export
                                                                      |
                                                           QQuickRenderControl / QRhi
                                                                      |
                                              FFV1/BGRA staged overlay --> FFmpeg HEVC/AAC MP4
```

## Application

`AppController` is the QML-facing application boundary. It exposes source state, playback time, synchronization, live values, project dirty state, loading state, export state, analysis state, and the `WidgetModel`. It also owns the preview `TelemetryRenderContext`, so QML reads telemetry through one time transform rather than parsing files itself.

## Project

`ProjectDocumentState` tracks the current project path, revision, saved revision, and a pending destructive action. New, open, and quit therefore require a decision when the document is dirty.

`ProjectWriter` saves serialized `.fetproject` data through `QSaveFile` with direct-write fallback disabled. A project is marked saved only after the atomic write commits.

The saved `.fetproject` is the authoritative clean document. `AppController` retains its complete JSON object as the serialization base and overlays known edits onto nested project objects, so unknown or future fields survive open/edit/save cycles. Successful Save/Save As updates that base, marks the current revision saved, and removes stale recovery only after the project write commits. A failed save leaves both dirty state and recovery intact.

Persistent unsaved edits are serialized as one complete project object plus recovery version, original project path, timestamp, and revision metadata. `ProjectRecoveryStore` atomically replaces `project-recovery.json` under `AppLocalDataLocation` through `QSaveFile`; a restartable 250 ms single-shot debounce coalesces persistent edit bursts. Playback position, Analysis-window visibility, and other transient UI state do not trigger it. Recovery-write failure is logged and cannot modify the saved project.

QSettings is application preference storage, not document storage. It retains window and analysis-window geometry and the last project path. Legacy QSettings document keys under `editor/widgets`, `sync/*`, `sources/*`, and `analysis/*` are deleted and are never reconstructed as a clean document. Analysis channel configuration remains project content; floating-window visibility does not.

On a clean startup, the remembered `.fetproject` is opened through the ordinary transactional loader. If it is missing, the path is forgotten and a default new document is used. If dirty recovery exists, startup waits for an explicit choice: Recover transactionally loads the snapshot with its original path and keeps it dirty; Discard deletes it and loads the saved file, or starts a default document when there is no saved file. Discard during Quit, Open, or New also deletes recovery before continuing.

Project open first parses and validates the JSON version, widget scene, synchronization, analysis settings, and source-reference structure. It then commits that complete document atomically before resolving external assets. Video and telemetry resolve independently against the project directory and load asynchronously; a missing, mismatched, or invalid asset changes only its source state and never rolls back the document, widgets, synchronization, or settings.

`ProjectSourceReferenceCodec` owns the v2 source-reference migration, project-relative resolution, absolute fallback, serialization, and lightweight fingerprints. The current schema and compatibility rules are in [project-format.md](project-format.md).

## Source loading

Video metadata probing uses `MediaProbe`/`ffprobe`; VBO loading parses telemetry and builds `TrackGeometry`. Standalone loads, resolved project sources, and relink candidates run asynchronously. Their independent states are `idle`, `loading`, `ready`, `missing`, `mismatch`, or `error`. A relink candidate is probed/parsed before commit; a fingerprint mismatch is held outside committed state until the user explicitly accepts replacement.

Each source operation begins a new source generation and uses normalized source identities: cleaned absolute paths, or canonical paths when available. Results carry their generation and are ignored if a newer operation has started or identities no longer match. All long-running source work receives the same lightweight `CancellationCheck` callback and throws `OperationCancelled` on cancellation. VBO reads/parsing, media probes, GoPro packet indexing/reads/decoding, and both coarse and fine synchronization check it in bounded batches. Starting a replacement generation signals every previous source and sync token; destruction does the same before a bounded two-second convergence wait. Generation checks prevent stale commits while cancellation stops wasted work, so neither replaces the other. At startup, sources are restored only as part of loading the authoritative saved project or an explicitly accepted recovery snapshot.

## Telemetry

`VboParser` performs bounded chunked file reads, reads VBO sections, resolves standard channel aliases, normalizes supported coordinate formats, and produces a `TelemetrySession`. It rejects files above 128 MiB, more than 1,000,000 lines or 500,000 data rows, more than 512 columns, lines above 1 MiB, and fields above 64 KiB before the corresponding unbounded work. These limits leave substantial headroom over the validated 32,718-row, 49-channel fixture while preventing multi-GiB allocation patterns. `TelemetrySession` performs time-based channel lookup and interpolation, with one lazily cached median positive interval per immutable channel for O(1) cadence-gap thresholds after first use. `TrackGeometry` derives an offline normalized track outline from valid latitude/longitude samples and cooperatively checks cancellation in bounded batches.

`TelemetryRenderContext` combines a session, optional track geometry, and the central `SyncTransform`. Its telemetry time is `videoTime * timeScale + offset`; preview and export both use this context. The context converts normalized track points to its cached QML representation exactly when `setTrackGeometry()` is called and publishes a dedicated geometry revision; time updates do not rebuild that cache. It is also the presentation boundary: bounded holding and small channel-specific smoothing windows apply there only. `AppController` obtains analysis segments directly from raw `TelemetrySession` samples, so charts retain gaps and extrema and never inherit overlay filtering. The full behavioral contract is in [telemetry-semantics.md](telemetry-semantics.md).

## GoPro and synchronization

`GoProTelemetrySource` discovers the MP4 `gpmd` data stream with the authoritative `FfmpegTools::ffprobePath()`, indexes packets, and decodes supported GPS5/GPS9 records into a telemetry session. Its process runner polls without busy-waiting, enforces a 120-second timeout and 64 MiB stdout bound, and terminates/reaps ffprobe on cancellation or limit failure. Packet count is capped at 100,000, aggregate GPMF bytes at 512 MiB, parsed KLV headers at 1,000,000, and container depth at 32. The header counter covers all sensor and structural/container records across the track; limit diagnostics report actual count, configured limit, packet, and parse context. These independent limits remain overflow-safe and cancellable. Packet position/size is checked with subtraction-based file extents before allocation. Final GPS samples are stable-sorted by timestamp and later equal timestamps are discarded, preserving the first sample for each time; channels are then verified finite and strictly increasing before publication.

`TelemetrySyncEngine` compares GoPro GPS speed with VBO speed and returns an offset/time-scale candidate with diagnostics and confidence. Cancellation is checked between offsets and in sampling/correlation batches, without changing the existing numerical algorithm. Async sync results are also generation and path checked before they can affect the controller.

## Widgets and QML

The floating Analysis window is transient UI state. It starts closed for application startup, New, and Open. A QML `Loader` creates `AnalysisWindow` and its secondary `MediaPlayer` only after the user opens Analysis; closing it deactivates the loader and releases the secondary decoder. Channel selection remains persistent project configuration.

`WidgetModel` owns persistent widgets, groups, appearance cues, and templates. `TelemetryScene.qml` is the render-only telemetry layer: it has a render context and widget model but no editor-selection or media-player dependency. Its shared frame owns normalized geometry, appearance cues, background, border, title, and formatting helpers; one `Loader` then instantiates only the renderer matching each widget type from `qml/widgets/`. Editor interaction remains in the surrounding QML components, while preview and export use the same scene definition.

Custom templates are atomically persisted through the template store. Creating a template assigns a user ID; updating one replaces only its `widgets` snapshot after a successful store commit, retaining its ID, metadata, and compatible unknown fields. The editor records an applied template separately from the picker selection, so a passive picker change cannot redirect an in-place update. Picker selection is a QSettings UI preference keyed by template ID (never list index), survives template-list refreshes and application restart, and is deliberately reconciled to the first available template only when its selected ID disappears. Project documents do not store template provenance: loading one clears the applied-template relationship while retaining the independent picker preference.

The scene uses 1920×1080 as its canonical visual canvas. Normalized widget geometry is resolved directly against the target canvas, while pixel-like typography, padding, borders, lines, and markers use one scene scale. Canvases use widget-relative geometry; the retro Grand Prix renderer is the exception and retains its explicit 440×420 design-space transform, so it must not receive a second scene transform.

The modern motorsport broadcast HUD uses `TelemetryPanel.qml` as its dark translucent panel family: `#111A22` at 86% opacity, a restrained `#8895A3` border, canonical 12 px radii, 14 px label / 38 px value / 15 px unit roles, near-white primary text, and muted `#B5C0CA` secondary text. Speed, pedals, Retro Custom values, Heart Rate, and the G-Force bar own this surface directly, so existing layouts immediately use the same visual system rather than a generic wrapper. Throttle is green, active brake is red, and G-Force is amber; a zero brake is represented solely by the neutral track. The analog Retro Tachometer remains circular with an integrated rounded dark RPM value plate, while the F1 radar remains an unboxed circular field with six rings. `VisualSmokeScene.qml` is the deterministic composition review scene: it mounts these widgets over a bright roadway backdrop with representative sample-only data (4300 RPM, 86 km/h, 63% throttle, 18% brake, 84 °C ATF, 90 °C coolant, 145 bpm, 0.8 g), and the `motorsport-broadcast-smoke` template carries the same nine-widget layout for the editor and export scene. The local `--render-visual-smoke <png>` mode captures this production QML scene for visual review.

Optional widget `fontSize` values are canonical canvas pixels: zero or an absent/invalid value uses the renderer's established automatic expression, and a positive finite value is multiplied by the scene scale. This preserves old scenes and keeps explicit value typography proportional between preview, 720p, 1080p, 4K, and larger exports.

Classic G-Force, F1 G-Force Radar, and G-Force Bar renderers share a small presentation object that resolves the two channels, preserves missing-axis no-data behavior, applies per-widget axis inversion only for display, and derives combined magnitude. The F1 radar derives its concentric rings from `floor(maxG / ringStepG)` (defaults: 1.5 g and 0.25 g); it is a padded circular field with its own configurable dark background (`radarBackgroundColor`) and opacity, leaving the surrounding widget rectangle transparent. Its marker and the bar fill clamp visually, while the bar's numeric magnitude remains unbounded by `maxG`.

`TrackWidget.qml` keeps the normalized polyline in a static Qt Quick `Shape`/`PathPolyline` layer. Its pixel path is derived only from a geometry revision, widget dimensions, scene-scaled track padding, or mirroring; line width and color remain direct static shape properties. The current-position marker is a separate QML rectangle bound to `currentTrackPoint`, so playback and export time changes never reconstruct or repaint the full path.

## Export

`MediaProbe` reads explicit source and output raster, rate, codec/profile, bit-depth, orientation, and raw color metadata. `ExportMediaProfile` centrally derives the preservation format and encoder profile. `EncoderDetector` tests usable HEVC encoders and caches an exact raster/rate/pixel-format/profile probe; `TelemetryFrameRenderer` mounts `TelemetryScene.qml` offscreen through `QQuickRenderControl` and QRhi and checks the backend's reported texture limit. QRhi readback is canonical premultiplied RGBA and conditionally normalized from Y-up once before Stage A; Stage B explicitly composites it as premultiplied alpha. `ExportEngine` runs the two-stage FFmpeg pipeline only after those checks. `RawFrameTransport` applies bounded byte-oriented backpressure while complete rendered frames are delivered to the Stage A encoder. `ExportOutputTransaction` creates and owns a same-directory staging output, rejects linked targets, snapshots an approved existing regular file's native identity and modification state, and commits only when that state still matches immediately before replacement. `AppController` consumes worker events and is the sole writer of the corresponding durable export diagnostic file, preserving the same formatted entries shown by Very Verbose without concurrent worker/UI file access. Color policy is documented in [media-color-policy.md](media-color-policy.md).

The detailed pipeline and timing contract are in [export-pipeline.md](export-pipeline.md). Target-file transaction guarantees are in [export-output-safety.md](export-output-safety.md).
