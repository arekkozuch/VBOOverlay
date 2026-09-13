# Event projects (development v3)

Flapped Ear Telemetry remains one macOS-first telemetry and overlay application.
The project schema is developmental: the owner confirmed that no existing user
data needs migration protection. v3 gives events a single authoritative source
model; the small existing v2 read/write path remains for single-recording files.
This is not a frozen schema or a promise to maintain future migrations.

## Ownership

An event owns an ordered list of runs and an `activeRunId`. Each run owns:

- Stable `id`, display `name` and `primaryTelemetrySourceId`.
- `sources.telemetry`: 1–8 source entries, each with an event-wide unique `id`
  and a `reference` containing relative/absolute paths and an optional fingerprint.
- Optional `sources.video` reference and its `sync.offset`/`sync.timeScale`.
- `trackConfiguration` on new imports: explicit layout/direction/gate identity,
  bound to the primary source ID and fingerprint (details below).
- Optional metadata such as `notes`; bounded unknown fields survive round trips.

Event/run/source IDs are document identities, not filenames or import proposal
hashes. Import confirmation will allocate them once; relinking does not replace
these IDs. Alternate exports are retained, but only the explicit primary source
is loaded. There is no RCZ-over-VBO priority or channel fusion.

Laps remain derived from the selected source and source gates, not serialized
sample arrays or cached lap times. Published lap summaries include `runId` and a
portable `reference` object. Display lap numbers and row indices are not durable
annotation/bookmark identities; use the derivation-bound reference below.

The widget scene, analysis channel selection and export/map settings are shared
by the document. Per-run scenes and per-run export ranges are not implemented.
Only one run's telemetry and video are loaded at a time. Selection clears prior
loaded data and starts cancellable, generation-guarded source loading; a missing
primary stays missing even if an alternative exists. Missing external assets do
not invalidate an otherwise valid event document.

## Track configuration and derivation identity (KAN-19)

New imports persist the following run-local configuration. Layout IDs are opaque
identifiers assigned explicitly, not filenames, display names or inferred GPS
clusters. Reuse an ID only for the same physical layout. Direction is independent
of a gate's crossing sign and remains `unknown` until explicitly assigned.

```json
"trackConfiguration": {
  "layoutId": null,
  "direction": "unknown",
  "gateRevision": null,
  "sourceId": "primary-source-id",
  "sourceFingerprint": {}
}
```

`layoutId` is null or a nonblank ID up to 128 characters. Direction is `unknown`,
`clockwise` or `counterclockwise`. `gateRevision` is null or `gates-v1:` followed by
64 lowercase SHA-256 hex characters. Import derives a revision from the ordered
source gate types and endpoints, normalizing explicit west-positive longitude to
east-positive for identity only. Names/descriptions do not change the revision;
endpoint, gate type/order or gate count changes do. Missing/ambiguous start gates,
unknown gate types, invalid coordinates or more than 128 gates remain unresolved.
A known revision identifies geometry; it does not establish lap eligibility.

`sourceId` must name this run's primary source; `sourceFingerprint` must equal
that source reference's fingerprint. Foreign, alternative or stale bindings are
rejected before project commit. Legacy v3 runs without the object read as unknown;
opening them alone does not assert a layout, direction or gate revision.

Save, Save As, recovery and run switching retain the configuration. Same-content
relinking preserves it. Replacing source content clears its asserted fields to
unknown and binds them to the replacement fingerprint. The controller API
`setRunTrackConfiguration(runId, layoutId, direction)` supports validated explicit
assignment or clearing (empty layout ID / `unknown` direction), records a normal
persistent edit, and leaves the loaded editor source intact. Step 012 exposes this
through the Track configuration dialog; automatic track/direction recognition is
not implemented.

`lapDerivationKey` combines a version tag, run ID, primary source ID/fingerprint
and configuration. Names, notes, video synchronization and source path spelling
are excluded. Outing-analysis requests include this identity: changing layout,
direction, gate revision or source cancels/rejects old work and closes stale lap
detail. Analysis also checks asserted gate revisions against the loaded recording
before publishing rows. Existing source fingerprint and generation checks remain.
Compatibility groups are derived in memory as described below. Portable lap
references bind to this derivation identity. Unknown identities never establish that two
runs are compatible merely because their unknown values match.

## Stable lap references (KAN-20)

Every published OUT/LAP/IN/UNKNOWN section has a portable JSON `reference` with:

| Field | Contract |
| --- | --- |
| `version` | Numeric `1`; this schema has exactly these ten fields |
| `algorithm` | `source-laps-v1`; bump when lap detection/section semantics change |
| `eventId`, `runId`, `sourceId` | Event/run/primary source identities, each a nonblank string up to 128 characters |
| `sourceRevision` | Full-file SHA-256, 64 lowercase hex characters |
| `derivationKey` | Step 009 run/source/configuration key, 64 lowercase hex characters |
| `type` | `OUT`, `LAP`, `IN` or `UNKNOWN` |
| `startTime`, `endTime` | Exact finite telemetry seconds; `0 <= startTime < endTime` |

Store the whole object alongside a future annotation or selection. JSON number
round trips preserve its bounds. Reopening the same event/source/derivation,
Save As, same-content relocation, renaming a run and reordering display rows do
not change the reference. Source replacement, gate/configuration edits or a new
algorithm cannot silently rebind it to a matching lap number or nearby time.
References to partial/unknown sections identify those sections; they do not make
them eligible for rankings. No annotation/bookmark storage UI is added here.

`resolveOutingLapReference(reference)` returns a `state` and, only when resolved,
a current `index`. Failure states include a reason:

| State | Meaning |
| --- | --- |
| `resolved` | Exactly one current derived row matches every reference field |
| `invalid` | Malformed/unsupported schema, identity, digest, section or bounds |
| `stale` | Different event/run/source/configuration/algorithm, changed content/bounds or ambiguous matches |
| `loading` | Current derivation is not ready, including edits before the refresh timer runs |
| `unavailable` | Referenced run exists but its recording has no available derivation |

Resolution describes the current derived snapshot. `selectOutingLapReference`
opens only resolved references; keyboard, mouse and accessibility row actions use
this API. Invalid/stale/unavailable references never select a substitute. The
index API also rejects outdated snapshots before a queued refresh can run.

The worker computes full content identity before and after lap derivation, using
the shared bounded/cancellable SHA-256 reader (128 MiB/file, 64 KiB blocks; existing
256 MiB outing/batch limits remain). Detail loading checks the referenced digest
again before parsing and verifies it afterward. An external edit since the last
snapshot produces an explicit stale-reference error, with no old track/series
published. The resolver then marks references to that run stale until a fresh
derivation is published; a missing source produces an unavailable recording error. This closes
the gap left by sampled fingerprints for same-size edits outside sampled blocks.
The existing faster project fingerprint/relink policy remains in place. Full
hashing adds bounded sequential reads; native/private-file performance acceptance
is separate from synthetic correctness tests.

## Persistence and limits

v3 forbids root `sources`, `sync`, `videoPath` and `vboPath`. A temporary single-run
editor projection feeds existing source loading, preview and export; it is never
stored alongside the event. v2 with an `event` object is rejected rather than
silently dropping the event on save. A previous app that only knows v2 rejects v3.

An event contains 1–64 runs, at most 8 telemetry sources per run and at most 128
telemetry sources overall. IDs are nonblank strings up to 128 characters; names
up to 160. The common 4 MiB project/recovery payload, depth, string and scene
limits still apply. Active/primary references, unique identities, reference
shapes and finite positive time scales are validated before controller commit.
Dates and track configurations are not inferred from filenames or file times.

Save and recovery use the existing atomic writers and logical document revision.
Selecting a different run is a persistent edit: it keeps the document identity,
dirty edits and last-saved revision; it does not reopen the project as clean.
It preserves the open Analysis window and current template provenance. Selection
is blocked during project loading, export, startup recovery decisions and dirty
new/open/quit decisions. Late results cannot restore the previously selected run.

Save As rebases all source references, including inactive runs, alternatives and
missing video/telemetry. Resolution prefers an existing relative location, then
the absolute fallback. If neither exists, the previous document's relative
location is retained as the intended location. An old relative spelling is never
reused against an unrelated Save As directory. Existing directory symlinks are
resolved before rebasing a missing source, so Save As through a linked folder
does not introduce a dependency on the old link name. Existing fingerprint checks and
explicit mismatch confirmation still apply on relink.
Export target protection includes all event references, including inactive runs
and alternative exports; overwrite consent cannot turn a source into an output.

## Try this slice on macOS

1. Build the development branch using the normal native build instructions.
2. Open `native/tests/fixtures/event-demo.fetproject` using Open Project, then
   open the Telemetry Analysis window. This fixture has two deliberately synthetic,
   independent runs; it is not a real track day or performance comparison.
3. Use **Active run** to switch between GPS lap data and the basic channel fixture.
   No video is required. This slice does not add cross-run comparison or change
   the existing video-dependent lap navigation list.
4. Change sync on one run, switch away/back, and verify the value is run-local.
   Use **Save As** outside the fixture directory and reopen that saved project.
   Keep the original source fixtures available. The scene is initially empty;
   apply a widget template if testing an overlay with your own video binding.

Use **File → Import telemetry runs…** to create an event from multiple VBO/RCZ
files, or add runs to the current event. See [batch import review](batch-import.md).
The separate single-file **Open telemetry…** picker still replaces the active
run's primary source; it does **not** add another run.

Automated coverage: `flappedear_event_project_tests` validates the schema,
resource bounds, reference rebasing and recovery round trip; controller tests
cover run selection, laps, dirty state, Save As, relink/fingerprint policy,
recovery, stale worker rejection and the actual Analysis QML selector. Existing
native, import, RCZ, render/export and startup suites remain required. Hosted
checks do not replace interactive macOS or private-recording validation.

## User lap exclusions (step 011)

An optional `event.lapExclusions` array stores `{reference, reason}` entries.
`reference` is the step-010 portable LAP reference: event, run, source, complete
source SHA-256, derivation key, algorithm version and exact time bounds. Reasons
must contain text, contain no NUL, and fit within 256 characters. Duplicate
references and more than 20,000 entries are rejected, alongside the existing
project byte limit. Legacy documents without this array include every otherwise
eligible lap.

In lap detail, enter a reason (for example Traffic or Cooldown) and choose
**Exclude lap**. **Restore lap** removes the exclusion. The lap, map, charts and
cursor remain inspectable. Excluded rows show their reason. Save, Save As and
unsaved-document recovery preserve exclusions through the ordinary project
transaction. Source or derivation changes never transfer a reason by lap number
or nearby time: unmatched entries remain saved and a notice identifies the count.
The controller API also permits explicit removal of a historical reference.

`TimedLap::referenceEligible()` combines GPS quality with user exclusion.
`eligibleLapIndices()` is the shared input policy for ranking and future
potential/statistics calculations; those later analysis features are not introduced
by this step. Recomputing best/deltas retains all measured laps and raw traces;
restoring a GPS-ineligible lap does not make it a valid reference. An all-excluded
run has no best lap. Preview and the export worker use the same policy, and export
verifies the complete source revision before and after deriving laps.

Validation includes malformed documents, exact-reference invalidation, all-excluded
ranking, shared render-context results, keyboard-operated production QML controls,
and actual save/reopen/recovery flows. Native macOS Debug/Release CI provides the
build/test gate; private recordings and physical-Mac acceptance remain separate.

## Explicit compatibility groups (step 012)

Open **Track configuration…** in All laps, choose a recording, enter a layout
name, choose clockwise/counterclockwise and confirm. The entered name is the
layout ID: leading/trailing whitespace is removed, but spelling and case must
match to join the same layout. The decision applies to all laps in that recording
and uses the existing persisted source-bound configuration. An outdated dialog
cannot overwrite a changed derivation. Cancel leaves unresolved fields unchanged.
Gate revisions come from verified source geometry, never from a user override.
A recording without a verified gate revision remains unresolved even after its
layout/direction are confirmed. Configuration changes can leave prior lap
exclusions unmatched, as explained in the dialog.

A resolved group requires an exact match of layout ID, direction and timing-gate
revision. Its stable `compatibility-v1:` SHA-256 ID hashes only those fields and a
version, so identical configurations can group different recordings. Unknown or
malformed fields never compare equal for this purpose: unresolved recordings
remain separate, visible entries and cannot be chosen for comparison.

Choose a resolved comparison group to see each lap's reasons relative to it.
All recorded sections remain visible and inspectable. Reasons are independent:
different layout, opposite direction, different/unresolved gates, unresolved
layout/direction, incomplete/invalid GPS, user exclusion, stale source and
incomplete timed section. Several can apply to the same row. Tooltips expose
reasons when the row label is too narrow.

`outingCompatibilityGroups` supplies all LAP members and the eligible member
references separately; OUT/IN/UNKNOWN sections never become comparison candidates.
GPS/user exclusions do not redefine physical compatibility or hide group members.
No group is selected implicitly, and `comparisonEligible` requires an explicit
selected group plus no blocking reasons. Group choice is session-only; confirmed
run configuration survives save/reopen/recovery. Missing, changed or in-flight
source/derivation generations cannot serve stale group selections. Group membership
and reasons are rebuilt from validated current lap references; they are not saved
as an independent cache. This step does not add alignment, potential estimates,
automatic direction inference or judgments about representative performance.

Regression coverage verifies exact grouping boundaries, unknown-to-unknown
rejection, simultaneous GPS/user/compatibility reasons, durable explicit decisions,
generation invalidation, and keyboard interaction with the production QML dialog
and group selector. Native verification uses macOS arm64 Debug and Release CI;
physical-Mac and private-recording acceptance remain separate.
