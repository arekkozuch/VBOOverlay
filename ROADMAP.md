# FlappedEar Telemetry roadmap

This roadmap tracks remaining work. It is not a record of completed implementation history.

## Completed foundation

- [x] Native Qt 6/C++/QML application and local Qt Test target.
- [x] VBO parsing, time-based telemetry lookup, synchronization, GoPro GPMF GPS extraction, and GPS-speed auto-sync.
- [x] Widget editor, templates, projects, analysis workspace, and shared preview/export telemetry scene, including minimum-size-reachable sidebars and synchronized keyboard/full-screen transport.
- [x] HEVC/AAC export with custom source ranges, staged overlay validation, diagnostics, progress, and cancellation.
- [x] Export output transactions, including explicit state-bound overwrite consent, changed-target refusal, and protected user targets.
- [x] Atomic project saving and dirty-state safeguards for destructive project actions.
- [x] Separate atomic unsaved-document recovery from authoritative saved projects, including explicit startup recovery/discard and unknown-field preservation.
- [x] Portable project-relative video/VBO references, bounded source fingerprints, document-first opening with missing assets, explicit relinking, and mismatch confirmation.
- [x] Asynchronous video/VBO loading, transactional document commit, and stale-result rejection.
- [x] Cooperative cancellation and defensive resource bounds for VBO parsing, GoPro probing/GPMF decoding, auto-sync, source replacement, and shutdown.
- [x] Resource-bound project/template/recovery/manifest JSON, FFmpeg/ffprobe diagnostics and progress, asynchronous project parsing, and retriable visible recovery-persistence degradation.
- [x] Version recovery metadata so a snapshot left behind after a successful save but failed physical deletion is provably stale on the next launch.
- [x] Persist a revision-bounded recovery-discard intent before destructive cleanup so Quit, New, Open, and startup Discard cannot resurrect the explicitly discarded snapshot after a deletion failure.
- [x] Monotonic/rollover-safe VBO timestamps and explicit missing-data semantics.
- [x] Stable no-data-aware overlay presentation and gap/extrema-preserving analysis decimation.
- [x] Cached static track geometry and time-independent track-marker rendering.
- [x] Windows compilation validation.
- [x] Windows runtime/export validation on one Windows 11 / Qt 6.11 / MSVC 2022 / Intel Iris Plus / Quick Sync configuration.

## Correctness / release hardening

- [x] Enforce final CFR at the effective rational export rate, including deterministic VFR-to-CFR, non-zero range, and audio-timeline coverage.
- [ ] Expand final-media validation across a broader real-media matrix.
- [x] Make source/native raster characteristics authoritative, with no product-level 4K ceiling, continuous bitrate scaling, checked frame accounting, and runtime renderer/encoder capability preflight.
- [x] Preserve 8-bit and 10-bit SDR through explicit HEVC Main/Main10 policy and deterministic composition validation.
- [x] Establish explicit QRhi-to-FFmpeg premultiplied-alpha composition, including conditional framebuffer-Y normalization and pixel-level regression coverage.
- [x] Preserve full-range BT.709 color values through 10-bit YUV overlay composition by explicitly unpremultiplying staged BGRA before straight-alpha blending; validate decoded RGB through VideoToolbox and a real 4K60 export.
- [x] Validate one real HERO11 5.3K Main10/BT.709 fixture at native 5312×2988 and 3840×2160 on macOS/Metal/VideoToolbox; audio validation now accepts a legitimately shorter source-audio timeline.
- [ ] Implement and validate color-managed HDR/HLG/PQ/Log preservation; current export rejects these sources without silent conversion.
- [ ] Validate production 8K on representative renderer/encoder hardware; deterministic model and capability-decision coverage is complete.
- [ ] Preserve rotation and sample-aspect-ratio display transforms end to end (probe/model retention is complete; export currently fails fast for non-zero rotation or non-square SAR rather than applying an implicit transform).
- [x] Establish process-tree termination guarantees for FFmpeg/ffprobe (macOS/Unix runtime-tested; Windows validated on the known configuration above).
- [x] Add disk-space preflight and manifest-owned temporary-file management policy, including representative FFV1 sampling.
- [ ] Validate the QML preview/export result against broader real media.
- [ ] Broaden Windows GPU/encoder and installed-dependency runtime coverage beyond the known configuration.
- [ ] Validate heavy 4K export GUI responsiveness on Windows.
- [ ] Add native Windows ACL-denied filesystem coverage and validate multi-instance export-log safety.
- [ ] Define the packaging, signing, and release gate.

Development validation includes a successful private 3840×2160, `60000/1001`, 30→90 HEVC/AAC export on macOS with 3,597 final packets; native and 3840×2160 exports of one 5312×2988 HERO11 Main10/BT.709 fixture (442 final packets each); a separate 5.855-second 3840×2160 Main10/full-range BT.709 color-fidelity export with the production nine-widget overlay (351 packets); plus restored real GoPro/VBO auto-sync at +90.217 s and 0.999575 correlation. It does not replace wider real-media or Windows runtime validation.

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
