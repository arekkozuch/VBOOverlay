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
reused against an unrelated Save As directory. Existing fingerprint checks and
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

Batch import/review and event creation UI are the next slice. The existing
single-file telemetry picker replaces the active run's primary source; it does
**not** add another run. The development fixture exposes this persistence slice
without pretending that the multi-file workflow is already available.

Automated coverage: `flappedear_event_project_tests` validates the schema,
resource bounds, reference rebasing and recovery round trip; controller tests
cover run selection, laps, dirty state, Save As, relink/fingerprint policy,
recovery, stale worker rejection and the actual Analysis QML selector. Existing
native, import, RCZ, render/export and startup suites remain required. Hosted
checks do not replace interactive macOS or private-recording validation.
