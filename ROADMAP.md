# FlappedEar Telemetry roadmap

This file tracks agreed product work. It is intentionally separate from implementation notes.

## Production platform: native desktop

The application uses **Qt 6 with C++ and QML** so macOS and Windows share one native codebase. The
earlier Electron prototype has been removed from the repository.

Migration requirements:

- Preserve VBO parsing, time-based interpolation, synchronization mathematics, and test fixtures as
  behavioral specifications.
- Use Qt Multimedia for preview and FFmpeg/FFprobe for media inspection and HEVC export where Qt's
  platform APIs are insufficient.
- Render preview and export from the same native scene definitions.
- Produce a macOS `.app` and a portable Windows x64 folder before adding installers.
- Measure clean-install size, startup time, idle memory, scrubbing latency, and 4K/60 playback on
  both target platforms.
- Use a clean, versioned native `.fetproject` schema; pre-release prototype compatibility is not a
  requirement.

Current implementation status:

- [x] Qt 6/CMake application and test targets.
- [x] Time-based telemetry session, interpolation, sync transform, and VBO parser port.
- [x] Supplied real VBO integration test.
- [x] Native video/VBO shell, source restoration, window settings, timeline, and fullscreen.
- [x] Configurable native widget scene, v2 project schema, and shareable JSON layout templates.
- [x] Persistent custom templates with import/export and multiple built-in use-case layouts.
- [x] Per-widget timed visibility cues with fade and entrance effects.
- [x] GoPro GPS5/GPS9 GPMF extraction and automatic GPS-speed synchronization port.
- [x] Removal of the obsolete Electron/React prototype and Node build chain.
- [x] Custom telemetry branding and native macOS/Windows application icon assets.
- [ ] HEVC export, progress, cancellation, and output validation.
- [ ] Windows build and runtime validation.
- [ ] Repeatable self-contained packaging below the agreed size budget; preliminary macOS dependency
      deployment measured 126 MB before trimming.

## To do

### 1. Synchronized telemetry analysis workspace

Build one analysis workspace that is docked below the editor by default and can be detached into a
separate window. Both presentations must use the same models and playback clock, so switching
between them never creates a second synchronization path.

Foundation delivered:

- [x] Docked multi-channel chart panel using the shared synchronized playhead.
- [x] Full-resolution live values with bounded plot sampling for long sessions.
- [x] Chart scrubbing, compact track position, channel selection, and project/session persistence.
- [x] Detachable analysis window with independently resizable video, track, and chart panes.
- [ ] Chart zoom/range selection, annotations, and configurable axes.

- Add a collapsible bottom analysis panel with a detachable-window action and persistent panel/window
  geometry.
- Keep the main preview, compact analysis video, track map, charts, timeline, and numeric cursor values
  driven by one shared playback time and video-to-telemetry transform.
- Display a compact video preview, compact track map with the current position, and one or more stacked
  telemetry charts in the detached layout.
- Allow any recorded channel to be added to or removed from a chart, with editable label, unit, color,
  axis range, line style, and grouping by compatible axes.
- Support a shared playhead, hover values, chart-to-video scrubbing, zooming, panning, and selection of a
  time range without breaking normal timeline scrubbing.
- Add markers and annotations that can later be reused as widget visibility cues and export ranges.
- Downsample only the plotted geometry for long sessions while retaining full-resolution values at the
  playhead and during export.
- Persist selected channels, chart arrangement, zoom range, map visibility, and whether the workspace is
  docked or detached in the project.
- Add deterministic synchronization, chart-range, decimation, and project round-trip tests, plus a
  performance check using the supplied real VBO session.

### 2. Video export

Export finished clips from the same native scene definitions used by the preview. Rendering must be
timestamp-driven so variable-frame-rate sources and telemetry remain synchronized.

- Add an export dialog for output path, resolution, frame rate, codec, bitrate/quality, audio, and export
  range, with sensible presets for video editors and direct sharing.
- Render every output frame at its presentation timestamp through the same widget layout, cue, animation,
  and interpolation logic as the editor preview.
- Start with H.264, HEVC, and ProRes where the platform FFmpeg build supports them; detect hardware
  encoders and keep a reliable software fallback.
- Preserve audio timing by remuxing compatible audio or re-encoding when required, including sources with
  non-zero start timestamps.
- Show frame-accurate progress, elapsed/remaining time, current stage, cancellation, and actionable encoder
  errors; clean partial and temporary files after cancellation or failure.
- Validate the finished duration, dimensions, frame cadence, audio stream, and final timestamp before
  reporting success.
- Add an editor-oriented transparent-overlay follow-up (ProRes 4444 or PNG sequence) so telemetry can be
  composited separately from the source video.
- Add timing and cancellation unit tests plus short deterministic integration exports before validating a
  full supplied GoPro clip.

### Multiple GoPro video chapters

Support recordings split by the camera into multiple MP4/MOV chunks as one continuous source.

- Allow selecting multiple videos and adding/removing/reordering chunks.
- Detect likely GoPro chapters from filenames and embedded creation/chapter metadata, while allowing
  manual ordering.
- Validate resolution, codec, frame rate, time base, audio, and telemetry compatibility and show
  actionable warnings for mismatches.
- Build a virtual media timeline from cumulative clip durations so playback and scrubbing cross clip
  boundaries without resetting telemetry.
- Stitch each clip's GPMF telemetry into the same continuous time domain, accounting for gaps,
  overlaps, and missing telemetry.
- Synchronize the combined video timeline to one VBO session; do not create independent widget
  timing logic per clip.
- Store the ordered clip list in `.fetproject` while continuing to open existing single-video
  projects.
- Export all chapters as one HEVC video with continuous overlays and preserved audio.
- Add deterministic unit tests for boundary lookup and timing, plus integration tests with real GoPro
  chapter files when samples are available.

### Native feature parity

- Video/VBO import and automatic source restoration.
- Accurate resizing, playback, frame-independent scrubbing, and fullscreen preview.
- Automatic and manual telemetry synchronization.
- Configurable widgets and live telemetry panel with persistent layouts.
- Native menus, shortcuts, dialogs, recent projects, and window-state persistence.
- Timing-correct HEVC export with audio, progress, and cancellation.
- Track/map rendering and offline-safe export behavior.

## Deferred distribution work

- macOS Developer ID signing and notarization.
- Windows code signing.
- Installers and automatic updates.
