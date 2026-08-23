# Telemetry semantics

This document is the contract for VBO parsing and public telemetry lookup.

## VBO timestamps

The parser recognizes time text before numeric conversion:

- `HH:MM:SS[.fraction]`, for example `01:02:03.500`.
- `HHMMSS[.fraction]`, for example `010203.500`.
- Plain finite seconds, for example `3723.5`.

Clock hours must be `0..23`, minutes `0..59`, and seconds `0 <= seconds < 60`. A six-digit integer component is interpreted as compact clock syntax, so `003059.500` means `00:30:59.500`, not 3,059.5 relative seconds. Invalid timestamps skip their row with a warning.

Clock timestamps can cross midnight once when a previous clock value is at or after 23:00 and the next is at or before 01:00. The parser adds 24 hours for that rollover. Later duplicate timestamps are skipped; any other backward timestamp is skipped. Emitted timestamps are checked to be strictly monotonic.

Parser warnings are capped at 200 stored messages; additional warnings are summarized in one final message.

VBO input is treated as untrusted. Parsing is cooperatively cancellable and rejects files above 128 MiB, more than 1,000,000 lines or 500,000 data rows, more than 512 columns, lines above 1 MiB, and fields above 64 KiB. Resource-limit failures and cancellation are distinct from invalid VBO syntax.

GoPro GPMF input is likewise bounded independently by packet count, aggregate metadata bytes, parsed KLV-header work, and container depth. The KLV counter includes structural/container and non-GPS sensor headers as well as GPS records; it is not a GPS sample count. Limit failures report the reached count, configured limit, packet, and parse context, while cancellation remains a distinct outcome.

## Missing values and lookup

The parser may retain non-finite numeric values internally as placeholders so channel rows remain aligned. The public `TelemetrySession::valueAt()` API never returns `NaN` or infinity: it returns no data instead.

- A time outside a channel's timestamp range is no data.
- An exact sample whose value is missing is no data.
- Linear interpolation requires two adjacent finite samples.
- Previous returns only the immediately preceding sample; it does not search backward across a gap.
- Nearest returns the nearest sample even when that nearest value is missing; it does not substitute a farther finite value.
- No mode bridges a missing gap automatically.

For example, with samples `0 s = 10`, `1 s = missing`, and `2 s = 30`, a lookup at `1 s` is no data in every mode. A linear lookup at `0.5 s` and `1.5 s` is also no data because one adjacent endpoint is missing.

Missing telemetry is not numeric zero. A finite zero remains a valid measurement, while widgets expose unavailable or stale values as no data (`—`). Geometry may use an internal minimum/zero fallback only when a separate validity flag prevents that fallback from being presented as measured telemetry.

## Overlay presentation

Preview and export share `TelemetryRenderContext`, which applies presentation filtering without modifying `TelemetrySession` or its raw lookup API. Ordinary finite samples are timestamp-interpolated (gear uses previous-value semantics). A missing value or the end of a channel may hold the most recent finite value for a bounded stale interval: 750 ms for ordinary channels and 2 seconds for heart rate. After that, the value is no data.

Light, recency-weighted trailing smoothing uses finite values only, with older samples contributing progressively less. The centralized starting windows are:

- speed and RPM: 150 ms;
- lateral/longitudinal G-force: 200 ms;
- throttle and brake: 100 ms;
- heart rate: 250 ms;
- other continuous channels: 150 ms;
- gear: no smoothing.

These short windows reduce frame-to-frame jitter without delaying pedal events with a large average. Missing channels remain unavailable; in particular, absent G-force channels hide the moving dot instead of placing it at fake `0 g`.

For a finite timestamp jump, three times the channel's median positive sample interval defines the normal-cadence tolerance. Overlay presentation uses the larger of that tolerance and its stale interval. Across a larger jump it stops interpolation, briefly holds the preceding value, then becomes stale. This distinguishes ordinary sparse sampling from a real gap deterministically.

## Analysis ranges

Analysis reads actual raw channel samples, not presentation-filtered values. Non-finite values start a new segment. A timestamp jump greater than three times the median positive channel interval also starts a new segment, so the renderer issues a new path rather than drawing across a real gap.

Display decimation divides the requested range into time buckets and retains each bucket's minimum and maximum in timestamp order. This preserves short braking, RPM, throttle, and acceleration extrema where practical. Returned data is bounded to at most twice the requested bucket count; pathological high-gap input may omit some runs to respect that bound, but retained runs are still separate and never connected across a gap. Analysis never inserts zero, interpolates a replacement sample, or applies overlay smoothing.

## GPS tracks

Track construction ignores non-finite latitude/longitude values, latitudes outside `-90..90`, and longitudes outside `-180..180`. If there are no usable coordinate pairs, the track is unavailable. A current track position is unavailable when either latitude or longitude has no telemetry value at the requested time.

The normalized track outline is static for the lifetime of an assigned geometry and is cached for QML rendering. Time changes update only the independently rendered current-position marker. Replacing or clearing geometry invalidates the cached outline and marker together; this rendering lifecycle does not alter GPS lookup or missing-data semantics.

RaceChrono coordinates supplied as signed total arc-minutes are converted to degrees when their magnitude identifies that representation.
