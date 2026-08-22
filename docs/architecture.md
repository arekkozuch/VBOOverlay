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

Project open first validates the JSON version, widget scene, and synchronization data. It then loads requested sources in the background into a candidate result. The existing project remains committed until the candidate is complete; only then does `AppController` replace widgets, sources, track data, sync state, and project metadata together.

## Source loading

Video metadata probing uses `MediaProbe`/`ffprobe`; VBO loading parses telemetry and builds `TrackGeometry`. The standalone loads and project-source loads run asynchronously.

Each source operation begins a new source generation and uses normalized source identities: cleaned absolute paths, or canonical paths when available. Results carry their generation and are ignored if a newer operation has started or identities no longer match. Cancellation flags are passed to probe operations. At startup, saved source paths are restored through the same asynchronous loaders when their files still exist.

## Telemetry

`VboParser` reads VBO sections, resolves standard channel aliases, normalizes supported coordinate formats, and produces a `TelemetrySession`. `TelemetrySession` performs time-based channel lookup and interpolation. `TrackGeometry` derives an offline normalized track outline from valid latitude/longitude samples.

`TelemetryRenderContext` combines a session, optional track geometry, and the central `SyncTransform`. Its telemetry time is `videoTime * timeScale + offset`; preview and export both use this context. The full behavioral contract is in [telemetry-semantics.md](telemetry-semantics.md).

## GoPro and synchronization

`GoProTelemetrySource` discovers the MP4 `gpmd` data stream with `ffprobe`, indexes packets, and decodes supported GPS5/GPS9 records into a telemetry session. `TelemetrySyncEngine` compares GoPro GPS speed with VBO speed and returns an offset/time-scale candidate with diagnostics and confidence. Async sync results are also generation and path checked before they can affect the controller.

## Widgets and QML

`WidgetModel` owns persistent widgets, groups, appearance cues, and templates. `TelemetryScene.qml` is the render-only telemetry layer: it has a render context and widget model but no editor-selection or media-player dependency. Its shared frame owns normalized geometry, appearance cues, background, border, title, and formatting helpers; one `Loader` then instantiates only the renderer matching each widget type from `qml/widgets/`. Editor interaction remains in the surrounding QML components, while preview and export use the same scene definition.

The scene uses 1920×1080 as its canonical visual canvas. Normalized widget geometry is resolved directly against the target canvas, while pixel-like typography, padding, borders, lines, and markers use one scene scale. Canvases use widget-relative geometry; the retro Grand Prix renderer is the exception and retains its explicit 440×420 design-space transform, so it must not receive a second scene transform.

## Export

`MediaProbe` reads source and output metadata; `EncoderDetector` tests usable HEVC encoders; `TelemetryFrameRenderer` mounts `TelemetryScene.qml` offscreen through `QQuickRenderControl` and QRhi; and `ExportEngine` runs the two-stage FFmpeg pipeline. `ExportOutputTransaction` creates and owns a same-directory staging output, validates it through the worker, and commits it to the selected target only after success.

The detailed pipeline and timing contract are in [export-pipeline.md](export-pipeline.md). Target-file transaction guarantees are in [export-output-safety.md](export-output-safety.md).
