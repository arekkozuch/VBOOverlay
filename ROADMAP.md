# Flapped Ear Telemetry roadmap

Updated 12 September 2026. This roadmap tracks completed foundations and remaining work. Current evidence is in [currentstate.md](currentstate.md); the invited-beta gate is [beta acceptance](docs/beta-acceptance.md). Event/multi-run analysis is now requested for the same application, with macOS as the development focus; see the [delivery plan](docs/event-analysis-plan.md).

## Completed foundation

- [x] Native Qt 6/C++/QML application and local Qt Test target.
- [x] VBO parsing, time-based telemetry lookup, synchronization, GoPro GPMF GPS extraction, and GPS-speed auto-sync.
- [x] Widget editor, templates, projects, analysis workspace, and shared preview/export telemetry scene, including minimum-size-reachable sidebars and synchronized keyboard/full-screen transport.
- [x] HEVC/AAC export with custom SMPTE source ranges and single-lap hotlap ranges (5–8 second handles), staged overlay validation, diagnostics, progress, and cancellation.
- [x] Export output transactions, including explicit state-bound overwrite consent, changed-target refusal, and protected user targets.
- [x] Atomic project saving and dirty-state safeguards for destructive project actions.
- [x] Separate atomic unsaved-document recovery from authoritative saved projects, including explicit startup recovery/discard and unknown-field preservation.
- [x] Portable project-relative video/VBO/RCZ references, bounded source fingerprints, document-first opening with missing assets, explicit relinking, and mismatch confirmation.
- [x] Asynchronous video/VBO/RCZ loading, transactional document commit, and stale-result rejection.
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

- [x] Close shipping-review R1–R8: frame arithmetic, global sync confidence, bounded template persistence, GUI recovery ownership, complete interleaved source requests, positive PTS, delayed audio and FFmpeg composition preflight.
- [x] Guard auto-sync results with a timing-edit revision and cancellation; invalidate stale review candidates.
- [ ] Preserve GPS-gap segment boundaries in best-lap reference traces; cover numeric delta and nearby track crossings.
- [ ] Bound VBO line splitting before bulk allocation; validate finite derived timestamps and synchronization work budgets.
- [ ] Terminate Unix descendants when the process-group leader has already exited; verify application cleanup ownership in that case.
- [ ] Correct export-log retention for hyphenated production UUIDs.
- [ ] Validate coordinate-unit ambiguity near the equator/prime meridian across exporters.
- [ ] Validate long final scans, slow destinations and a destination volume filling during Stage B.

These additional audit findings remain open. Their effect on the advertised beta workflow must be resolved or explicitly bounded before approval; a passing CI run does not close them.


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
- [x] Define single-session beta scope, reproducible candidate build procedure and exact-build acceptance record.
- [x] Add Debug/Release platform CI and internal Qt deployment with SDK-isolated startup and archive hashes.
- [ ] Complete clean-machine and real-video acceptance on an identified candidate archive.
- [ ] Finalize distribution notices/source access, signing/notarization, retained artifacts and installation UX.

Development validation includes a successful private 3840×2160, `60000/1001`, 30→90 HEVC/AAC export on macOS with 3,597 final packets; native and 3840×2160 exports of one 5312×2988 HERO11 Main10/BT.709 fixture (442 final packets each); a separate 5.855-second 3840×2160 Main10/full-range BT.709 color-fidelity export with the production nine-widget overlay (351 packets); plus restored real GoPro/VBO auto-sync at +90.217 s and 0.999575 correlation. It does not replace wider real-media or Windows runtime validation.

## Product work

### Event and multi-run analysis (macOS first)

- [x] Native review-only batch import preparation: per-file results, immutable
  telemetry/lap proposals, source provenance, full-content duplicate detection,
  cancellation and aggregate budgets. UI behavior is unchanged.
- [x] Conservative VBO/RCZ GPS-evidence candidates for user review, without
  automatic merging or filename-based recording identity.
- Verification gate: the new import target and existing regressions must pass
  on the exact macOS PR head; [PR #7](https://github.com/arekkozuch/VBOOverlay/pull/7)
  carries current CI status and validation evidence.
- [ ] Persist Event → Run → Lap and source groups with explicit legacy-project
  migration, recovery and relinking behavior.
- [ ] Connect macOS multi-file/folder/drop import and transactional review.
- [ ] Add event overview, compatible event best, run notes and cross-run comparison.
- [ ] Add sectors, theoretical best, progression and evidence-based insights in
  dependency order. See the delivery plan for acceptance gates and non-goals.

### Native RaceChrono RCZ import

Requested and implemented 11 September 2026 for flat, uninterrupted version-1 shared sessions.
See [supported format and validation](docs/rcz-format.md). Resumed sessions, backups and
multiple-session selection remain unsupported and fail explicitly.

- [x] Inspect a representative RCZ session and matching VBO export from the same
  RaceChrono version; record archive/schema variants and verified channel/unit mappings.
- [x] Import RCZ directly into the existing `TelemetrySession` model alongside VBO,
  preserving per-channel timestamps, missing values, GPS, recorded OBD channels and heart rate.
  Never infer an unavailable channel or couple telemetry time to video FPS.
- [x] Define supported single-session, resumed-session and multi-session archive behavior;
  require explicit selection or reject unsupported variants rather than silently choosing data.
- [x] Bound archive/member counts and compressed/expanded bytes, validate paths and metadata,
  reject malformed/truncated/unsupported inputs, and keep loading cooperatively cancellable.
- [x] Preserve source timing-gate metadata where supported and verify lap results against
  the corresponding recording; do not invent gates from undocumented fields.
- [x] Support RCZ in file dialogs, asynchronous import, project save/reopen/relink/recovery,
  and the export worker. Preview and export must resolve the same telemetry and transform.
- [x] Add synthetic native regression fixtures to both CI platforms and separate private
  RCZ/VBO equivalence acceptance for timestamps, units, overlapping channels, gaps and laps,
  allowing documented precision/sampling differences between the formats.

- [x] Correct the verified RaceChrono Pro 10.2.4 VBO gate geometry; the private pair now derives five complete laps through VBO and RCZ.
- [ ] Validate additional RaceChrono VBO exporter versions; identified unverified versions omit gates with a warning.

### Lap timing and session analysis

- [x] Parse bounded source-defined RaceChrono timing gates without making malformed timing metadata fatal to otherwise valid telemetry.
- [x] Derive same-direction Start passages, complete timed laps, lap traces, and fastest-lap state from raw GPS and raw telemetry time.
- [x] Add Analysis navigation for Out lap, every measured lap, and In lap through the central synchronized playback timeline.
- [x] Add live best-lap comparison plus independent Best, Current, and Delta tiles for lap time and speed.
- [x] Finalize an active Start-gate cluster when telemetry ends inside the corridor, so the last completed lap is not omitted.
- [x] Publish Current state after the first accepted Start passage instead of requiring an already completed lap.
- [x] Add deterministic parser/detector/controller/QML coverage and validate the private Jastrząb fixture on the macOS Qt toolchain.
- [ ] Later: manual Start/Finish override, sectors/theoretical best, distance-normalized comparison charts, and automatic sectors.

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
- [x] Windows per-user NSIS internal installer with exact-file removal and CI lifecycle checks.
- [ ] Complete distribution packaging/signing and an automatic update strategy; initial Windows candidates use uninstall/reinstall.
