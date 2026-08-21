# FlappedEar Telemetry

FlappedEar Telemetry is a native motorsport telemetry overlay editor for macOS and Windows. It uses
Qt 6, C++20, QML, Qt Multimedia, CMake, and Qt Test; the earlier Electron/React prototype has been
removed.

## Current capabilities

- MP4/MOV playback with aspect-correct overlays, audio, scrubbing, and fullscreen preview.
- Dynamic VBOX `.vbo` parsing with time-based interpolation and access to every numeric channel.
- Native GoPro GPS5/GPS9 GPMF extraction and background GPS-speed synchronization.
- Persistent `.fetproject` scenes, source restoration, window state, sync offset, and time scale.
- Configurable Speed, RPM, Heart Rate, Pedals, G-Force, GPS Track, Custom Value, arc gauge, dial
  gauge, data-strip, and retro broadcast widgets.
- Widget positioning, resizing, rotation, duplication, visibility, grouping, multi-selection, and
  Delete/Backspace removal.
- Multiple timed appearance cues per widget with Fade, Pop, and Slide Up effects.
- Nine built-in layouts plus persistent custom templates and `.fettemplate` import/export.
- File → Export produces H.265/HEVC MP4 clips from the same telemetry scene as preview, with AAC
  audio when present, source-resolution/source-CFR output, custom ranges, progress, cancellation,
  and post-export media validation.
- A custom Qt Quick design system rather than platform-default Qt controls.

## Requirements

- CMake 3.24 or newer.
- A C++20 compiler.
- Qt 6.8 or newer with Concurrent, Core, Gui, Quick, Quick Controls 2, Multimedia, and Test.
- macOS or Windows for the intended desktop targets.
- FFmpeg and FFprobe on `PATH` (or Homebrew's `/opt/homebrew/bin` or `/usr/local/bin` on macOS) for
  GoPro indexing and export. A working HEVC encoder is required for export.

On Apple Silicon with Homebrew Qt installed in `/opt/homebrew/opt/qt`:

```bash
cmake -S . -B build-native \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_PREFIX_PATH=/opt/homebrew/opt/qt
cmake --build build-native --parallel
ctest --test-dir build-native --output-on-failure
open "build-native/native/FlappedEar Telemetry.app"
```

## Architecture

- `native/src/telemetry`: typed telemetry sessions, interpolation, track geometry, and VBO parsing.
- `native/src/gopro`: bounded MP4/GPMF packet discovery and GPS telemetry decoding.
- `native/src/sync`: centralized time-domain synchronization.
- `native/src/widgets`: persistent widget, group, animation-cue, and template model.
- `native/src/export`: FFmpeg/FFprobe discovery, media probing, working-encoder detection,
  timestamp-driven telemetry frames, HEVC compositing, and output validation.
- `native/src/app`: application state, projects, source restoration, and QML-facing controller.
- `native/qml`: native editor, inspector, controls, and preview renderer.
- `native/resources`: shareable built-in layout definitions.
- `native/tests`: Qt Test coverage for parsing, synchronization, widgets, templates, groups, cues,
  track geometry, and GPMF decoding.
- `native/tests/fixtures`: small deterministic VBO input used by the native test target.

Preview and export consume `TelemetryScene.qml` through independent `TelemetryRenderContext` objects.
All synchronization is expressed in seconds rather than frames. A custom range remains on the source
timeline: exporting 120–140 seconds renders its first telemetry frame at source time 120 seconds.

## Private integration tests

Real recordings are ignored by Git. They can be supplied explicitly without entering source
control:

```bash
FLAPPEDEAR_REAL_GOPRO=/absolute/path/video.mp4 \
FLAPPEDEAR_REAL_VBO=/absolute/path/session.vbo \
  ./build-native/native/tests/flappedear_native_tests
```

The supplied private RaceChrono file was validated with 32,718 samples, 49 numeric channels, and
3,271.7 seconds of telemetry. The private 11,526,059,397-byte GoPro recording yielded 1,536 GPMF
packets and synchronized to the VBO at approximately `+90.2 s` with correlation above `0.97`.
Neither recording is part of the repository.

## Projects and templates

Native `.fetproject` files use the v2 JSON schema and contain source paths, synchronization,
widgets, groups, animation cues, map settings, and export settings. Editor state is also autosaved
locally.

Built-in layouts live in `native/resources/widget-templates.json`. Users can capture the current
scene as a persistent custom template and share it as a `.fettemplate` file.

## Current limitations

- A source currently contains one video file; continuous multi-chapter GoPro support is planned.
- Export is explicit-CFR. Likely variable-frame-rate sources are warned about in the export dialog;
  native timestamp-preserving VFR output is not yet validated.
- HEVC export requires a locally working FFmpeg encoder. The detector verifies a small encode before
  selecting an advertised encoder; it does not bundle FFmpeg or an encoder.
- HEVC/AAC export has deterministic synthetic validation. A current real GoPro/VBO export could not
  be run in this workspace because the private sample paths were unavailable.
- Interactive map tiles and offline-safe map export are pending; GPS track outlines work offline.
- Windows source compilation has been confirmed locally, but Windows runtime validation and repeatable
  self-contained packaging remain pending.
- macOS and Windows builds are unsigned.

See [ROADMAP.md](ROADMAP.md) for the agreed remaining work.
