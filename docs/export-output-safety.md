# Export output transaction guarantees

The selected user target is never passed to FFmpeg. Each export creates an empty staging file with
`QIODevice::NewOnly` in the target directory and records that exact path in the live
`ExportOutputTransaction`. FFmpeg writes only to that staging file. Cancellation, worker failure,
validation failure, forced worker termination, and controller destruction remove only paths in that
ownership record. Filename shape alone is never treated as proof of ownership.

For a new target, the staging file is renamed to the target on the same filesystem after final media
validation. The transaction refuses to commit if another file appeared at the target while encoding.

For an existing target, replacement requires the controller's explicit `overwriteAllowed` flag:

- macOS (and other POSIX builds): `rename(2)` replaces the directory entry atomically. A crash exposes
  either the old complete file or the new complete file; there is no delete-then-rename window.
- Windows: `ReplaceFileW` with `REPLACEFILE_WRITE_THROUGH` replaces the existing file as one system
  operation. If Windows rejects the replacement, the existing target remains in place and the staged
  file remains owned for cleanup.
- Other platforms: the fallback renames the target to a transaction-owned backup, installs staging,
  and rolls the backup back if installation fails. This preserves the old file on ordinary operation
  failure, but it does not claim crash atomicity between the two renames.

Input video, VBO, and known transaction paths are compared using cleaned absolute paths and canonical
paths where Qt can resolve them, including existing symlinks and canonicalized parent directories.

## Active artifact manifests and recovery

Before a worker is started, the controller writes an atomic versioned JSON manifest in the system temporary directory. It records the export UUID, creation time, worker PID, state, temporary FFV1 path, staging path, and final target for diagnostics. The manifest is the authorization record: only its validated overlay and staging paths may be removed automatically; the final target is never a cleanup candidate.

On normal success, cancellation, or failure the controller removes those owned artifacts and the manifest. If cleanup cannot finish, the manifest remains. At application startup the janitor reads only FlappedEar manifest files, rejects malformed records, skips a record whose PID is still active, and removes only paths proven by the valid manifest. Names such as `*.mkv` or `*.part.mp4` alone never authorize deletion.

PID reuse can conservatively cause an old manifest to be retained when an unrelated process has reused its recorded PID. That may leave recoverable temporary files behind, but it never broadens deletion authority.
