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
- [ ] HEVC export, progress, cancellation, and output validation.
- [ ] Windows build and runtime validation.
- [ ] Repeatable self-contained packaging below the agreed size budget; preliminary macOS dependency
      deployment measured 126 MB before trimming.

## To do

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

- Custom application icon and platform branding.
- macOS Developer ID signing and notarization.
- Windows code signing.
- Installers and automatic updates.
