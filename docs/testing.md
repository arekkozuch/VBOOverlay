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

- VBO parsing, malformed input, text time formats, monotonic/rollover behavior, coordinate conversion, and optional real VBO parsing.
- Missing-data lookup semantics and sampled telemetry ranges.
- Deterministic and ambiguous GPS-speed synchronization, plus optional real GoPro/VBO synchronization.
- GPS9 GPMF decoding and malformed-GPMF handling.
- Widget, group, cue, template, and track-geometry behavior.
- Project atomic-save behavior, dirty-state actions, failed async source loads, and transactional project loading.
- Export-output transaction safety, export progress/diagnostics parsing, durable export-log creation/append/retention, media probing, exact rational rate comparison, and HEVC encoder detection.
- Injectable multi-volume storage preflight policy, measured-sample/fallback/margin/overflow estimate regressions, malformed/owned/live manifest recovery rules, and a macOS/Unix helper child/grandchild process-tree shutdown integration test. Windows Job Object setup errors are explicit; Windows runtime behavior remains untested here.
- Synthetic FFmpeg integrations for non-zero-range video/audio timelines, non-zero source stream PTS, VFR-to-CFR conversion, CFR packet/frame counts, completed-overlay frame identity, and full decoded FFV1 staged-overlay frame counts.
- Temporary-overlay validation regressions for `60000/1001` cadence represented by Matroska's 1 ms timestamp quantization (`19001/317`), while rejecting a meaningful `30/1` mismatch. Production uses metadata plus producer/encoder frame invariants; the decoded count is retained by the integration test.

Unit and synthetic integration tests do not replace manual real-media validation. The latter should identify the fixture class, platform, source range, output properties, and any untested behavior.

## Before claiming a feature works

- Run the focused test and the normal local gate when applicable.
- For parser or synchronization work, include malformed and deterministic/ambiguous cases.
- For export changes, distinguish staged-overlay tests from final real-media validation.
- For real media, state platform and fixture scope; do not generalize one machine's result.
- For UI or host behavior, reproduce the visible interaction rather than inferring it from a build or unit test.
