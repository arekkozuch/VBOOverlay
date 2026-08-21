# Export pipeline

Export uses two FFmpeg stages so telemetry frames are generated from the same QML scene as preview without relying on a live secondary video stream.

## Stage 0: probe and capability detection

`MediaProbe` reads the input video metadata with `ffprobe`. `EncoderDetector` discovers HEVC encoders and verifies a small encode before one is selected. The effective export rate is one exact `MediaRational`: an explicitly supplied export rate when valid, otherwise the source `avg_frame_rate`, falling back to `r_frame_rate`. This exact rational is retained throughout export; it is not reconstructed from a rounded decimal.

Likely variable-frame-rate input is detected from a meaningful difference between nominal and average frame rate and is surfaced as a warning.

Before Stage A, export independently inspects the temporary-overlay and destination filesystems with `QStorageInfo`. The conservative temporary estimate currently uses a documented 6 MiB/frame FFV1 fallback plus 30% margin; the final estimate uses the selected target bitrate plus 25% margin. A 2 GiB reserve is retained. If both paths resolve to one filesystem, their concurrent requirements are combined. An export fails before rendering with an actionable free-space error when the relevant requirement cannot fit. During encoding the temporary volume is sampled about once per second and the process is stopped before the reserve is exhausted.

## Stage A: render and stage telemetry

For the selected source range, `ExportEngine` calculates an explicit rational frame cadence and asks `TelemetryFrameRenderer` for each source-time frame. The renderer mounts `TelemetryScene.qml` through `QQuickRenderControl` and QRhi, reads back full-resolution RGBA images, and sends them through a bounded `QProcess` pipe to FFmpeg.

FFmpeg writes the completed overlay as FFV1/BGRA in a temporary Matroska file. The bounded pipe limits queued raw frames while preserving renderer/encoder overlap.

The normal staged-overlay validation uses a metadata-only `ffprobe` call plus
independent producer/encoder invariants. It checks:

- FFV1 codec;
- expected dimensions;
- `generatedFrames == submittedFrames == FFmpeg progress frame count == expectedFrames`;
- zero-origin stream start when reported; and
- duration and reported `r_frame_rate` / `avg_frame_rate` within the tolerance
  implied by the Matroska stream time base and exact scheduled duration.

The rawvideo command receives the authoritative exact `MediaRational` (for
example `60000/1001`). Matroska commonly stores this FFV1 stream with a 1 ms
time base, so FFmpeg may report a nearby rational such as `19001/317` for both
reported rates after timestamp quantization. That metadata is not required to
be rationally identical to the schedule; it must be explainable by the
stream-time-base duration bound. A meaningful cadence error such as `30/1`
remains outside that bound and fails validation.

Normal export deliberately does not pass `-count_frames` for the complete
temporary FFV1 file. Full decoded frame counts remain deterministic/deep test
evidence, while the production path uses the already-known producer count,
FFmpeg's final progress count, and a quick metadata probe. Only if FFmpeg's
final progress record is incomplete does it fall back to `-count_packets`; the
generated FFV1 Matroska integration test verifies the one-packet-per-frame
property used by that exceptional path.

The staging step exists because FFmpeg framesync can select the latest secondary frame at or before a primary timestamp. With a live secondary pipe, the primary decoder can advance while telemetry repeats a stale frame. Stage B starts only after a completed, validated overlay exists on disk.

## Stage B: compose the final MP4

Stage B trims the source video to the requested range, normalizes timestamps to zero, then applies FFmpeg's `fps` filter at the effective exact rational rate before framesync. The CFR source and the completed CFR overlay therefore enter framesync on the same deterministic cadence. The filter is limited to the authoritative `[start, end)` frame count before composition.

The output also uses the per-video-stream `-fps_mode:v cfr` control. The `fps` filter establishes the cadence before framesync; `-fps_mode:v cfr` makes the final output policy explicit without relying on deprecated global `-vsync`. Stage B then overlays telemetry, encodes HEVC, optionally trims/re-encodes source audio to AAC, and writes an MP4 staging target.

Telemetry rendering still uses absolute source time: exporting source seconds 120–140 renders its first overlay at source time 120. The output audio/video timeline starts at zero.

Progress and Very Verbose diagnostics report stage activity, FFmpeg progress,
temporary-overlay size, frame-count source, reported rates/time base, metadata
validation elapsed time, validation checks, and bounded diagnostic output.
One cancellation file is consulted by input/temporary/final `ffprobe` calls and both FFmpeg stages. Cancellation follows cooperative request, a short graceful wait, process-tree termination, then force kill. On macOS/Unix the GUI worker starts in a dedicated process group; FFmpeg and ffprobe inherit it, so forced worker shutdown reaches the complete export tree. The current Unix behavior is runtime-tested. Windows assigns the top-level worker to a `JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE` job; that path is compile-tested here, not runtime-validated on Windows.
Final validation checks for a nonempty result, HEVC codec, dimensions, exact
nominal and average rate, progress frame count, independent video packet count
when available, zero video start, scheduled video duration, and requested audio.

Audio is trimmed from the requested source interval and reset to output time zero. Its start is compared with video using at most one AAC access-unit duration (1024 samples at the reported sample rate, or the audio time base when larger); its duration is compared with the requested interval using that same defensible tolerance. This permits normal AAC priming/edit-list granularity without accepting arbitrary A/V drift.

## CFR and VFR status

Every final export is CFR at the effective exact rational export rate. For a selected source interval `[start, end)`, `expectedFrames` is `ceil((end - start) * rate)` and output video duration is `expectedFrames / rate`; the final frame's PTS is `(expectedFrames - 1) / rate`. Telemetry frame `N` remains evaluated at `start + N / rate`, while the output timeline begins at zero.

The normal validation path uses FFmpeg's final progress frame count plus an independent packet count where the container exposes one. Full decoded frame counts remain part of deterministic integration tests rather than every production export, avoiding an unnecessary full decode of long media.

For target-file transaction guarantees, see [export-output-safety.md](export-output-safety.md).
