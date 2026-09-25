# Session handover — 25 September 2026

Written for: the next Claude Code session continuing this work (likely on
another device). Read this first, then `AGENTS.md` (checked into the repo
root — engineering rules and safety invariants for this codebase). This file
replaces the previous same-day handover — that one's work (KAN-39, plus the
marker-size/legend/color fixes) is done and merged; this covers KAN-42 (M2
acceptance) and the start of KAN-43 (first M3 ticket).

## Project

VBOOverlay / "Flapped Ear Telemetry" — a Qt6/C++20/QML native desktop app
combining a video overlay editor with track-day telemetry analysis (VBO/RCZ
GPS+telemetry files, GoPro video). Repo: `arekkozuch/VBOOverlay` on GitHub.

**Deadline context**: the owner's 27 September M2 milestone target has
effectively arrived. KAN-42's own reforecast (below) shows M2 took ~11.4
elapsed days for 13 tickets — full F00–F20 scope will not land by 27
September. The owner has seen this evidence and, immediately after KAN-42
merged, asked to push ahead into M3 anyway, citing a separate reference
codebase (below). Continue delivering M3 increments; don't re-litigate the
deadline math already recorded in `docs/kan42-m2-acceptance.md`.

## What just happened (this session, in order)

1. **KAN-42 (M2 acceptance and reforecast)** — PR #41 (KAN-39) was already
   merged when this session started; the owner asked to proceed straight to
   KAN-42 (confirmed via explicit check-in, since the ticket's own
   handover note said to ask before starting it this close to the
   deadline). PR #42, **merged**, integrated-main head
   `2f98cb1c66a36db86212a189af3740d89d9e076a`; both macOS Debug/Release CI
   green pre-merge and post-merge.
   - Four of the five acceptance-criteria fixture categories (crossings,
     gaps, different lines, known delta) already had solid dedicated
     regressions from KAN-31 through KAN-34 — referenced, not duplicated.
   - Added `comparesKnownDeltaThroughFullComparisonPipeline` (elevates the
     known-delta guarantee to the full AppController import → comparison →
     shared-axis pipeline), `excludesChannelMissingFromOneComparisonSlot`
     (the one real untested gap), and
     `keepsComparisonAndOutingLapVideoIndependent` (the "two-run comparison
     with optional video" acceptance criterion, reframed as an
     editor-independence test once investigation showed the comparison view
     has zero video code — see "Real finding" below).
   - New doc `docs/kan42-m2-acceptance.md`: acceptance matrix plus a real
     M2 cycle-time reforecast from Jira status history (KAN-29–41): median
     75.86 min per-ticket status interval (four of those cycles are a
     disclosed ~12-hour same-day pause, not real duration), but the more
     decision-relevant number is calendar elapsed time — **~11.4 days for
     13 tickets**, ~1.1 items/day. At that rate the 68 numbered tasks left
     after step 032 would take ~60 elapsed days. Recorded for the owner to
     decide on; not something this task resolved.
   - **Real bug this PR's own CI caught**: the known-delta test's first push
     used `routeVbo(..., turns=2)`. `inferTrack()` requires ≥2 complete laps
     to resolve a route; `turns=2` on this closed-circuit fixture yields
     only 1 complete lap (OUT + 1 lap + IN), so the route never resolved and
     the comparison pair stayed empty. Every other two-run comparison test
     in the suite relies on `routeVbo()`'s default `turns=4` — switched to
     that default and it passed. Worth remembering if you ever see
     "fixture must resolve one lap from each run" again.
   - **Real finding, not a bug**: `ComparisonDetailPanel.qml` /
     `AppControllerComparison.cpp` have **zero video code**. This app has
     exactly one central video slot (`m_videoSource`/`m_sync`/
     `m_exportSourceInfo`), gated to whichever run the currently *open
     single lap* belongs to (KAN-39). Dual, side-by-side comparison video is
     explicitly separate scope (KAN-104–107 per the delivery ledger), not
     implemented by KAN-42.
2. **KAN-43 started** (M3, step 033, "Add a versioned sector and corner
   model") — branch `feature/kan-43-sector-model` off `origin/main`, Jira
   transitioned to "W toku". **Not implemented yet** — an investigation
   agent was dispatched to map the existing `ProgressAxis`/track-config/
   gate-revision/stable-lap-reference/KAN-41-persistence patterns this new
   sector model needs to match, and its report had not landed when this
   handover was written. Check `git log feature/kan-43-sector-model` and
   Jira KAN-43's comments for what actually got built before assuming
   nothing happened.

## The DrivingCoach reference (owner-supplied context for M3)

The owner pointed at a **separate repository**, `FlappedEar/DrivingCoach`
(public, cloned read-only this session to `/home/user/flappedear/drivingcoach`
via the anonymous git-read lane — re-clone if that path is gone), as prior
art for M3. It is a **Dart/Flutter mobile app**, unrelated stack, ~2,150
lines under `lib/core/analysis/`:

- `segmenter.dart` — corner/segment proposal via speed-trough detection
  (prominence + lateral-G-adjusted threshold, minimum spacing).
- `event_detector.dart` — braking/throttle-lift/throttle-return events with
  hysteresis (0.2s + 3m sustain rule).
- `opportunity_detector.dart` (368 lines) — five loss/coaching categories
  (EARLY_LIFT, EXCESSIVE_COASTING, LOW_MINIMUM_SPEED,
  LATE_THROTTLE_REAPPLICATION, IMPROVING_TECHNIQUE) with a documented
  confidence formula and evidence payload.
- `coaching_planner.dart` — prioritized, capped (≤3 item) recommendation
  output.

This is genuinely validated (real private VBO capture, lap timing within
0.06s of RaceChrono) design work directly relevant to M3's F02/F06 scope —
see `docs/analysis-model.md` and `docs/validation.md` in that repo. **It is
a design reference to port algorithms/thresholds from, not code to reuse
directly**: different language, and it normalizes laps with its own simple
median-length interpolation rather than this app's more rigorous
crossing/gap-aware `ProgressAxis` (KAN-31–33) — the port must run on top of
the existing axis, not introduce a second normalization. It also has no
sector review/edit UI and no "sector theoretical/donor lap" concept, so
KAN-43/44/45/50/56–58 still need genuine new design, just with the hardest
detection-algorithm decisions (what is a corner, what is a braking event,
how do you score confidence) already made and tested once elsewhere.

## Working conventions established this session (mostly carried over from
the prior handover, still in force)

- **Jira**: process tickets as work happens — transition to "W toku" (In
  Progress, id `21`) when starting, add a comment with the branch/PR/commit
  and what landed + verification evidence, transition to "Gotowe" (Done, id
  `41`) only after merge with the final SHA. Cloud ID:
  `315ac5b8-6fd1-4518-8b5f-4433bcc33447`. **Always disclose gaps/limitations
  in the comment** rather than silently claim full acceptance-criteria
  coverage. The Jira MCP server (v1 SSE transport) has sent a deprecation
  notice (sunset after 30 June 2026, migrate to v2/streamable-HTTP when
  prompted) — not urgent, but expect a future re-auth prompt about it.
- **GitHub**: real feature branches + PRs for planned/Jira-tracked work.
  This session pushed directly to a PR branch (amending/force-pushing) more
  than once while iterating on CI failures before the owner had reviewed
  anything — that's fine on your own unreviewed branch, never on one anyone
  else has based work on. Cloud CI (macOS Debug + Release, Qt 6.8.3)
  triggers automatically on PR push and on push to `main` — wait for it
  before considering something done, and verify the **exact head SHA**, not
  a historical run. **Never merge without the owner's explicit go-ahead**
  (a guardrail blocks `gh pr merge`/API merge from an agent session,
  respected again this session) — post the green CI evidence and wait for
  the owner to say "merged"/"done". This session subscribed to PR activity
  via the GitHub MCP tools and got woken automatically on CI
  failure/completion/merge events — use that instead of polling.
- **Coordinator workspace has no Qt/CMake toolchain** (only bare `cmake`/
  `ctest` binaries, no Qt 6 install) — all real build/test evidence comes
  from hosted macOS CI. Don't claim a test passed without an actual CI run
  link for the exact commit.
- **Build/test gate, every time, before calling anything done** (on a
  machine that actually has Qt):
  ```bash
  cmake --build build-native --parallel
  ctest --test-dir build-native --output-on-failure
  ```
- **When a new comparison-level test needs a route/track to actually
  resolve** (compatibility group, `inferTrack`, etc.), use
  `EventProjectFixture::routeVbo()`'s **default `turns=4`** unless you have
  a specific, checked reason not to — `inferTrack()` needs ≥2 complete laps,
  and lower turn counts can silently produce zero eligible comparison rows.
- **macOS only** per AGENTS.md's owner direction — don't start Windows
  builds/CI.
- **Doc types**: `handover.md` (this file) is agent-owned, rewritten every
  session. `docs/product-delivery.md` and `docs/product-vision.md` are
  owner/audit-authored — don't silently rewrite their forecasts/vision,
  flag staleness instead (this session only appended a new M2 row/reforecast
  pointer, per KAN-42's own scope). `docs/kanNN-*-acceptance.md` files are
  the per-milestone acceptance/reforecast record pattern (KAN-28 for M1,
  KAN-42 for M2) — follow that shape for a future M3 acceptance ticket
  rather than inventing a new format.

## Known disclosed gaps

- KAN-42: no interactive/real-GoPro/real-VBO verification for the three new
  tests — all synthetic fixtures and friend-class member injection, same
  pattern as KAN-39/40/41. Dual/side-by-side comparison video is
  unimplemented by design (KAN-104–107), not a KAN-42 gap.
- KAN-43: not implemented as of this handover — branch exists, Jira is "W
  toku", investigation was in flight. Do not assume any sector/corner data
  model exists in code yet; check the branch.
- M2's cycle-time reforecast is workflow-status elapsed time (includes
  CI/review/admin gaps), not pure engineering effort — see
  `docs/kan42-m2-acceptance.md` for the full caveat, including the
  disclosed same-day pause behind KAN-35–38's outlier cycle times.
- Real VBO/GoPro media validation remains outstanding across all of M2/M3
  so far — only synthetic fixtures have been exercised in automated tests.

## Key files

- `docs/kan42-m2-acceptance.md` — KAN-42's full acceptance matrix, the three
  new tests' rationale, the turns=2 bug writeup, and the M2 reforecast math.
  Read this before touching comparison tests again.
- `native/tests/TelemetryTests.cpp` — KAN-42 added
  `excludesChannelMissingFromOneComparisonSlot`,
  `comparesKnownDeltaThroughFullComparisonPipeline`,
  `keepsComparisonAndOutingLapVideoIndependent`, plus two new free helpers
  right after `writeBytes`/`readBytes`: `withSyntheticSpeedChannel()` (appends
  a synthetic extra VBO column) and `routeVboWithTimeScale()` (rescales the
  time column only, for a byte-for-byte-path known-delta fixture).
- `native/src/telemetry/TrackProgress.h/.cpp` — `ProgressAxis`,
  `buildProgressAxis`, `projectLapTrace`, `computeDeltaSeries`: the shared
  cross-lap arc-length axis KAN-43's sector boundaries must be expressed
  against (progress meters along this axis), not a second normalization.
- `native/src/telemetry/OutingLaps.cpp` — `lapCompatibilityGroupId()`
  (hash of `{version, layoutId, direction, gateRevision}`) is the existing
  precedent for a stable, config-derived identity; KAN-43's sector
  track-configuration reference should likely follow the same shape.
- `/home/user/flappedear/drivingcoach` — the DrivingCoach reference clone
  (read-only, anonymous git lane, not part of this session's authenticated
  repo scope). See "The DrivingCoach reference" section above before
  treating anything in it as portable code.

## Credentials/access already set up this session

- Jira: read/write via the `plugin:atlassian:atlassian` MCP connector,
  cloud ID above. Not durably authorized across sessions/devices — expect
  to re-authorize via `/mcp`, and possibly a v1→v2 transport migration
  prompt per the deprecation notice above.
- GitHub: authenticated for `arekkozuch/VBOOverlay` (push, PR, Actions API)
  via this session's GitHub MCP tools. `FlappedEar/DrivingCoach` is only
  attached read-only via the anonymous public-repo git lane — attach with
  `access: "push"` if you ever need to push to it (you won't; it's a
  reference only).
