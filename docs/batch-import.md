# Flapped Ear Telemetry: multi-file import review

## macOS workflow

1. Choose **File → Import telemetry runs…** and select multiple VBO/RCZ files
   in the native file dialog (up to 64).
2. Review the file names, durations in minutes:seconds and complete-lap counts.
   Duplicates and failures remain visible. **Show file details and import warnings**
   reveals paths, parser diagnostics and possible same-run GPS matches.
3. Keep **Import as a run**, choose **Skip this file**, or **Same run as**
   another source. That other source must itself remain a separate primary run.
   Grouping is explicit and does not depend on filename, extension or a guessed
   recording date. Similar GPS traces are suggestions, not automatic merges.
4. Choose **Create a new event** and supply a name, or **Add runs to current
   event**. Confirm. The files are rechecked before the document changes.
5. Save the event. Use **Active run** in Analysis to switch recordings.

The lap timing panel displays all complete telemetry laps even without video or
when a lap falls outside video coverage. Video coverage only enables the seek
action; video in/out fragments remain available through the existing video navigation.
Import dropdowns use the application's explicit dark background and highlighted
text colors, independent of the native macOS control palette.

A new event starts without a video binding and with independent zero-offset,
unit-scale transforms; the current widget layout and global analysis/settings
are retained. It gets a new document identity and requires Save As. Creating it
is blocked if the current document has unsaved edits: cancel the review, save
those edits and import again. Existing saved projects remain on disk. Appending
keeps current unsaved edits, the active run, its video/sync and all existing IDs.

The multi-file dialog does not turn the existing **Open telemetry…** action
into an append action. Folder discovery and drag/drop are not part of this
iteration. It also does not add event best times or cross-run lap comparison.
Appending creates new runs; attaching another export to an existing run is not
supported by this dialog. Import complementary exports together to group them.

## Source groups and duplicate policy

Only the selected primary supplies channels and derived laps. Alternatives are
persisted with independent references and fingerprints; no channel fusion or
cross-source clock transform is invented. Grouping two sources is a user
assertion, not proof that they represent the same stint. Cycles, missing/skipped
primaries, duplicate choices and oversized source groups are rejected.

Identical same-format files within a batch appear as duplicates and produce one
proposal. Sources added through this workflow retain their full SHA-256, format
and original sampled project fingerprint in `importProvenance`. When appending,
an identical imported source whose saved reference fingerprint still matches
that provenance is skipped; a relink with a different fingerprint cannot inherit
the old digest identity. Older event sources without this provenance are not
automatically deduplicated: review them manually. Existing-source matches remain
available when creating a separate new event.

No dates or track configuration are inferred, and no event-wide performance
statistics are calculated from unconfirmed compatibility. A malformed file does
not hide other successful results. An all-failed/all-skipped batch cannot commit.
Links whose backing file has a different format extension are reported as errors:
project reopening resolves the backing path, so import a correctly named regular
copy instead. Ordinary same-format links retain the normal project behavior.

## Transaction, cancellation and limits

Preparation and final full-digest verification run off the GUI thread and check
cancellation between bounded reads. Progress counts processed files; preparation
also performs fingerprint checks and GPS candidate analysis before review opens.
Final verification covers only selected sources. The digest check detects
ordinary changes between review and commit; it is not an atomic filesystem
snapshot or a guarantee against adversarial concurrent mutation. Subsequent
source loading still enforces the saved project fingerprint.

No project, source selection, dirty state or recovery snapshot changes before
successful confirmation. Cancel (including Escape), validation failure or stale
results leave the current document intact. Import context includes document ID,
revision, source generation **and project path** (Save As invalidates a review).
Only one import worker may be outstanding; cancellation cannot start parallel
batches that exceed the retention budget. Closing the controller requests worker
cancellation without allowing worker callbacks to access destroyed GUI objects.

The existing ceilings remain: 64 input files, 128 MiB/file, 256 MiB/batch,
16 million retained channel samples, plus parser bounds. Fingerprinting and final
verification add bounded I/O, not additional admitted raw telemetry. Event commit
also validates 64 runs, 8 telemetry sources/run, 128 sources/event and the 4 MiB
project payload. Stable event/run/source IDs are allocated for the candidate
document and become authoritative only on successful commit.

## Validation

Controller tests cover six independent runs and partial file failure, exact
duplicates, append/reopen identity, explicit RCZ/VBO grouping, cycles, cancelled
preparation/confirmation, file changes after review, dirty-document protection,
Save As/source-generation invalidation and submission through the production QML
dialog. The import suite also verifies completed-file progress callbacks.

Run the normal native build and all five CTest registrations. This Work runtime
has no CMake/Qt, so executable results are recorded in the implementation PR's
CI validation record. Interactive macOS/native-picker testing, private recordings
and hardware export validation are separate and are not implied by hosted CI.
