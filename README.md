# FlappedEar Telemetry

FlappedEar Telemetry is a native desktop editor for synchronizing motorsport telemetry with video and rendering telemetry overlays.

## Status

The application is a Qt 6, C++20, and QML native application in alpha and active development. Development has been validated on macOS. Windows runtime/export has been validated on one Windows 11 / Qt 6.11 / MSVC 2022 / Intel Iris Plus / Quick Sync configuration; broader hardware and packaging validation remain pending.

## Current capabilities

- MP4/MOV playback with timeline controls and preview overlays.
- RaceChrono and VBOX VBO telemetry import.
- GoPro GPMF GPS extraction and GPS-speed auto synchronization.
- A visual widget editor, projects, built-in layouts, and shareable templates.
- Lazily loaded synchronized telemetry analysis, including charts and a track view; its secondary decoder exists only while the Analysis window is open.
- Source-driven CFR HEVC/AAC MP4 export at the effective rational export rate, including runtime raster/profile checks, validated 8-bit and 10-bit SDR preservation, optional custom source ranges, progress, cancellation, and verbose diagnostics retained in a durable per-export log.
- Portable `.fetproject` media references with project-relative lookup, bounded source fingerprints, missing-media recovery, explicit relinking, and stale asynchronous-result rejection.
- Crash-safe export-output handling with state-bound overwrite consent, atomic project saving, and explicit unsaved-change recovery.

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

A saved `.fetproject` is the authoritative clean document. It can open without its external video or VBO assets; missing or mismatched sources remain independently relinkable without losing the scene or settings. Unsaved persistent edits are held separately in an atomic recovery snapshot and are recovered or discarded explicitly at startup; QSettings stores only application preferences and the last project path. The portable source format is documented in [docs/project-format.md](docs/project-format.md).

Module details are in [docs/architecture.md](docs/architecture.md). The project source lives in [`native/src/project`](native/src/project).

## Telemetry semantics

Telemetry has strict no-data semantics: missing is distinct from a measured numeric zero, public raw lookup never returns `NaN` or infinity, and raw analysis does not bridge missing samples or real timestamp gaps. Preview/export overlays use a separate bounded hold and lightweight smoothing policy to remain stable between ordinary samples without changing raw telemetry. See [docs/telemetry-semantics.md](docs/telemetry-semantics.md).

## Export

Export stages a frame-cadenced telemetry overlay, converts the source onto that same CFR cadence before composition, and validates the staged overlay and final MP4 before committing the target file. Qt Quick offscreen pixels retain an explicit premultiplied-alpha contract through QRhi readback, FFV1 staging, and FFmpeg composition. Source raster and bit depth remain authoritative rather than being reduced to presets; HDR/Log is explicitly rejected until a color-managed compositor is validated. Each prepared export writes a separate support log in the app-data `exports` directory; failures and cancellations keep their logs. See [docs/export-pipeline.md](docs/export-pipeline.md), [docs/media-color-policy.md](docs/media-color-policy.md), and [docs/export-output-safety.md](docs/export-output-safety.md).

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
