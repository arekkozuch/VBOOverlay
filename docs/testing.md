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
- GPS9 GPMF decoding, malformed packet extents, container-depth and record-count limits, and deterministic sorting/deduplication of timestamps.
- A portable native fake ffprobe covers prompt cancellation/reaping and bounded stdout without shell dependencies or long real inputs.
- Widget, group, cue, template, and track-geometry behavior, including a 30,000-point cache benchmark, same-address geometry invalidation, cache clearing, marker movement, and a QML guard against time-driven static-path reconstruction.
- Project atomic-save behavior; authoritative startup loading; QSettings document-key retirement; dirty-state actions for Quit, New, and Open; explicit saved and project-less recovery/discard; failed-save recovery retention; unknown-field preservation; failed async source loads; transactional project loading; source replacement, bounded controller shutdown, and rejection after an intervening document edit.
- Export-output transaction safety, including native regular-file identity capture, unchanged replacement, in-place modification/replacement/disappearance refusal, new-target appearance refusal, and symlink rejection where the host permits link creation; plus export progress/diagnostics parsing, durable export-log creation/append/retention, media probing, exact rational rate comparison, and HEVC encoder detection.
- Portable storage resolution coverage for existing files/directories, future files, nested future paths, and unavailable inputs; injectable multi-volume preflight and measured-sample/fallback/margin/overflow regressions.
- Portable raw-frame transport helper coverage for exact 1920×1080 and 3840×2160 RGBA frame transfers to a slow consumer, early consumer exit, sustained stall, prompt cancellation, and bounded queue size.
- Malformed/owned/live manifest recovery rules and a macOS/Unix helper child/grandchild process-tree shutdown integration test. Windows Job Object setup errors are explicit. Windows runtime/export is validated on one known Windows 11 / Qt 6.11 / MSVC 2022 / Intel Iris Plus / Quick Sync configuration; native Windows ACL-denied coverage remains pending because `QFile::setPermissions()` does not model Windows ACL denial reliably.
- Synthetic FFmpeg integrations for non-zero-range video/audio timelines, non-zero source stream PTS, VFR-to-CFR conversion, CFR packet/frame counts, completed-overlay frame identity, and full decoded FFV1 staged-overlay frame counts.
- Temporary-overlay validation regressions for `60000/1001` cadence represented by Matroska's 1 ms timestamp quantization (`19001/317`), while rejecting a meaningful `30/1` mismatch. Production uses metadata plus producer/encoder frame invariants; the decoded count is retained by the integration test.

Unit and synthetic integration tests do not replace manual real-media validation. The latter should identify the fixture class, platform, source range, output properties, and any untested behavior.

## Before claiming a feature works

- Run the focused test and the normal local gate when applicable.
- For parser or synchronization work, include malformed and deterministic/ambiguous cases.
- For export changes, distinguish staged-overlay tests from final real-media validation.
- For real media, state platform and fixture scope; do not generalize one machine's result.
- For UI or host behavior, reproduce the visible interaction rather than inferring it from a build or unit test.
