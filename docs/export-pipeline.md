# Export pipeline

Export uses two FFmpeg stages so telemetry frames are generated from the same QML scene as preview without relying on a live secondary video stream.

## Stage 0: probe and capability detection

`MediaProbe` reads the input video metadata with `ffprobe`. `EncoderDetector` discovers HEVC encoders and verifies a small encode before one is selected. The effective export rate is one exact `MediaRational`: an explicitly supplied export rate when valid, otherwise the source `avg_frame_rate`, falling back to `r_frame_rate`. This exact rational is retained throughout export; it is not reconstructed from a rounded decimal.

Likely variable-frame-rate input is detected from a meaningful difference between nominal and average frame rate and is surfaced as a warning.

## Export format recommendations

The export dialog resolves its displayed dimensions, exact rational rate, video bitrate, and size estimate from one shared format model. HEVC recommendations for high-motion onboard footage use 30 fps resolution-tier baselines: 6 Mbps (720p), 12.5 Mbps (1080p), 21.2 Mbps (1440p), and 36.5 Mbps (2160p+). The selected rate applies a square-root `sqrt(fps / 30)` adjustment, so 60 fps preserves additional temporal detail without doubling the bitrate. Smaller file and High quality apply 0.70x and 1.30x to that recommendation; Custom is the only editable bitrate mode and remains validated at 0.5–120 Mbps. Estimated output size uses the resolved video bitrate plus enabled 192 kbps AAC audio, including its existing 3% container allowance, and is displayed in MiB below 1024 MiB or GiB otherwise.

Before Stage A, export renders 24 evenly distributed telemetry frames (or fewer for short ranges) with the production QRhi scene and encodes them to an in-memory FFV1/BGRA Matroska sample. The measured encoded bytes/frame is projected across the exact scheduled frame count with a 1.50 safety margin. Sampling does not decode source video or create a disk artifact. If that tiny sample cannot complete, the fallback remains 512 KiB/frame with a 1.75 margin: intentionally far above the observed roughly 71 KiB/frame 4K telemetry overlay, without treating a mostly transparent scene as raw RGBA. The selected final bitrate receives a 25% margin and a 2 GiB reserve is retained. Temporary and destination filesystems are inspected independently; a future artifact is resolved against its nearest existing filesystem ancestor while diagnostics retain the intended path. If both resolve to one volume, concurrent requirements are combined. An export fails before Stage A when the requirement cannot fit. During encoding the temporary volume is sampled about once per second and the process is stopped before the reserve is exhausted.

## Stage A: render and stage telemetry

For the selected source range, `ExportEngine` calculates an explicit rational frame cadence and asks `TelemetryFrameRenderer` for each source-time frame. The renderer mounts `TelemetryScene.qml` through `QQuickRenderControl` and QRhi, reads back full-resolution RGBA images, and sends them through a bounded `QProcess` pipe to FFmpeg.

FFmpeg writes the completed overlay as FFV1/BGRA in a temporary Matroska file. The shared transport writes at most 1 MiB at a time, applies an 8 MiB high-water and 4 MiB low-water policy independent of frame resolution, and treats cancellation, process exit, rejected/partial writes, and sustained lack of byte progress as explicit outcomes. A frame is counted as submitted only after all its bytes have been accepted. The same transport is used for the representative sample and Stage A.

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

Stage B trims the source video at the requested start and normalizes timestamps to zero, then applies FFmpeg's `fps` filter at the effective exact rational rate before framesync. The pre-`fps` stream intentionally retains end-boundary lookahead: at fractional rates an exact range endpoint can fall between source frames, and removing the following frame prevents `fps=round=near` from selecting the last scheduled pre-end frame. The authoritative `end_frame=expectedFrames` trim after cadence conversion enforces the `[start, end)` schedule without timeline drift. The CFR source and completed CFR overlay therefore enter framesync on the same deterministic cadence.

The output also uses the per-video-stream `-fps_mode:v cfr` control. The `fps` filter establishes the cadence before framesync; `-fps_mode:v cfr` makes the final output policy explicit without relying on deprecated global `-vsync`. Stage B then overlays telemetry, encodes HEVC, optionally trims/re-encodes source audio to AAC, and writes an MP4 staging target.

Telemetry rendering still uses absolute source time: exporting source seconds 120–140 renders its first overlay at source time 120. The output audio/video timeline starts at zero.

Progress and Very Verbose diagnostics report stage activity, FFmpeg progress,
temporary-overlay size, frame-count source, reported rates/time base, metadata
validation elapsed time, validation checks, and bounded diagnostic output.
When an export is prepared, the controller also creates one flushed text log in
`QStandardPaths::AppLocalDataLocation/exports/`, named
`export-YYYYMMDD-hhmmss-<export-uuid>.log`. Its header records the source,
requested resolved dimensions/rational rate/video bitrate/audio policy, and range;
the controller writes the same formatted worker diagnostics plus lifecycle events
and a final success, cancellation, or failure footer. Logs are support artifacts,
so inability to create one does not prevent an export. Retention removes only
older files matching that export-log naming convention in this dedicated directory,
keeping approximately the ten newest logs.
One cancellation file is consulted by input/temporary/final `ffprobe` calls, encoder discovery/capability checks, and both FFmpeg stages. Cancellation follows cooperative request, a short graceful wait, process-tree termination, then force kill. On macOS/Unix the GUI worker starts in a dedicated process group; FFmpeg and ffprobe inherit it, so forced worker shutdown reaches the complete export tree. The current Unix behavior is runtime-tested. Windows assigns the top-level worker to a `JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE` job and reports the exact failed Job Object API if that cannot be established; export then fails before worker release. The worker waits for an explicit parent readiness file before starting FFmpeg/ffprobe work, closing the assignment-before-descendant race in the current design. Windows runtime/export has been validated on one Windows 11 / Qt 6.11 / MSVC 2022 / Intel Iris Plus / Quick Sync configuration; heavy 4K GUI responsiveness, a wider GPU/encoder matrix, packaging/signing, and multi-instance export-log safety remain open.
Final validation checks for a nonempty result, HEVC codec, dimensions, exact
nominal and average rate, authoritative video packet count when available, zero
video start, scheduled video duration, and requested audio. FFmpeg progress is
retained for diagnostics and progress reporting, but an apparent mismatch does
not bypass the final probe or override its packet count. A real packet deficit or
surplus fails with direction-specific diagnostics.
After validation, an existing destination is replaced only if its prepare-time native regular-file
identity, size, and modification state still match; otherwise encoding is reported as finished but the
changed destination is preserved and commit fails.

Audio is trimmed from the requested source interval and reset to output time zero. Its start is compared with video using at most one AAC access-unit duration (1024 samples at the reported sample rate, or the audio time base when larger); its duration is compared with the requested interval using that same defensible tolerance. This permits normal AAC priming/edit-list granularity without accepting arbitrary A/V drift.

## CFR and VFR status

Every final export is CFR at the effective exact rational export rate. For a selected source interval `[start, end)`, `expectedFrames` is `ceil((end - start) * rate)` and output video duration is `expectedFrames / rate`; the final frame's PTS is `(expectedFrames - 1) / rate`. Telemetry frame `N` remains evaluated at `start + N / rate`, while the output timeline begins at zero.

The normal validation path records FFmpeg's final progress frame count and uses the independent final packet count as authoritative where the container exposes one. Full decoded frame counts remain part of deterministic integration tests rather than every production export, avoiding an unnecessary full decode of long media.

For target-file transaction guarantees, see [export-output-safety.md](export-output-safety.md).
