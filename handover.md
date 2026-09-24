# Session handover — 2026-09-24 (evening)

Written for: the next Claude Code session continuing this work (likely on
another device). Read this first, then `AGENTS.md` (checked into the repo
root — engineering rules and safety invariants for this codebase). This
file replaces the previous same-day handover — that one's work (KAN-31
through KAN-38) is done and merged; this covers what happened after it.

## Project

VBOOverlay / "Flapped Ear Telemetry" — a Qt6/C++20/QML native desktop app
combining a video overlay editor with track-day telemetry analysis (VBO/RCZ
GPS+telemetry files, GoPro video). Repo: `arekkozuch/VBOOverlay` on GitHub.

**Deadline context**: the owner set a hard deadline of **2026-09-27** for a
usable milestone (M2: A/B lap comparison). Go check the actual current
date, don't trust this file's age — 3 days away as of this writing. Time
pressure is real; keep scope decisions honest and disclosed rather than
silently narrowing them (`docs/product-delivery.md` has this same value
baked in).

## What just happened (this session, in order)

1. **KAN-35/36/37/38 wiring** (PR #38, merged, commit `f818c08`) — carried
   over from the prior handover; already done when this session started.
2. **KAN-41: persist comparison range and channel selection** (PR #39,
   merged, commit `6e16cfe`) — completed the groundwork commit `c98d27e`
   had explicitly deferred. Added `analysisDecisions.comparisonRange`
   ({startMeters, endMeters}, validated: finite, non-negative, start < end,
   capped at 1,000,000m) and `analysisDecisions.comparisonChannels` (≤4
   bounded channel names, no duplicates). `ComparisonDetailPanel.qml`
   restores persisted range/channels once when a pair's axis/available
   channels first become valid after a document (re)opens; re-armed on
   view close (including the forced close on new document load).
3. **KAN-40: compatibility/exclusion/GPS-coverage context in the compare
   view** (PR #40, merged, commit `bcb542e`) — the full-screen compare view
   previously gave zero context when a slot wasn't usable. Added a per-slot
   status line (compatibility group, GPS coverage/`referenceIssue`,
   exclusion + reason, other compatibility reasons, or the slot's error
   message), computed in `AppController::comparisonSlots()` as plain
   `statusText`/`statusIsWarning` fields — always-visible text, never
   behind a mouse-only hover/tooltip.
4. **Found and fixed two real QML re-entrancy bugs while writing KAN-40's
   test** (bundled into PR #40, not separate tickets):
   - `comparisonProgressAxisLength` was a `Q_INVOKABLE` read via a
     comma-operator "force a dependency" hack (dating to the original
     KAN-35-38 PR). Bisection proved merely inserting *any* additional
     Layout-managed sibling into `ComparisonDetailPanel.qml` — even fully
     static, content-unrelated — tripped a genuine "Binding loop detected"
     QML warning. Fixed by making it a real `Q_PROPERTY` with
     `NOTIFY comparisonSlotsChanged`.
   - KAN-41's `persistComparisonRange`/`persistComparisonChannels` calls,
     made synchronously from `onZoomStartChanged`/`onZoomEndChanged`/
     `onVisibleChannelsChanged`, recursed back into
     `invalidateComparisonLaps` → `comparisonSlotsChanged` while the
     triggering binding was still on the call stack. Fixed by deferring
     those calls with `Qt.callLater` (which also usefully coalesces a
     zoom-drag's many change events into one write).
   - Both are covered by the new test
     `TelemetryTests::showsComparisonSlotCompatibilityAndCoverageContext`.
5. **Owner smoke-tested the merged build interactively** and asked for
   bigger track-position marker dots in the A/B comparison map
   (`ComparisonOverlayMap.qml`) — done, **not yet committed** (see
   "Immediate next step").

## Immediate next step

**Nothing uncommitted.** The marker-size tweak below was committed directly
to `main` (not a feature branch/PR — the owner explicitly asked to "commit
everything" for a clean handover) as `080d4ea` and pushed to `origin/main`.
Working tree is clean; `main` is even with `origin/main`.

```diff
--- a/native/qml/ComparisonOverlayMap.qml
+++ b/native/qml/ComparisonOverlayMap.qml
@@ -82,11 +82,12 @@ Rectangle {
                 readonly property var point: root.hoverDistanceMeters >= 0
                     ? appController.comparisonPositionAtProgress(index, root.hoverDistanceMeters) : ({})
                 visible: point.x !== undefined
-                width: 10
-                height: 10
-                radius: 5
+                width: 16
+                height: 16
+                radius: 8
                 color: index === 0 ? "#55e6a5" : "#58bfff"
                 border.color: "#0c150f"
+                border.width: 2
                 x: Number(point.x || 0) * mapArea.width - width / 2
                 y: Number(point.y || 0) * mapArea.height - height / 2
             }
```

Track-position marker dots in the shared A/B map, bumped 10px→16px
(radius 5→8) with a heavier border, per the owner's direct request while
interactively testing the merged build. Verified before commit: `cmake
--build build-native --parallel` + `ctest --test-dir build-native
--output-on-failure`, 9/9 suites green (cosmetic QML-only change, no test
asserts marker size specifically). **Not yet visually confirmed by the
owner** — their running instance (PID 16797 at handover time) was still on
the pre-change binary when this was committed, since QML changes need an
app relaunch, not just a rebuild, to take effect. If the very first thing
you hear from the owner is that the dots still look small, check they
relaunched from the freshly built `build-native/native/Flapped Ear
Telemetry.app` before assuming the fix didn't take.

**This was a "find bugs interactively" pass, not a Jira-tracked task** — no
ticket exists for it. If the owner reports more small cosmetic/interactive
issues found this way, the established pattern from this session is: fix,
rebuild, run the full test gate, then ask whether to commit immediately or
batch with other small fixes — don't assume either way.

## Working conventions established this session (mostly carried over from
the prior handover, still in force)

- **Jira**: process tickets as work happens — transition to "W toku" (In
  Progress, id `21`) when starting, add a comment with the branch/PR/commit
  and what landed + verification evidence, transition to "Gotowe" (Done, id
  `41`) only after merge with the final SHA. Cloud ID:
  `315ac5b8-6fd1-4518-8b5f-4433bcc33447`. **Always disclose gaps/limitations
  in the comment** rather than silently claim full acceptance-criteria
  coverage.
- **Atlassian MCP auth is session-scoped, not durable**: this session had
  to re-authorize the `plugin:atlassian:atlassian` MCP connector via
  `/mcp` partway through (it started unauthorized). If Jira tools aren't
  in the tool list on a fresh device/session, tell the owner rather than
  giving up on Jira updates — they'll need to authorize it again.
- **GitHub**: real feature branches + PRs, not direct pushes to `main`
  (`gh auth status` already logged in as `arekkozuch`). Cloud CI (macOS
  Debug + Release, Qt 6.8.3) triggers automatically on PR push — wait for
  it (`gh pr checks <n> --watch` or a background Monitor loop) before
  considering something done. **Never merge without the owner's explicit
  go-ahead** — a guardrail blocks `gh pr merge` from an agent session, and
  this was hit and respected (not routed around) for PRs #36/#37/#38
  historically; this session didn't attempt it and just waited for the
  owner to say "merged"/"done" each time.
- **Local commits only, until a PR is warranted**: for a self-contained
  bounded item, commit locally first on a feature branch off synced
  `main`, verify build+tests, push, open the PR.
- **Build/test gate, every time, before calling anything done**:
  ```bash
  cmake --build build-native --parallel
  ctest --test-dir build-native --output-on-failure
  ```
  9 suites, must be 9/9 green. Redirect output to a file and grep for
  "error:" rather than piping through `tail` (a real compile failure was
  once reported as exit 0 that way).
- **Manual smoke test before claiming a QML change works**: launch the app
  directly (`build-native/native/Flapped Ear Telemetry.app/Contents/MacOS/
  Flapped Ear Telemetry`), check stderr for QML warnings, quit cleanly.
  Single-instance-locked — check `pgrep -f "Flapped Ear
  Telemetry.app/Contents/MacOS"` first and **never kill a process this
  session didn't start** — the owner runs the app interactively themselves
  to explore/find bugs (as happened this session), and killing their
  instance would lose their in-progress state. If a pgrep hit isn't one you
  launched, don't touch it — just tell the owner a relaunch is needed to
  see a change.
- **When the owner interactively finds a bug or requests a tweak while
  running the app themselves**: fix it, rebuild, run the full test gate,
  but **don't assume commit/PR ceremony is wanted immediately** — ask, or
  wait for them to say they're done iterating, especially for small
  cosmetic changes. This differs from the Jira-task workflow above, which
  is for planned M2 backlog items.
- **macOS only** per AGENTS.md's owner direction — don't start Windows
  builds/CI.

## Known disclosed gaps (don't silently claim these are done)

- KAN-40/41: no interactive manual verification of the live compare view
  was done by the agent (needs a real project with an A/B pair loaded) —
  the owner did this themselves this session and found the marker-size
  issue, which is itself evidence the disclosed gap was real and worth
  flagging, not just a formality.
- KAN-32 (older): no figure-eight/parallel-section fixtures built on the
  shared `EventProjectFixture::routeVbo()` GPS generator — ambiguity/
  heading tests use a small hand-built synthetic axis instead.
- KAN-35 (older): no "measured vs. calculated" provenance field exists
  anywhere in `TelemetryChannel`/`TelemetrySession`; discrete-channel
  (gear) interpolation isn't integration-tested.
- Real VBO/GoPro media validation is still outstanding across all of M2 —
  only synthetic fixtures have been exercised in automated tests. Report
  real-media results separately per AGENTS.md when that happens.

## Remaining M2 scope (per the original plan, not yet started)

- **KAN-39** (029 — connect optional video evidence to analysis and run
  synchronization): depends on KAN-38 (done) and KAN-17 (done, older).
  Bigger/riskier under time pressure — touches `PreviewPlayback.cpp`,
  `TelemetrySyncEngine.cpp`, `AnalysisWindow.qml`.
- **KAN-42** (032 — accept A/B comparison and reforecast after M2, High
  priority): depends on KAN-39, KAN-40 (done), KAN-41 (done). This is the
  acceptance/reforecast ticket for the whole milestone — needs real
  crossings/gaps/different-lines/missing-sensor/known-delta fixture
  coverage plus two-run comparison with optional video, so it can't
  meaningfully start until KAN-39 lands.
- Given the 2026-09-27 deadline (2-3 days out depending on when you read
  this), the owner already agreed earlier in this engagement that if time
  runs short, persistence (KAN-41, now done) beat video linkage (KAN-39)
  in priority. KAN-39 is now the natural next planned task, but check with
  the owner first given how close the deadline is — they may prefer to
  scope down, or may want KAN-42's fixture work started in parallel against
  what already exists.

## Key files

- `native/src/app/AppControllerComparison.cpp` — comparison feature logic:
  slot lifecycle, `comparisonSlots()` (now also computing per-slot
  `statusText`/`statusIsWarning`, KAN-40), `persistComparisonRange`/
  `persistComparisonChannels`/`comparisonPersistedRangeMeters`/
  `comparisonPersistedChannels` (KAN-41), `ensureComparisonSharedGeometry`/
  `ensureComparisonProgressAxis` (memoized, request-id-gated).
- `native/src/app/AppController.h` — note `comparisonProgressAxisLength` is
  now a real `Q_PROPERTY` (fixed this session; was a `Q_INVOKABLE` +
  comma-hack before — don't revert that without re-reading the KAN-40 PR
  description's binding-loop bisection writeup).
- `native/qml/ComparisonDetailPanel.qml` — the full A/B compare view:
  shared zoom/pan (`zoomStart`/`zoomEnd`/`totalMeters`), channel picker
  (`visibleChannels`), the `pendingRangeRestore`/`pendingChannelsRestore`
  one-shot restore flags (KAN-41), the `comparisonSlotStatusList`/
  `comparisonSlotStatusRepeater` per-slot status labels (KAN-40). Any new
  reactive binding added here that depends on `comparisonSlotsChanged`
  (directly or via `root.slots`) should prefer reading a plain field
  already present on `comparisonSlots()` over adding a new QML-side JS
  function that re-reads `root.slots` from multiple places — that shape of
  fan-out is what caused the binding-loop bug this session.
- `native/qml/ComparisonOverlayMap.qml` — shared A/B track map; position
  markers now 16px (was 10px) per the owner's live-testing feedback,
  **uncommitted** as of this handover.
- `native/tests/TelemetryTests.cpp` — see
  `showsComparisonSlotCompatibilityAndCoverageContext` (KAN-40, also proves
  the binding-loop fix) and `restoresComparisonRangeAndChannelsAfterReopen`
  (KAN-41) for the patterns used to drive `ComparisonDetailPanel.qml`
  through `QQmlComponent` in a test (load via `ANALYSIS_PANEL_QML_PATH` as
  base URL, `findChild`/`itemAt` on `Repeater`s, `QSignalSpy` on
  `QQmlEngine::warnings` asserted to `0`).
- `native/tests/EventProjectTests.cpp` —
  `boundsAndPreservesAnalysisDecisions` covers malformed-input rejection
  for `comparisonRange`/`comparisonChannels` (KAN-41).

## Credentials/access already set up this session

- Jira: read/write via the `plugin:atlassian:atlassian` MCP connector,
  cloud ID above. Had to be (re-)authorized mid-session via `/mcp` — it
  is **not** durably authorized across sessions/devices, unlike GitHub.
- GitHub: `gh` authenticated as `arekkozuch` (gist, read:org, repo,
  workflow scopes). Git push over HTTPS also works via the macOS keychain
  credential helper independently of `gh`'s own auth.
