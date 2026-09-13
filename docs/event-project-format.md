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
sample arrays or cached lap times. Published lap summaries include `runId`;
`(runId, number)` identifies a lap within the current derivation. Replacing a
source or changing gates can change lap numbering, so this is not yet a durable
annotation/bookmark ID. Cross-run references will also need derivation identity.

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
persistent edit, and leaves the loaded editor source intact. This model step does
not add a configuration UI or automatic track/direction recognition.

`lapDerivationKey` combines a version tag, run ID, primary source ID/fingerprint
and configuration. Names, notes, video synchronization and source path spelling
are excluded. Outing-analysis requests include this identity: changing layout,
direction, gate revision or source cancels/rejects old work and closes stale lap
detail. Analysis also checks asserted gate revisions against the loaded recording
before publishing rows. Existing source fingerprint and generation checks remain.
No cross-run comparison cache is persisted yet; compatibility groups and durable
lap references are subsequent tasks. Unknown identities never establish that two
runs are compatible merely because their unknown values match.

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
