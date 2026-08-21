# FlappedEar Telemetry

FlappedEar Telemetry is a native desktop editor for synchronizing motorsport telemetry with video and rendering telemetry overlays.

## Status

The application is a Qt 6, C++20, and QML native application in alpha and active development. Development has been validated on macOS, and Windows compilation has been validated. Windows runtime and package validation are still pending.

## Current capabilities

- MP4/MOV playback with timeline controls and preview overlays.
- RaceChrono and VBOX VBO telemetry import.
- GoPro GPMF GPS extraction and GPS-speed auto synchronization.
- A visual widget editor, projects, built-in layouts, and shareable templates.
- Synchronized telemetry analysis, including charts and a track view.
- HEVC/AAC MP4 export, optional custom source ranges, progress, cancellation, and verbose diagnostics.
- Asynchronous video/VBO loading, transactional project loading, and stale asynchronous-result rejection.
- Crash-safe export-output handling and atomic project saving.

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

The application keeps telemetry parsing, synchronization, video/media handling, widgets, and QML presentation separate. Preview and export mount the same `TelemetryScene.qml` with independent render contexts.

Module details are in [docs/architecture.md](docs/architecture.md). The project source lives in [`native/src/project`](native/src/project).

## Telemetry semantics

Telemetry has strict no-data semantics: public lookup never returns `NaN` or infinity, and it does not bridge missing samples or values outside a channel's range. See [docs/telemetry-semantics.md](docs/telemetry-semantics.md).

## Export

Export stages a frame-cadenced telemetry overlay before timestamp-driven final composition, and it validates the staged overlay and final MP4 before committing the target file. See [docs/export-pipeline.md](docs/export-pipeline.md) and [docs/export-output-safety.md](docs/export-output-safety.md).

## Private integration tests

Private recordings are ignored by Git and can be supplied through environment variables:

```bash
FLAPPEDEAR_REAL_GOPRO=/path/to/video.mp4 \
FLAPPEDEAR_REAL_VBO=/path/to/session.vbo \
  ./build-native/native/tests/flappedear_native_tests
```

A private RaceChrono fixture has been validated with 32,718 samples, 49 channels, 3,271.7 seconds, and zero parser warnings. Development validation has also completed a real non-zero-range 4K, approximately 59.94 fps HEVC/AAC export on macOS through the private GoPro/VBO workflow. This is a development result, not a cross-platform performance guarantee.

## Current limitations

- Final CFR enforcement and VFR-output validation are incomplete; VFR-looking inputs are warned about.
- Windows runtime validation is pending.
- Packaging and signing are pending.
- A source currently contains one video file; multi-chapter timelines are not implemented.
- Export requires external FFmpeg at runtime.
- Rotation, sample-aspect-ratio, color, HDR, and 10-bit media handling have not been fully validated.
- Real-media coverage remains limited.
- Interactive map tiles are pending; the local GPS track view works without map tiles.

For remaining work, see [ROADMAP.md](ROADMAP.md). Developer contribution rules are in [AGENTS.md](AGENTS.md), and local test guidance is in [docs/testing.md](docs/testing.md).
