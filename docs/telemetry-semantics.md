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

## Missing values and lookup

The parser may retain non-finite numeric values internally as placeholders so channel rows remain aligned. The public `TelemetrySession::valueAt()` API never returns `NaN` or infinity: it returns no data instead.

- A time outside a channel's timestamp range is no data.
- An exact sample whose value is missing is no data.
- Linear interpolation requires two adjacent finite samples.
- Previous returns only the immediately preceding sample; it does not search backward across a gap.
- Nearest returns the nearest sample even when that nearest value is missing; it does not substitute a farther finite value.
- No mode bridges a missing gap automatically.

For example, with samples `0 s = 10`, `1 s = missing`, and `2 s = 30`, a lookup at `1 s` is no data in every mode. A linear lookup at `0.5 s` and `1.5 s` is also no data because one adjacent endpoint is missing.

This is intentional: visible missing telemetry is safer than fabricated or stale telemetry.

## GPS tracks

Track construction ignores non-finite latitude/longitude values, latitudes outside `-90..90`, and longitudes outside `-180..180`. If there are no usable coordinate pairs, the track is unavailable. A current track position is unavailable when either latitude or longitude has no telemetry value at the requested time.

RaceChrono coordinates supplied as signed total arc-minutes are converted to degrees when their magnitude identifies that representation.
