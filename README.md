# FlappedEar Telemetry

FlappedEar Telemetry is a native desktop editor for synchronizing motorsport telemetry with video and rendering telemetry overlays.

## Status

The application is a Qt 6, C++20, and QML native application in alpha and active development. Development has been validated on macOS. Windows runtime/export has been validated on one Windows 11 / Qt 6.11 / MSVC 2022 / Intel Iris Plus / Quick Sync configuration; broader hardware and packaging validation remain pending.

## Current capabilities

- MP4/MOV playback with timeline controls and preview overlays.
- RaceChrono and VBOX VBO telemetry import.
- GoPro GPMF GPS extraction and GPS-speed auto synchronization.
- A visual widget editor, projects, built-in layouts, and shareable templates. Both editor sidebars remain fully scrollable at the supported 1180×720 minimum size.
- Lazily loaded synchronized telemetry analysis, including charts and a track view; its secondary decoder exists only while the Analysis window is open.
- Source-driven CFR HEVC/AAC MP4 export at the effective rational export rate, including runtime raster/profile checks, validated 8-bit and 10-bit SDR preservation, optional custom source ranges, progress, cancellation, and verbose diagnostics retained in a durable per-export log.

## Keyboard controls

Space plays or pauses. Left/Right seek five seconds; Shift+Left/Right seek thirty seconds; Home/End seek to timeline bounds. These playback shortcuts are disabled while typing or operating a focused editor control. Ctrl/Cmd+E opens Export, Ctrl/Cmd+Shift+A toggles Telemetry Analysis, and F11/Escape enter and leave full screen. Full-screen preview provides the same visible transport and scrubber as the editor.

Very Verbose export diagnostics follow the live tail until the user scrolls into history. Historical inspection stays fixed while new lines arrive; **Jump to latest** explicitly resumes following.
- Portable `.fetproject` media references with project-relative lookup, bounded source fingerprints, missing-media recovery, explicit relinking, and stale asynchronous-result rejection.
- Crash-safe export-output handling with state-bound overwrite consent, atomic project saving, and explicit unsaved-change recovery.
- Resource-bounded external JSON documents and subprocess output, with visible recovery-protection warnings when automatic snapshots cannot be persisted.

## Requirements

- CMake 3.24 or newer.
- A C++20 compiler.
- Qt 6.8 or newer with Concurrent, Core, Gui, Quick, Quick Controls 2, Multimedia, and Test.
- FFmpeg and ffprobe available at runtime. Export depends on an externally installed FFmpeg and a working HEVC encoder; neither is bundled.

The supported development targets are macOS and Windows. The repository currently has tested macOS/Homebrew commands below; it has no separate, validated Windows packaging procedure yet.

## Build

On Apple Silicon with Homebrew Qt in `/opt/homebrew/opt/qt`:

```bash
cmake -S . -B build-native \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_PREFIX_PATH=/opt/homebrew/opt/qt
cmake --build build-native --parallel
ctest --test-dir build-native --output-on-failure
open "build-native/native/FlappedEar Telemetry.app"
```

## Architecture

The application keeps telemetry parsing, synchronization, video/media handling, widgets, and QML presentation separate. Preview and export mount the same `TelemetryScene.qml` with independent render contexts. Track geometry is converted and painted as a static layer when its source or appearance changes; playback updates move only the independent position marker.

## Widget editing

G-Force widgets can invert lateral and longitudinal presentation axes independently, without changing imported telemetry. Alongside the classic target, the widget catalog includes an **F1 G-Force Radar** with a dark semi-transparent circular field, 1.5 g range, and six 0.25 g concentric-ring levels by default, plus a compact **G-Force Bar** that shows true combined magnitude while clamping only its fill. Speed, pedals, Heart Rate, Retro Custom, and G-Force Bar use the same translucent charcoal broadcast panel, typography, rounded border, and neutral bar-track treatment; throttle is green, brake is red only when it has fill, and G-Force is amber. The analog retro tachometer keeps its circular identity with a matching rounded dark RPM plate. `Retro Custom` accepts arbitrary channel, adjustment, formatting, unit, fallback, and palette settings. Text-bearing value widgets offer **Font size**: `Auto` retains their responsive legacy sizing, while a positive canonical size is scene-scaled consistently for preview and every export resolution.

Templates have two explicit operations. **Save current** updates the custom template that was applied (or just created), preserving its ID, name, and description. Selecting a template alone does not make it editable, so Save current opens **Save as new** until that custom template is applied. Built-in templates are immutable and always use Save as new.

A saved `.fetproject` is the authoritative clean document. It can open without its external video or VBO assets; missing or mismatched sources remain independently relinkable without losing the scene or settings. Unsaved persistent edits are held separately in an atomic recovery snapshot and are recovered or discarded explicitly at startup only when their logical document state is newer than the saved authority. A leftover snapshot after successful Save is cleanup debt, not degraded recovery protection or a user warning. QSettings stores only application preferences and the last project path. The portable source format is documented in [docs/project-format.md](docs/project-format.md).

External JSON documents are size- and structure-bounded before they can create editor models. FFprobe payloads, FFmpeg diagnostics, progress lines, and export-worker messages are bounded as well; diagnostic tails retain the newest useful output. If automatic recovery storage fails, the editor shows a persistent manual-save warning and retries safely after a backoff. A requested recovery discard is durable before cleanup: a matching residual snapshot is suppressed at the next startup, while newer or different-document recovery remains available.

Module details are in [docs/architecture.md](docs/architecture.md). The project source lives in [`native/src/project`](native/src/project).

## Telemetry semantics

Telemetry has strict no-data semantics: missing is distinct from a measured numeric zero, public raw lookup never returns `NaN` or infinity, and raw analysis does not bridge missing samples or real timestamp gaps. Preview/export overlays use a separate bounded hold and lightweight smoothing policy to remain stable between ordinary samples without changing raw telemetry. See [docs/telemetry-semantics.md](docs/telemetry-semantics.md).

## Export

Export stages a frame-cadenced telemetry overlay, converts the source onto that same CFR cadence before composition, and validates the staged overlay and final MP4 before committing the target file. Qt Quick offscreen pixels retain an explicit premultiplied-alpha contract through QRhi readback and FFV1 staging. Before 10-bit YUV composition, Stage B explicitly unpremultiplies that overlay and uses straight-alpha blending so transparent RGB-to-YUV chroma offsets cannot alter the source. Source raster and bit depth remain authoritative rather than being reduced to presets; HDR/Log, non-zero rotation metadata, and non-square sample-aspect-ratio sources are explicitly rejected only at export preflight until their display transforms can be preserved. Import, probe, playback, and editing remain available. Each prepared export writes a separate support log in the app-data `exports` directory; failures and cancellations keep their logs. See [docs/export-pipeline.md](docs/export-pipeline.md), [docs/media-color-policy.md](docs/media-color-policy.md), and [docs/export-output-safety.md](docs/export-output-safety.md).

## Private integration tests

Private recordings are ignored by Git and can be supplied through environment variables:

```bash
FLAPPEDEAR_REAL_GOPRO=/path/to/video.mp4 \
FLAPPEDEAR_REAL_VBO=/path/to/session.vbo \
  ./build-native/native/tests/flappedear_native_tests
```

A private RaceChrono fixture has been validated with 32,718 samples, 49 channels, 3,271.7 seconds, and zero parser warnings. An August 2026 macOS regression run on one private GoPro/VBO pair restored 1,536-packet / 14,796-sample GPS extraction and +90.217 s auto-sync at 0.999575 correlation. The exact 3840×2160, `60000/1001`, 30→90 HEVC/AAC export also passed with 3,597 final video packets, alongside short 1080p59.94 and 720p29.97 checks. These are development results, not cross-platform performance guarantees.

## Current limitations

- Windows runtime/export validation currently covers one known Windows 11 / Qt 6.11 / MSVC 2022 / Intel Iris Plus / Quick Sync configuration, not a broad hardware matrix.
- Packaging and signing are pending.
- A source currently contains one video file; multi-chapter timelines are not implemented.
- Export requires external FFmpeg at runtime.
- Rotation and sample-aspect-ratio display-transform preservation, HDR/Log color-managed preservation, and production 8K validation remain pending. One real HERO11 5312×2988 10-bit SDR fixture has passed native and 3840×2160 macOS exports; this is not a broader hardware guarantee.
- Real-media coverage remains limited.
- Interactive map tiles are pending; the local GPS track view works without map tiles.

For remaining work, see [ROADMAP.md](ROADMAP.md). Developer contribution rules are in [AGENTS.md](AGENTS.md), and local test guidance is in [docs/testing.md](docs/testing.md).
