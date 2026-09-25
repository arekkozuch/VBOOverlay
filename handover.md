# Session handover — 25 September 2026 (evening)

Written for: the next Claude Code session continuing this work (likely on
another device). Read this first, then `AGENTS.md` (checked into the repo
root — engineering rules and safety invariants for this codebase).

**Two sessions have been working this repo concurrently.** An earlier
handover (preserved below, "KAN-42/43 session") ran in a *coordinator*
workspace with no Qt/CMake toolchain, delegating implementation and relying
on hosted CI for all build/test evidence, and got as far as opening KAN-43.
This later session (mine — full local Qt toolchain, `/Users/arek/VBOOverlay`)
picked up from KAN-55 onward, after discovering **KAN-42 through KAN-54 had
all already landed on `main`** (PRs #42–#54) without any visibility into how
— that's the coordinator session's work, confirmed real and green (18/18
suites) but not narrated here in detail. If you need the KAN-43–54
implementation rationale, read their individual PR descriptions/Jira
comments; this file only carries forward what each session directly did.

## Project

VBOOverlay / "Flapped Ear Telemetry" — a Qt6/C++20/QML native desktop app
combining a video overlay editor with track-day telemetry analysis (VBO/RCZ
GPS+telemetry files, GoPro video). Repo: `arekkozuch/VBOOverlay` on GitHub.

**Deadline context**: the owner's 27 September M2 milestone target has
effectively arrived and passed working scope-wise — M2 (KAN-29–42) is done,
and the owner explicitly chose to push into M3 anyway (see the KAN-42
reforecast in `docs/kan42-m2-acceptance.md`: ~1.1 tickets/day observed rate,
~60 elapsed days for all remaining numbered tasks at that rate — the owner
has seen this and decided to continue). M3's segment/corner-metrics pipeline
(KAN-43–54) is now also done. **KAN-55 (Corner Analyzer UI, this session)
is the next M3 UI-facing ticket; KAN-56+ remain.** Separately, the owner
made a personal binary checkpoint earlier and said the pre-M3 state was
"good enough for Sunday" — that checkpoint is now well behind current `main`.

## What this session did (KAN-55)

Starting point: KAN-55 ("045 — Build the Corner Analyzer view with A/B
evidence") was the next Jira item, depending on KAN-52/53/54 (corner speeds,
braking, exit metrics) and KAN-38 (comparison cursor/state) — all already
done. **PR #55, open, CI status unknown as of this writing** — check
`gh pr view 55` before assuming merged or pending.

- Added an approved-segment list with A/B/delta metrics to the comparison
  view: sector time (every segment type) and, for corners, entry/apex/
  minimum/exit speed, braking point, throttle pickup. New button "Corner
  Analyzer" in `ComparisonDetailPanel.qml` swaps out the channel-chart area
  for the new `ComparisonSegmentPanel.qml`. Selecting a metric's "jump to
  segment" (⌖) control sets the *existing* shared `zoomStart`/`zoomEnd`/
  `hoverDistanceMeters` — no second cursor mechanism.
- New `AppControllerCornerAnalyzer.cpp`: `comparisonApprovedSegments()` and
  `comparisonSegmentMetrics(segmentId)`, parallel to the existing
  single-open-lap `outingLap*Metrics` (KAN-51–54) but keyed to the
  comparison feature's two slots and its shared progress axis
  (`ensureComparisonProgressAxis`) instead of the segment-review session.
  Segments only ever show when both compared laps' approved segmentation
  matches **exactly** (same revision + track configuration reference) —
  segments are approved per run (KAN-48–50), so there's no guessed
  correspondence between two independently-approved sets.
- Two new comparators filling a real, pre-existing gap: `compareSectorTimes`
  (`SectorTiming.h`) and `compareCornerSpeeds` (`CornerSpeeds.h`) — braking
  and exit metrics already had `BrakingComparison`/`ExitComparison`, sector
  timing and corner speeds didn't. Mirrors their exact pattern (same
  segment/revision/configuration required; mixed channel/provenance
  withholds the comparison rather than guessing).
- New `native/src/telemetry/MetricProvenance.h`: a
  `measured`/`calculated`/`inferred`/`unavailable` enum. **No such shared
  vocabulary existed anywhere** before this ticket — each calculator has
  its own narrower, inconsistent provenance concept (sector times have
  none at all). This header is a *display-time derivation layer* for the
  new combined view only — it does not touch or retrofit the four
  calculators' own existing fields.

### A real QML bug found (and fixed) via the new test

Bare (non-`id.`-qualified) references from a child `Label` to a sibling
`readonly property var` declared on an enclosing `RowLayout` **do not
reliably resolve under `pragma ComponentBehavior: Bound`**. It was silently
masked in the corner-speed rows specifically because the GPS-only test
fixture (`routeVbo()`, no speed channel) keeps that `Repeater`'s model
empty, so the broken binding never actually ran — only the
always-instantiated braking/exit rows surfaced it as a runtime
`ReferenceError` (caught by the `QSignalSpy` on `QQmlEngine::warnings`
already standard in this test suite). Fixed by giving each row an explicit
`id` and qualifying every reference through it (`brakingRow.brakingPoint`,
not bare `point`) — matching the `rowItem.modelData`-style convention
already used elsewhere (`ComparisonOverlayChart.qml`). **If you write a new
QML row/delegate anywhere in this codebase that declares a
`readonly property` and reads it from a child item, always qualify through
an explicit `id`, even if it "should" be in scope — bare references are not
reliably safe under Bound mode**, and a broken one can sit completely
invisible until the exact data shape that exercises it shows up.

## Working conventions established this session (still in force, carried
forward from the coordinator session's handover below)

- **Jira**: transition to "W toku" (id `21`) when starting, comment with
  branch/PR/commit + what landed + disclosed gaps, transition to "Gotowe"
  (id `41`) only after merge with the final SHA. Cloud ID:
  `315ac5b8-6fd1-4518-8b5f-4433bcc33447`. Always disclose gaps rather than
  claim full coverage.
- **GitHub**: real feature branches + PRs for planned/Jira-tracked work.
  Cloud CI (macOS Debug + Release, Qt 6.8.3) triggers on PR push and on
  push to `main` — wait for the **exact head SHA**'s run, not a historical
  one. **Never merge without the owner's explicit go-ahead** — the
  guardrail blocking agent-initiated merges has been hit and respected
  repeatedly across both sessions; post green CI evidence and wait.
- **Build/test gate, every time** (this session's workspace has a full Qt
  toolchain; the coordinator workspace did not and relied entirely on
  hosted CI — check which situation you're in):
  ```bash
  cmake --build build-native --parallel
  ctest --test-dir build-native --output-on-failure
  ```
  If you add/rename a `.cpp` file, you must also re-run
  `cmake -S . -B build-native -DCMAKE_BUILD_TYPE=Debug
  -DCMAKE_PREFIX_PATH=/opt/homebrew/opt/qt` (or your local equivalent)
  before building — a plain `cmake --build` won't pick up a new source file
  added to `CMakeLists.txt`/`tests/CMakeLists.txt`.
- **When a new comparison-level test needs a route/track to actually
  resolve**, use `EventProjectFixture::routeVbo()`'s **default `turns=4`**
  — `inferTrack()` needs ≥2 complete laps; lower turn counts can silently
  produce zero eligible comparison rows (a real bug the KAN-42 session hit
  and fixed).
- **macOS only** per AGENTS.md's owner direction — don't start Windows
  builds/CI.
- **Doc types**: `handover.md` (this file) is agent-owned, rewritten (or, as
  this update shows, carefully merged when another session's work is still
  relevant) every session. `docs/product-delivery.md`/`docs/product-vision.md`
  are owner/audit-authored — flag staleness, don't silently rewrite their
  forecasts. `docs/kanNN-*-acceptance.md` is the per-milestone
  acceptance/reforecast record pattern (KAN-28 for M1, KAN-42 for M2) —
  follow that shape for a future M3 acceptance ticket.
- **Two-session coexistence**: before starting substantial new work, check
  `git fetch origin && git log --oneline origin/main -20` and Jira status
  for tickets you'd assume are still open — this session discovered 13
  tickets (KAN-42–54) already done this way, entirely by surprise. Don't
  trust this file's own "what's done" narrative over that direct check;
  it can go stale within the same day if another session is also active.

## Known disclosed gaps

- KAN-55: not every sub-metric is surfaced (braking distance/mean-
  deceleration, exit interval-end-speed/elapsed-seconds are computed but
  not exposed through `comparisonSegmentMetrics`/the QML panel yet).
  Cross-run comparison (two *different*, independently-approved runs that
  happen to share a matching revision) isn't directly tested — the test
  fixture uses two laps of one run/file, which trivially share one run's
  `trackSegments`. No interactive/real-media verification (another app
  instance was already running this session); the route fixture has no
  speed channel, so "measured" corner/braking/exit provenance is only
  proven by the pure-C++ comparator tests (synthetic loop fixtures with
  real speed data), not the AppController/QML integration tests.
- KAN-42 (coordinator session): no interactive/real-GoPro/real-VBO
  verification for its three new tests. Dual/side-by-side comparison video
  is unimplemented by design (KAN-104–107), not a KAN-42 gap.
- M2's cycle-time reforecast is workflow-status elapsed time (includes
  CI/review/admin gaps), not pure engineering effort — see
  `docs/kan42-m2-acceptance.md`.
- Real VBO/GoPro media validation remains outstanding across all of M2/M3
  so far — only synthetic fixtures have been exercised in automated tests.

## Key files

- `native/src/app/AppControllerCornerAnalyzer.cpp` — KAN-55's per-slot
  segment-metrics surface. `comparisonApprovedSegmentation(slot)` (private
  helper) mirrors `AppController::currentApprovedSegmentation()`
  (`AppControllerSegmentReview.cpp`) but parameterized by comparison slot.
- `native/src/telemetry/MetricProvenance.h` — the
  measured/calculated/inferred/unavailable vocabulary. Read its header
  comment before adding a fifth calculator's metrics to any future combined
  view — it explains why this is additive, not a retrofit.
- `native/qml/ComparisonSegmentPanel.qml` — the Corner Analyzer panel.
  Read the id-qualification note above before adding another metric row.
- `native/src/telemetry/SectorTiming.h` / `CornerSpeeds.h` — now also
  `compareSectorTimes`/`compareCornerSpeeds`; `BrakingMetrics.h`/
  `ExitMetrics.h` already had the equivalent `BrakingComparison`/
  `ExitComparison` pattern these two were built to match.
- `docs/kan42-m2-acceptance.md` — KAN-42's full acceptance matrix and the
  M2 reforecast math; read before touching comparison tests.
- `native/src/telemetry/TrackProgress.h/.cpp` — `ProgressAxis`,
  `buildProgressAxis`, `projectLapTrace`: the shared cross-lap arc-length
  axis all segment boundaries (KAN-43) and this session's corner-analyzer
  metrics are expressed against.
- `/home/user/flappedear/drivingcoach` (coordinator workspace path, likely
  not present in this session's environment) — a Dart/Flutter reference
  repo (`FlappedEar/DrivingCoach`) the owner pointed at as validated prior
  art for M3's corner/loss-detection algorithms (speed-trough segmenting,
  braking/lift hysteresis, a 5-category coaching-opportunity detector with
  confidence scoring). Design reference only, not portable code — different
  stack, and it normalizes laps with its own simpler interpolation instead
  of this app's crossing/gap-aware `ProgressAxis`. Relevant for KAN-56+
  (losses/coaching, not yet started). Re-clone via the anonymous git-read
  lane if you need it and the path is gone.

## Credentials/access

- Jira: read/write via the `plugin:atlassian:atlassian` MCP connector,
  cloud ID above. Not durably authorized across sessions/devices — expect
  to re-authorize via `/mcp`. A v1→v2 (SSE → streamable-HTTP) transport
  migration prompt may appear (deprecation, sunset after 30 June 2026) —
  not urgent.
- GitHub: `gh` authenticated for `arekkozuch/VBOOverlay` (push, PR, Actions).
