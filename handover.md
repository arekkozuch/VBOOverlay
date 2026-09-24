# Session handover — 2026-09-24

Written for: the next Claude Code session continuing this work (likely on
another device). Read this first, then `AGENTS.md` (checked into the repo
root — engineering rules and safety invariants for this codebase).

## Project

VBOOverlay / "Flapped Ear Telemetry" — a Qt6/C++20/QML native desktop app
combining a video overlay editor with track-day telemetry analysis (VBO/RCZ
GPS+telemetry files, GoPro video). Repo: `arekkozuch/VBOOverlay` on GitHub.

**Deadline context**: the owner set a hard deadline of **2026-09-27** for a
usable milestone (M2: A/B lap comparison). Today is 2026-09-24 — go check the
actual current date, don't trust this file's age. Time pressure is real; keep
scope decisions honest and disclosed rather than silently narrowing them (the
project's own delivery ledger, `docs/product-delivery.md`, has this same
value baked in).

## What just happened (this session, in order)

1. **Comparison view overhaul** (PR #36, merged) — replaced two independent,
   unaligned side-by-side A/B lap panels with a single overlaid chart + map
   (F1-debrief style). Added a "Δ time" pseudo-channel, scroll-wheel zoom/pan,
   multi-channel rows, fixed a real QML binding-loop bug (bare `width` inside
   a Rectangle's own binding resolves to its own width, not the parent's —
   check `AnalysisPanel.qml`/`ComparisonOverlayChart.qml` history if you hit
   something similar), and a perf fix (QPointF instead of QVariantMap per
   chart point — the QString-keyed QMap was the actual bottleneck).
2. **Shared track-progress alignment engine** (PR #37, merged) — new
   `native/src/telemetry/TrackProgress.{h,cpp}` (KAN-31/32/33/34): a
   gate-anchored, ~2m-spaced progress axis; a bounded local-search projection
   (`projectSample`) that resolves hairpin/parallel-section ambiguity via a
   forward-biased search window + runner-up-gap check + heading agreement;
   gap-preserving projected traces (`projectLapTrace`); and an A-minus-B
   delta series (`computeDeltaSeries`). Well tested — see
   `native/tests/TrackProgressTests.cpp`.
3. **Wired the alignment engine into the comparison UI** (PR #38, **open,
   CI green, NOT YET MERGED** — see "Immediate next step" below). Replaced
   the old per-lap-distance parameterization (`LapDistance.{h,cpp}`, now
   deleted entirely) with shared-progress-based invokables:
   `comparisonChannelSeriesByProgress`, `comparisonDeltaSeriesByProgress`,
   `comparisonPositionAtProgress`, `comparisonProgressAxisLength`. No UI
   restructuring was needed — the QML already treated its x-axis as opaque
   "meters."

## Immediate next step

**PR #38 (https://github.com/arekkozuch/VBOOverlay/pull/38) is open with
green CI (macOS Debug + Release) but not merged.** A Claude Code auto-mode
guardrail blocks `gh pr merge` from an agent session ("Merge Without Review")
— this is intentional and was hit (and respected, not routed around) twice
already this session for PRs #36/#37, which the owner then merged manually.
Don't try to bypass it. Either:
- Ask the owner to click merge (they've done this promptly both times so
  far), or
- If they've explicitly authorized merging in *this* session (check recent
  conversation), retry `gh pr merge 38 --merge --delete-branch` — if it's
  blocked again, report it and stop, same as before.

**After it merges**: `git fetch origin && git checkout main && git merge
--ff-only origin/main`, rebuild, re-run the full suite on the synced `main`
(see "Build/test" below) to double check, then delete the local
`feature/kan-35-38-progress-wiring` branch and transition KAN-35, KAN-36,
KAN-37, KAN-38 to "Gotowe" (Done, transition id `41`) in Jira with a comment
recording the merge commit SHA and verification — follow the exact pattern
of the existing comments already on those 4 tickets (they're currently "W
toku"/In Progress with a comment linking PR #38; add one more comment on
merge, mirroring how KAN-31-34 were closed out after PR #37 merged).

## Working conventions established this session (the owner asked for these explicitly)

- **Jira**: process tickets as work happens — transition to "W toku" (In
  Progress, id `21`) when starting, add a comment with the branch/PR/commit
  and what landed + verification evidence, transition to "Gotowe" (Done, id
  `41`) only after merge with the final SHA. Cloud ID:
  `315ac5b8-6fd1-4518-8b5f-4433bcc33447`. **Always disclose gaps/limitations
  in the comment** rather than silently claim full acceptance-criteria
  coverage — e.g. KAN-32's comment discloses that the figure-eight/parallel
  fixtures on the shared GPS generator weren't built (a hand-built synthetic
  axis was used instead); KAN-35's discloses that no "measured/calculated"
  provenance field exists anywhere in the codebase to expose, and that the
  discrete-channel (gear) interpolation path isn't integration-tested.
- **GitHub**: real feature branches + PRs, not direct pushes to `main`
  (`gh auth status` is already logged in as `arekkozuch` — this took the
  owner running `gh auth login` once mid-session; if a fresh device has no
  `gh` auth, tell the owner rather than trying to push to `main` directly).
  Cloud CI (macOS Debug + Release, Qt 6.8.3) triggers automatically on PR
  push via GitHub Actions — always wait for it (`gh pr checks <n> --watch
  --interval 30` in the background) before considering something done.
  **Never merge without the owner's go-ahead** (see guardrail above).
- **Local commits only, until a PR is warranted**: for a self-contained
  bounded item, commit locally first, verify build+tests, *then* branch off
  `origin/main`, cherry-pick, push, open the PR. Don't push directly to a
  branch the owner is also working on without checking `git status`/`git
  log` first (there was a backlog of un-pushed local commits earlier this
  session that had to be reconciled into two separate PRs by topic).
- **Build/test gate, every time, before calling anything done**:
  ```bash
  cmake --build build-native --parallel
  ctest --test-dir build-native --output-on-failure
  ```
  9 suites, must be 9/9 green. Note: piping either through `| tail` hides
  real failures behind `tail`'s own exit code — redirect to a file and grep
  for "error" instead, or check the file directly. This bit me once this
  session (a real compile failure was reported as "exit code 0").
- **Manual smoke test before claiming a QML change works**: launch the app
  directly (`build-native/native/Flapped Ear Telemetry.app/Contents/MacOS/
  Flapped Ear Telemetry`), check stderr for QML warnings, quit cleanly. The
  app is single-instance-locked — check `pgrep -f "Flapped Ear
  Telemetry.app/Contents/MacOS"` first and don't kill a process you didn't
  start (it might be the owner's real, in-progress session with unsaved
  recovery state).
- **macOS only** per AGENTS.md's owner direction — don't start Windows
  builds/CI.

## Known disclosed gaps (don't silently claim these are done)

- KAN-32: no figure-eight/parallel-section fixtures built on the shared
  `EventProjectFixture::routeVbo()` GPS generator (used across ~20 existing
  tests) — ambiguity/heading tests use a small hand-built synthetic axis
  instead. Real coverage against the shared generator is a legitimate
  follow-up, not done.
- KAN-35: no "measured vs. calculated" provenance field exists anywhere in
  `TelemetryChannel`/`TelemetrySession` — only `unit` is exposed. Discrete-
  channel (gear) interpolation logic (`InterpolationMode::Previous`) is
  implemented but not integration-tested, since extending the shared fixture
  to include a gear-like column wasn't judged safe under time pressure.
- Real VBO/GoPro media validation is still outstanding across all of this
  session's work — only synthetic fixtures have been exercised. Report real-
  media results separately per AGENTS.md when that happens.

## Remaining M2 scope (per the original plan, not yet started)

KAN-39 (video linkage), KAN-40 (missing-data/incompatibility context UI),
KAN-41 (persist A/B selection + channels + range with stale-reference
handling — partial groundwork already exists from an earlier session, commit
`c98d27e`, "KAN-41 groundwork"), KAN-42 (accept A/B comparison + reforecast
after M2). Given the 2026-09-27 deadline, the owner already agreed earlier in
this engagement that if time runs short, KAN-41 (persistence) beats KAN-39
(video linkage) in priority, and a shared scrub cursor (already done, KAN-38)
was wanted over a merely-static side-by-side view.

## Key files

- `native/src/telemetry/TrackProgress.{h,cpp}` — the alignment engine.
  `buildProgressAxis`, `projectSample`, `projectLapTrace`,
  `computeDeltaSeries`, `timeAtProgress`. Read the doc comments in the header
  first; they explain the safety properties (never bridge a gap, never guess
  under ambiguity) in detail.
- `native/src/app/AppControllerComparison.cpp` — comparison feature logic:
  slot lifecycle, `ensureComparisonSharedGeometry` (shared map bounding box)
  and `ensureComparisonProgressAxis` (shared alignment axis), both lazily
  rebuilt only when a slot's `request` id changes (memoized, not rebuilt per
  frame). The one expensive step (`deriveSourceLapSession`, a whole-file GPS
  scan) happens in the existing background worker
  (`AppController::readOutingLapDetail`, now takes a `deriveReferenceGate`
  bool so the plain single-lap outing-detail path doesn't pay for it).
- `native/qml/ComparisonDetailPanel.qml` /
  `native/qml/ComparisonOverlayChart.qml` /
  `native/qml/ComparisonOverlayMap.qml` — the comparison UI: multi-channel
  rows (up to 4, add/remove/replace), shared zoom/pan (scroll or drag),
  shared hover cursor driving both the chart value readouts and the map
  markers, all sharing one `hoverDistanceMeters`/`zoomStart`/`zoomEnd` state
  owned by `ComparisonDetailPanel` (the property names still say "Meters"/
  "Distance" for historical reasons — they now carry shared-progress values,
  not each lap's own distance-into-lap; this is noted in comments but not
  renamed, to limit churn).
- `native/tests/TrackProgressTests.cpp` — alignment engine tests (axis
  anchoring, identical-lap zero-delta, known-delay sign/magnitude, ambiguous-
  crossing rejection, heading-conflict rejection, gap/outlier segment
  breaking, delta-coverage bounding).
- `native/src/telemetry/LapDistance.{h,cpp}` — **deleted** this session
  (superseded by TrackProgress; nothing else referenced it).

## Credentials/access already set up this session

- Jira: read/write via MCP tools, cloud ID above. The Jira MCP tool call
  results always append a notice about the HTTP+SSE transport being retired
  2026-06-30 — this has been relayed to the owner once already; no need to
  keep repeating it.
- GitHub: `gh` is authenticated as `arekkozuch` (token scopes: gist,
  read:org, repo, workflow) via the owner running `gh auth login` mid-
  session. Git push over HTTPS also works via the macOS keychain credential
  helper independently of `gh`'s own auth.
