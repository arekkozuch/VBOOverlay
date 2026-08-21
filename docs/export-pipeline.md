# Export pipeline

Export uses two FFmpeg stages so telemetry frames are generated from the same QML scene as preview without relying on a live secondary video stream.

## Stage 0: probe and capability detection

`MediaProbe` reads the input video metadata with `ffprobe`. `EncoderDetector` discovers HEVC encoders and verifies a small encode before one is selected. Likely variable-frame-rate input is detected from a meaningful difference between nominal and average frame rate and is surfaced as a warning.

## Stage A: render and stage telemetry

For the selected source range, `ExportEngine` calculates an explicit rational frame cadence and asks `TelemetryFrameRenderer` for each source-time frame. The renderer mounts `TelemetryScene.qml` through `QQuickRenderControl` and QRhi, reads back full-resolution RGBA images, and sends them through a bounded `QProcess` pipe to FFmpeg.

FFmpeg writes the completed overlay as FFV1/BGRA in a temporary Matroska file. The bounded pipe limits queued raw frames while preserving renderer/encoder overlap.

The staged overlay is validated with `ffprobe` for:

- FFV1 codec;
- expected dimensions;
- expected average cadence;
- expected frame count; and
- duration within the frame-cadence tolerance.

The staging step exists because FFmpeg framesync can select the latest secondary frame at or before a primary timestamp. With a live secondary pipe, the primary decoder can advance while telemetry repeats a stale frame. Stage B starts only after a completed, validated overlay exists on disk.

## Stage B: compose the final MP4

Stage B trims the source video to the requested range, normalizes source and overlay timestamps to zero, overlays the staged telemetry, encodes HEVC, optionally trims/re-encodes source audio to AAC, and writes an MP4 staging target.

Telemetry rendering still uses absolute source time: exporting source seconds 120–140 renders its first overlay at source time 120. The output audio/video timeline starts at zero.

Progress and Very Verbose diagnostics report stage activity, FFmpeg progress, temporary-overlay size, validation checks, and bounded diagnostic output. Cancellation asks the active process to stop, then escalates to kill if needed. Final validation checks for a nonempty result, HEVC video codec, expected dimensions, duration tolerance, and requested audio before the output transaction commits the user target.

## CFR and VFR status

Stage A explicitly uses the selected rational cadence. Stage B is timestamp-driven: current arguments do not add an `fps` filter, `-r`, or `-fps_mode cfr`. Therefore the final MP4 is not documented as guaranteed CFR. Explicit final-CFR enforcement and final VFR behavior validation remain open media-correctness work.

For target-file transaction guarantees, see [export-output-safety.md](export-output-safety.md).
