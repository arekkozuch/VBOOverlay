# FlappedEar Telemetry

FlappedEar Telemetry is a motorsport telemetry overlay editor. The current Electron version is a functional prototype and behavior reference; production development is moving to a Qt 6/C++/QML native application for macOS and Windows. See [ROADMAP.md](ROADMAP.md) for the migration and agreed feature work, including support for GoPro recordings split into multiple video chapters.

The prototype provides secure MP4/MOV and VBO import, FFprobe media inspection, timeline scrubbing, manual synchronization, configurable live telemetry values, draggable/scalable widgets, track geometry, autosaved editor preferences, and versioned project files.

## Native rewrite

The production implementation is under `native/` and uses Qt 6, C++20, QML, Qt Multimedia, CMake,
and Qt Test. It currently includes:

- a native macOS/Windows application target with native menus and dialogs;
- Qt Multimedia video playback, aspect-correct overlay placement, timeline scrubbing, audio, and
  fullscreen mode;
- the ported typed telemetry session, central video-to-telemetry transform, binary-search
  interpolation, and dynamic VBO parser;
- persisted video/VBO sources, window state, offset, and time scale through native platform settings;
- live display of every numeric VBO channel plus initial Speed, RPM, and Heart Rate overlays.
- a persistent, `.fetproject`-compatible widget scene with add, select, drag, resize, rotate,
  duplicate, hide, delete, style, and per-channel binding controls;
- native Speed, RPM, Heart Rate, Pedals, G-Force, Custom Value, GPS track, arc gauge,
  analog dial gauge, and four-channel telemetry overlay widgets;
- bounded native GoPro GPMF packet reads, GPS9/GPS5 decoding, and background GPS-speed auto sync
  with persisted offset and confidence diagnostics.
- a fully custom Qt Quick design system and three-pane editor rather than platform-default Qt
  controls;
- comprehensive per-widget data, formatting, geometry, typography, color, range, and type-specific
  controls;
- data-defined widget archetypes and nine built-in scenes—Track Day, Minimal, Broadcast, Performance,
  Circuit Pro, Endurance, Drag Strip, Clean HUD, and a late-2000s-inspired Grand Prix layout—in
  `native/resources/widget-templates.json`;
- custom layout capture plus `.fettemplate` import/export and deletion. User templates persist in
  the platform application-data directory and retain widget geometry, channel bindings, styling,
  and animation cues;
- per-widget timed appearances with multiple cues, playhead-based placement, fade-in/out timing,
  and Fade, Pop, or Slide Up entrance effects for broadcast-style inserts;
- a focused welcome screen for selecting the clip, optional VBO, or a saved project before entering
  the editor; development restores can still continue directly into the studio.

Build and test on macOS:

```bash
cmake -S . -B build-native -DCMAKE_BUILD_TYPE=Debug -DCMAKE_PREFIX_PATH=/opt/homebrew/opt/qt
cmake --build build-native --parallel
ctest --test-dir build-native --output-on-failure
open "build-native/native/FlappedEar Telemetry.app"
```

Run the compatibility suite against a private real VBO without adding its path to source control:

```bash
FLAPPEDEAR_REAL_GOPRO=/absolute/path/video.mp4 \
FLAPPEDEAR_REAL_VBO=/absolute/path/session.vbo \
  ./build-native/native/tests/flappedear_native_tests
```

The development `.app` is approximately 712 KB and links to the installed Qt development runtime. A
preliminary dependency-deployment measurement was 126 MB with Qt 6.11 multimedia and QML included,
compared with approximately 328 MB for the Electron prototype. A repeatable, trimmed self-contained
package is not complete yet; release optimization, signing, and notarization remain pending.

## Electron prototype architecture

- `src/main`: Electron lifecycle, constrained file dialogs, FFprobe integration, encoder detection, and project persistence.
- `src/preload`: the small typed IPC bridge; no generic filesystem or shell access is exposed.
- `src/telemetry`: UI-independent VBO parsing, typed-array sessions, binary-search interpolation, GPS geometry, and synchronization strategies.
- `src/renderer`: React editor and shared canvas widget scene renderer.
- `src/shared`: serializable domain and IPC contracts.
- `tests`: synthetic unit fixtures and a conditional real-VBO integration test.

Electron uses `contextIsolation`, disables Node integration, enables renderer sandboxing, and serves user-selected video through a tokenized custom protocol. Widgets consume semantic aliases while the session retains every original VBO column name.

## Requirements and setup

- macOS or Windows
- Node.js 25+ and npm 11+ (the current development environment uses Node 25.6.1)
- FFmpeg and FFprobe available on `PATH`

macOS with Homebrew, if FFmpeg is missing:

```bash
brew install ffmpeg
```

Windows with winget, if FFmpeg is missing:

```powershell
winget install Gyan.FFmpeg
```

Install and run:

```bash
npm install
npm run dev
```

Validation:

```bash
npm run lint
npm run typecheck
npm test
npm run build
```

Desktop packages:

```bash
# macOS .app bundle in release/mac*/
npm run package:mac

# Portable Windows application folder in release/win-unpacked/
npm run package:win
```

The Windows target intentionally produces an unpacked portable folder rather than an installer. Build Windows releases on Windows for the most reliable result. Neither package includes source GoPro, VBO, RCZ, or project recordings. FFmpeg and FFprobe must remain available on the target machine's `PATH`.

## VBO support

The parser discovers sections, metadata, columns, numeric channels, missing values, clock-style or elapsed timestamps, and aliases for speed, RPM, pedals, heart rate, GPS, and acceleration. Data is loaded once into typed arrays; timeline lookup uses binary search with `previous`, `nearest`, and `linear` modes. Time is always expressed in seconds, never frames.

Place a real `.vbo` in `samples/` or the repository root to enable the conditional integration test. The supplied RaceChrono file was validated with 32,718 samples, 49 numeric channels, 3,271.7 seconds of telemetry, all primary semantic aliases, signed arc-minute GPS conversion, duplicate-column preservation, and zero parser warnings. Real recordings remain ignored by Git.

## Video, GoPro, and synchronization

FFprobe reports source duration, streams, resolution, codec, pixel format, frame rates, time base, audio, metadata streams, and likely variable frame rate. GoPro GPMF metadata is detected without preventing ordinary MP4/MOV import. The main process reads only indexed `gpmd` packet ranges—never the entire multi-gigabyte video—then converts GPS, accelerometer, and gyroscope data into typed channels through the maintained `gopro-telemetry` parser.

The independent GPS-speed sync strategy performs a full-session 1 Hz search followed by 10 Hz refinement, using normalized cross-correlation. It reports confidence from peak strength, uniqueness, duration, and sample count. The UI shows the candidate before applying it; manual correction remains available. Drift is deliberately left at `timeScale = 1` until it can be estimated reliably.

The supplied 11,526,059,397-byte GoPro recording was validated as 3840×2160 HEVC at 59.94 fps with AAC audio and 1,536 GPMF packets. Integration extracted 15,374 GPS samples, retained 14,796 with a valid fix, and extracted 309,862 samples per accelerometer/gyroscope axis. It synchronized the recording to the supplied VBO at approximately `+90.2 s` with correlation above `0.97`. Both recordings are ignored by Git.

## Widgets and maps

The shared canvas renderer implements Speed (km/h or mph), RPM, VBO Heart Rate, real accelerator/brake bars with numeric values, 2D G-force with resultant magnitude, a projected track outline with moving position, and a Custom Value widget. Every value widget can bind to an original numeric VBO column; custom values also support editable labels, suffixes, precision, and multipliers. Widgets can be added, selected, dragged, resized, rotated, duplicated, styled, hidden, and deleted. Background strength, panel/text/accent colors, opacity, and optional titles are configurable. The live telemetry panel can contain any number of recorded numeric channels with independent labels, units, and precision. The preview, transport, and timeline can be expanded into fullscreen mode.

A MapLibre-compatible provider abstraction supports no map, configurable XYZ raster sources, and style URLs. The public OpenStreetMap adapter is intended only for light interactive use; bulk export must use caching and a suitable provider. Remote-map rendering is not enabled in the current editor.

## H.265 export

Runtime detection prefers only encoders actually reported by FFmpeg (`hevc_videotoolbox`, `hevc_nvenc`, `hevc_qsv`, `hevc_amf`, then `libx265`). The current machine reports VideoToolbox and x265. The frame-streaming overlay/composite pipeline and export progress UI remain pending, so the Export button is intentionally disabled; the application does not claim working export yet.

## Project files

Native `.fetproject` files use the clean v2 JSON schema and contain source paths, sync transform,
widget scene (including animation cues), map settings, and export settings. Widget layouts, styling,
sync, and source choices are also autosaved locally, so they survive an app restart. Pre-release
Electron/native-v1 layout compatibility is intentionally not retained because the application has
not yet shipped.

## Current limitations and roadmap

- The current Electron package is not the production distribution architecture; native Qt migration
  is planned in `ROADMAP.md`.
- A video source currently contains only one file. Multi-chapter GoPro recordings are an explicit
  native-roadmap item.
- HEVC streaming export, audio preservation, progress, and cancellation are pending.
- MapLibre map display and tile caching are pending; track-outline rendering works offline.
- VFR is detected and shown in the model, but a timing-safe VFR export path is not implemented.
- Packages are currently unsigned. macOS Gatekeeper distribution and code signing are not configured.

Next native milestones are timing-correct HEVC export and the multi-chapter GoPro timeline described
in `ROADMAP.md`.
