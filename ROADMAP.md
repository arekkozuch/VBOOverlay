# FlappedEar Telemetry roadmap

This roadmap tracks remaining work. It is not a record of completed implementation history.

## Completed foundation

- [x] Native Qt 6/C++/QML application and local Qt Test target.
- [x] VBO parsing, time-based telemetry lookup, synchronization, GoPro GPMF GPS extraction, and GPS-speed auto-sync.
- [x] Widget editor, templates, projects, analysis workspace, and shared preview/export telemetry scene.
- [x] HEVC/AAC export with custom source ranges, staged overlay validation, diagnostics, progress, and cancellation.
- [x] Export output transactions, including explicit overwrite consent and protected user targets.
- [x] Atomic project saving and dirty-state safeguards for destructive project actions.
- [x] Asynchronous video/VBO loading, transactional project loading, and stale-result rejection.
- [x] Monotonic/rollover-safe VBO timestamps and explicit missing-data semantics.
- [x] Windows compilation validation.

## Correctness / release hardening

- [x] Enforce final CFR at the effective rational export rate, including deterministic VFR-to-CFR, non-zero range, and audio-timeline coverage.
- [ ] Expand final-media validation across a broader real-media matrix.
- [ ] Validate rotation, sample aspect ratio, color, HDR, and 10-bit media policy.
- [x] Establish process-tree termination guarantees for FFmpeg/ffprobe (macOS/Unix runtime-tested; Windows compile-only).
- [x] Add disk-space preflight and manifest-owned temporary-file management policy.
- [ ] Validate the QML preview/export result against broader real media.
- [ ] Validate Windows runtime behavior with installed dependencies.
- [ ] Define the packaging, signing, and release gate.

Development validation includes a successful private non-zero-range 4K, approximately 59.94 fps HEVC/AAC export on macOS. It does not replace wider real-media or Windows runtime validation.

## Product work

### Multi-chapter GoPro timelines

- [ ] Model ordered GoPro MP4/MOV chunks as one continuous time-based source.
- [ ] Preserve one telemetry timeline across chapter boundaries, including gaps and overlaps.
- [ ] Validate cross-chapter media compatibility and export a continuous result.

### Analysis and map workflow

- [ ] Add chart zoom, range selection, annotations, and configurable axes.
- [ ] Add interactive map tiles and define offline-safe map export behavior.

### Distribution

- [ ] macOS signing and notarization.
- [ ] Windows code signing.
- [ ] Repeatable self-contained packages, installers, and update strategy.
