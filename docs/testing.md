# Testing

## Normal local gate

```bash
cmake --build build-native --parallel
ctest --test-dir build-native --output-on-failure
```

Cloud CI is intentionally disabled. Run the local gate appropriate to the change before claiming a behavior works.

## Private real fixtures

Optional real-media tests read paths from environment variables:

```bash
FLAPPEDEAR_REAL_GOPRO=/path/to/video.mp4 \
FLAPPEDEAR_REAL_VBO=/path/to/session.vbo \
  ./build-native/native/tests/flappedear_native_tests
```

Private VBO and GoPro media are ignored by Git and must remain local. A VBO-only run can set `FLAPPEDEAR_REAL_VBO`; GoPro synchronization requires both variables.

## Current test coverage

- VBO parsing, deterministic mid-parse cancellation, file/line/row/column/field limits, malformed input, text time formats, monotonic/rollover behavior, coordinate conversion, and optional real VBO parsing.
- Missing-versus-zero raw and overlay presentation semantics, bounded stale holding, and channel availability.
- Segmented raw analysis ranges, cadence-relative timestamp gaps, and bounded min/max decimation that retains short peaks.
- Deterministic, ambiguous, and cooperatively cancelled GPS-speed synchronization, plus optional real GoPro/VBO synchronization.
- GPS9 GPMF decoding, malformed packet extents, container-depth and KLV-header-count limits with count/limit/context diagnostics, and deterministic sorting/deduplication of timestamps.
- A portable native fake ffprobe covers prompt cancellation/reaping and bounded stdout without shell dependencies or long real inputs.
- Widget, group, cue, template, and track-geometry behavior, including a 30,000-point cache benchmark, same-address geometry invalidation, cache clearing, marker movement, and a QML guard against time-driven static-path reconstruction.
- Project atomic-save behavior; authoritative startup loading; QSettings document-key retirement; dirty-state actions for Quit, New, and Open; explicit saved and project-less recovery/discard; failed-save recovery retention; unknown-field preservation and v2 migration; project-relative source serialization and whole-folder moves; independent missing video/VBO states; deterministic fingerprints and sampled-byte mismatch; valid/invalid relink candidates; explicit mismatch replacement; stale relink rejection; and bounded controller shutdown.
- Export-output transaction safety, including native regular-file identity capture, unchanged replacement, in-place modification/replacement/disappearance refusal, new-target appearance refusal, and symlink rejection where the host permits link creation; plus export progress/diagnostics parsing, durable export-log creation/append/retention, media probing, exact rational rate comparison, and HEVC encoder detection.
- Source-media coverage for 8-bit, 10-bit, unknown depth, Rec.709, HLG/PQ/Log classification, coded/display raster, rotation/SAR retention, arbitrary 5.3K and 8:7 rasters, non-heavy 8K representation, aspect-preserving downscales, checked RGBA sizing, continuous bitrate, centralized export profiles, renderer rejection, and encoder capability-cache keys. A deterministic FFmpeg composition proves that a Rec.709 10-bit source remains HEVC Main10/yuv420p10 with audio.
- Portable storage resolution coverage for existing files/directories, future files, nested future paths, and unavailable inputs; injectable multi-volume preflight and measured-sample/fallback/margin/overflow regressions.
- Portable raw-frame transport helper coverage for exact 1920×1080 and 3840×2160 RGBA frame transfers to a slow consumer, early consumer exit, sustained stall, prompt cancellation, and bounded queue size.
- Malformed/owned/live/idempotent manifest recovery rules and a macOS/Unix helper child/grandchild process-tree shutdown integration test. Windows Job Object setup errors are explicit. Windows runtime/export is validated on one known Windows 11 / Qt 6.11 / MSVC 2022 / Intel Iris Plus / Quick Sync configuration; native Windows ACL-denied coverage remains pending because `QFile::setPermissions()` does not model Windows ACL denial reliably.
- Synthetic FFmpeg integrations for non-zero-range video/audio timelines, the exact `60000/1001` 30→90 boundary schedule (3,597 packets), non-zero source stream PTS, VFR-to-CFR conversion, CFR packet/frame counts, completed-overlay frame identity, and full decoded FFV1 staged-overlay frame counts.
- Temporary-overlay validation regressions for `60000/1001` cadence represented by Matroska's 1 ms timestamp quantization (`19001/317`), while rejecting a meaningful `30/1` mismatch. Production uses metadata plus producer/encoder frame invariants; the decoded count is retained by the integration test.
- The startup QML smoke rejects `ReferenceError`, `TypeError`, and binding-loop diagnostics and verifies one primary decoder while Analysis is closed, two while open, and release back to one after close.

The August 2026 macOS real-fixture regression run measured 1,536 GPMF packets, 259,584 parsed KLV headers, 14,796 GPS9 samples, +90.217 s synchronization offset, and 0.999575 correlation. The same source passed final HEVC/AAC packet validation at 3840×2160 `60000/1001` for 30→90 (3,597 packets), 1920×1080 `60000/1001` for 30→33 (180 packets), and 1280×720 `30000/1001` for 30→35 (150 packets). A separate private HERO11 5312×2988 Main10/BT.709 clip passed native 5312×2988 and 3840×2160 Main10 exports at `60000/1001`, each with 442 final video packets and AAC; its `gpmd` track had no usable GPS-speed samples. These are private fixtures on one macOS machine, not broad compatibility claims.

Unit and synthetic integration tests do not replace manual real-media validation. The latter should identify the fixture class, platform, source range, output properties, and any untested behavior.

## Before claiming a feature works

- Run the focused test and the normal local gate when applicable.
- For parser or synchronization work, include malformed and deterministic/ambiguous cases.
- For export changes, distinguish staged-overlay tests from final real-media validation.
- For real media, state platform and fixture scope; do not generalize one machine's result.
- For UI or host behavior, reproduce the visible interaction rather than inferring it from a build or unit test.
