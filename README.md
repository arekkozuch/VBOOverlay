# FlappedEar Telemetry

FlappedEar Telemetry is an early cross-platform Electron editor for placing time-synchronized motorsport telemetry over video. Version 0.1 currently provides secure MP4/MOV and VBO import, FFprobe media inspection, timeline scrubbing, manual synchronization, live telemetry values, draggable/scalable widgets, track geometry, and versioned project files.

## Architecture

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

## VBO support

The parser discovers sections, metadata, columns, numeric channels, missing values, clock-style or elapsed timestamps, and aliases for speed, RPM, pedals, heart rate, GPS, and acceleration. Data is loaded once into typed arrays; timeline lookup uses binary search with `previous`, `nearest`, and `linear` modes. Time is always expressed in seconds, never frames.

Place a real `.vbo` in `samples/` or the repository root to enable the conditional integration test. The supplied RaceChrono file was validated with 32,718 samples, 49 numeric channels, 3,271.7 seconds of telemetry, all primary semantic aliases, signed arc-minute GPS conversion, duplicate-column preservation, and zero parser warnings. Real recordings remain ignored by Git.

## Video, GoPro, and synchronization

FFprobe reports source duration, streams, resolution, codec, pixel format, frame rates, time base, audio, metadata streams, and likely variable frame rate. GoPro GPMF metadata is detected without preventing ordinary MP4/MOV import. The main process reads only indexed `gpmd` packet ranges—never the entire multi-gigabyte video—then converts GPS, accelerometer, and gyroscope data into typed channels through the maintained `gopro-telemetry` parser.

The independent GPS-speed sync strategy performs a full-session 1 Hz search followed by 10 Hz refinement, using normalized cross-correlation. It reports confidence from peak strength, uniqueness, duration, and sample count. The UI shows the candidate before applying it; manual correction remains available. Drift is deliberately left at `timeScale = 1` until it can be estimated reliably.

The supplied 11,526,059,397-byte GoPro recording was validated as 3840×2160 HEVC at 59.94 fps with AAC audio and 1,536 GPMF packets. Integration extracted 15,374 GPS samples, retained 14,796 with a valid fix, and extracted 309,862 samples per accelerometer/gyroscope axis. It synchronized the recording to the supplied VBO at approximately `+90.2 s` with correlation above `0.97`. Both recordings are ignored by Git.

## Widgets and maps

The shared canvas renderer implements Speed (km/h or mph), RPM, VBO Heart Rate, real throttle/brake bars, 2D G-force, and a projected track outline with moving position. Widgets can be added, selected, dragged, scaled, hidden, and deleted.

A MapLibre-compatible provider abstraction supports no map, configurable XYZ raster sources, and style URLs. The public OpenStreetMap adapter is intended only for light interactive use; bulk export must use caching and a suitable provider. Remote-map rendering is not enabled in the current editor.

## H.265 export

Runtime detection prefers only encoders actually reported by FFmpeg (`hevc_videotoolbox`, `hevc_nvenc`, `hevc_qsv`, `hevc_amf`, then `libx265`). The current machine reports VideoToolbox and x265. The frame-streaming overlay/composite pipeline and export progress UI remain pending, so the Export button is intentionally disabled; the application does not claim working export yet.

## Project files

`.fetproject` files are versioned JSON containing source paths, sync transform, widget scene, map settings, and export settings. Opening a project restores its settings; source files must currently be reopened through the secure dialogs before previewing.

## Current limitations and roadmap

- HEVC streaming export, audio preservation, progress, and cancellation are pending.
- MapLibre map display and tile caching are pending; track-outline rendering works offline.
- VFR is detected and shown in the model, but a timing-safe VFR export path is not implemented.
- Packaging/signing installers for macOS and Windows is not yet configured.

Next work should prioritize timing-correct HEVC export before appearance or roadmap features.
