# Project format and external sources

`.fetproject` remains version 2. Source metadata is an optional extension of that format, so old v2 documents do not require a version bump or manual conversion. Unknown top-level and nested fields are retained when the application overlays known edits and saves.

## Source representation

New saves use a `sources` object and remove the legacy top-level `videoPath` and `vboPath` fields. Each `video` or `telemetry` entry may contain:

```json
{
  "sources": {
    "video": {
      "relativePath": "media/camera.mp4",
      "absolutePath": "/fallback/location/camera.mp4",
      "fingerprint": { "kind": "video-v1" }
    },
    "telemetry": {
      "relativePath": "media/session.vbo",
      "absolutePath": "/fallback/location/session.vbo",
      "fingerprint": { "kind": "telemetry-v1" }
    }
  }
}
```

Relative paths use forward-slash JSON spelling and resolve only against the directory containing the `.fetproject`. Clean nested paths and reasonable parent-relative paths are supported. They never resolve against the process working directory, application directory, or home directory. Lookup tries the relative path first, then the stored absolute fallback, and performs no recursive filesystem search.

When a loaded source is in the project directory, or no more than two parent directories above it, Save/Save As writes a relative reference. The normalized absolute location is retained as a fallback. Moving a project folder with its `media` subdirectory therefore keeps the relative reference usable on another filesystem root or operating system.

## Source fingerprints

Fingerprints are deterministic identity metadata, not cryptographic proof of complete-file identity. Both source types store file size and a SHA-256 digest over at most three fixed 64 KiB regions: head, middle, and tail. Video additionally stores probed duration in microseconds, dimensions, exact rational frame rate, and codec. Telemetry additionally stores parsed duration, sample count, and sorted channel name/unit/sample-count metadata.

The bounded byte sampling reads at most 192 KiB per source and is cheap relative to video probing or VBO parsing. It detects common accidental substitutions, including size, media-metadata, telemetry-structure, and sampled-content changes. Changes confined to unsampled bytes can collide, so the value is deliberately called a source fingerprint rather than a content hash.

## Opening and relinking

The project document is valid independently of external assets. After JSON, scene, and synchronization validation, the document commits first. Video and telemetry then resolve and load independently with `loading`, `ready`, `missing`, `mismatch`, or `error` state. Missing one or both assets does not replace the widget layout, synchronization, analysis settings, or other project fields.

Locate uses the ordinary bounded, cancellable, generation-guarded video probe or VBO parser. A matching fingerprint is accepted. A candidate that parses successfully but clearly differs is not committed until the user confirms intentional replacement. An invalid candidate remains an error and cannot replace the current source. Projects without a fingerprint accept a compatible candidate and acquire identity metadata in memory for the next save.

Successful relinking or intentional source replacement updates document source metadata and marks the project dirty. Resolving the same persisted relative reference after moving the complete folder does not mark it dirty. Recovery snapshots serialize the same complete source objects and remain unsaved document state; they never replace the saved project as authoritative clean state.

## Analysis state

`analysis.channels` is project content and remains in canonical saves and recovery snapshots. Whether the floating Analysis window is open is transient UI state: startup, New, and Open always begin with it closed. Older v2 documents may contain `analysis.visible`; the loader safely ignores that field and the next canonical save removes it without discarding channel configuration or unknown sibling fields.

## Legacy v2 compatibility

When `sources.video` or `sources.telemetry` is absent, the loader reads the old absolute-only `videoPath` or `vboPath` field. Available sources load normally and acquire fingerprints in memory. Missing legacy sources become independently relinkable rather than failing project open. The next save writes the canonical `sources` form, removes the known legacy path fields, and preserves unrelated unknown fields.
