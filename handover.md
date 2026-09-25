# Session handover — 2026-09-25

Written for: the next Claude Code session continuing this work (likely on
another device). Read this first, then `AGENTS.md` (checked into the repo
root — engineering rules and safety invariants for this codebase). This
file replaces the previous same-day handover — that one's work (KAN-31
through KAN-38, plus KAN-40/41) is done and merged; this covers what
happened after it, including the whole KAN-39 push.

## Project

VBOOverlay / "Flapped Ear Telemetry" — a Qt6/C++20/QML native desktop app
combining a video overlay editor with track-day telemetry analysis (VBO/RCZ
GPS+telemetry files, GoPro video). Repo: `arekkozuch/VBOOverlay` on GitHub.

**Deadline context**: the owner set a hard deadline of **2026-09-27** for a
usable milestone (M2: A/B lap comparison). Go check the actual current
date, don't trust this file's age. The owner has already said the current
`main` is "ready enough for Sunday" and made their own local copy of the
built binary as a checkpoint — so there is some schedule slack now, but
that doesn't mean scope decisions stop needing honest disclosure
(`docs/product-delivery.md` has this same value baked in, and its own
reforecast is explicitly deferred to KAN-42 using actual M2 results —
don't rewrite that document's forecast tables yourself; that reforecast is
KAN-42's job, not a docs-hygiene task).

## What just happened (this session, in order)

1. **KAN-35/36/37/38 wiring**, **KAN-41** (PR #39, merged `6e16cfe`) and
   **KAN-40** (PR #40, merged `bcb542e`) — all carried over from the prior
   handover; done before the second half of this session started. See the
   previous handover's git history if you need the detailed writeup; not
   repeated here.
2. **Owner smoke-tested the merged build interactively** and found three
   real issues, fixed and **committed directly to `main`** (not PRs — the
   owner explicitly asked to keep this lightweight and later said "commit
   everything" for a clean handover point):
   - Track-position marker dots too small (`ComparisonOverlayMap.qml`,
     10px→16px) — commit `080d4ea`.
   - The Δ-time legend only spelled out what a *positive* value means
     ("+ = A behind"), leaving negative to be inferred — easy to misread as
     "the other lap is faster" even when the math was already correct.
     Fixed the legend text. Also: Lap A's green (`#55e6a5`) and Lap B's
     blue (`#58bfff`) were reported as too similar to tell apart at a
     glance. Used the `dataviz` skill's palette validator against this
     app's actual dark chart surface — the old pair technically had enough
     hue separation but both sat in an unusually bright/pastel lightness
     band. Replaced Lap B with categorical orange `#d95926` everywhere it
     appears (chart, map, markers, lap labels), keeping Lap A's green
     untouched since it's the app's pervasive brand accent, not something
     to change for one feature — commit `143303c`.
3. **KAN-39: connect optional video evidence to lap analysis** (PR #41,
   **merged**, commit `9c8c5ec`; `main` re-verified post-merge, 9/9 suites
   green). The big lift of this session.
   - The Event/outing lap-detail view (`OutingLapDetailPanel.qml`) had zero
     video awareness before this — video only existed for the legacy
     single-project flow. Added bidirectional linkage: scrubbing the
     analysis cursor seeks video when paused; video position drives the
     cursor while playing (mutually exclusive on `playbackRunning` so they
     can't fight each other).
   - Reuses the existing central `SyncTransform`
     (`videoToTelemetryTime`/`telemetryToVideoTime` in
     `telemetry/TelemetrySession.h`) — no second, ad hoc time conversion.
   - **Deliberately scoped to same-run-only**: video is only available when
     the open lap's own run is the currently active/loaded one. A lap from
     a different run shows "video not shown — this lap is from a different
     run" rather than silently switching the active run (a heavier action
     with its own reload/generation semantics). This is a disclosed scope
     boundary, not a bug — cross-run auto-switching is real, undone
     remaining work if it's ever wanted.
   - `AppController` gains `outingLapVideoAvailable` /
     `outingLapVideoPositionMilliseconds` (real `Q_PROPERTY`s with
     `NOTIFY outingLapVideoChanged`) and
     `followOutingLapVideoPosition(videoMs)`.
   - Found and fixed two real bugs surfaced by getting existing tests green
     again (see "Two real bugs found this session" below) — a Qt Quick
     Layouts sizing quirk and a lazy-decoder-lifecycle violation.
   - New tests: `linksOutingLapVideoToActiveRunOnly`,
     `followsOutingLapVideoPositionWithinLapBounds` (both use direct
     friend-class member access to set a synthetic `MediaInfo` — no real
     decodable video file needed for the pure logic).

## Two real bugs found this session (both while getting existing tests
green again — not hypothetical, both reproduced and fixed)

1. **Qt Quick Layouts nested-width bug**, found via
   `startsOutingThroughAnalysisQml` going red. Wrapping the new video pane
   in a `ColumnLayout` (nested inside the existing `RowLayout`) caused that
   nested layout to ignore its own `Layout.preferredWidth` and silently
   expand to swallow most of the row — squeezing the channel-chart area to
   2px wide and moving a button's click target outside the test window.
   Fixed with `Layout.maximumWidth: Layout.preferredWidth` pinned onto the
   same item — verified working (geometry dumped before/after), but **the
   exact underlying Qt Quick Layouts mechanism was not root-caused**, only
   worked around. If you add another nested Layout-type child inside an
   existing Layout anywhere in this codebase and see similarly bizarre
   width behavior, this is a known, unresolved fragility — try the same
   `Layout.maximumWidth` pin first, and consider actually digging into why
   before adding a third instance of the workaround.
2. **Lazy-decoder-lifecycle violation**, found via `flappedear_startup_smoke`
   failing ("media players closed=1 open=3 released=1", expected
   closed=1/open=2/released=1). A dedicated smoke test asserts the analysis
   window's `MediaPlayer` is only ever created when actually needed, not
   just because the analysis window is open. The new video-pane
   `MediaPlayer` was being created eagerly. Fixed by wrapping it in a
   `Loader { active: root.visible }` (`root` = the panel itself, visible
   only when a lap is actually open). If you add a `MediaPlayer` anywhere
   in this codebase, check this test still passes — it's easy to
   accidentally violate without any other signal that something's wrong.

## Immediate next step

**Nothing pending.** PR #41 (KAN-39) merged into `main` at commit `9c8c5ec`;
`main` is fast-forwarded, working tree clean, local feature branch deleted,
9/9 suites re-verified post-merge, KAN-39 transitioned to Done in Jira with
the closing comment already posted. `git status --short --branch` should
show `main` even with `origin/main` and nothing else.

**KAN-42 (accept A/B comparison + reforecast) is the only M2 ticket left**,
and all three of its dependencies (KAN-39, KAN-40, KAN-41) are now done. It
needs real crossings/gaps/different-lines/missing-sensor/known-delta
fixture coverage plus two-run comparison with optional video, and is
explicitly where `docs/product-delivery.md`'s forecast gets reconciled
against actual M2 results (that reforecast is part of KAN-42's own scope,
not a docs-hygiene task to do separately). **Check with the owner before
starting it** — the deadline is close, they've already said the current
build is an acceptable Sunday checkpoint and made their own binary copy,
and they may prefer to spend the remaining time differently (more
interactive bug-hunting on the existing build, or nothing further until
after the track day).

## Working conventions established this session (mostly carried over from
the prior handover, still in force)

- **Jira**: process tickets as work happens — transition to "W toku" (In
  Progress, id `21`) when starting, add a comment with the branch/PR/commit
  and what landed + verification evidence, transition to "Gotowe" (Done, id
  `41`) only after merge with the final SHA. Cloud ID:
  `315ac5b8-6fd1-4518-8b5f-4433bcc33447`. **Always disclose gaps/limitations
  in the comment** rather than silently claim full acceptance-criteria
  coverage.
- **Atlassian MCP auth is session-scoped, not durable**: has needed
  re-authorizing via `/mcp` more than once across this session's history.
  If Jira tools aren't in the tool list on a fresh device/session, tell the
  owner rather than giving up on Jira updates.
- **GitHub**: real feature branches + PRs for planned/Jira-tracked work;
  direct small commits to `main` are acceptable for owner-requested
  interactive-testing fixes when the owner says so explicitly (see KAN-39
  section above) — this is a judgment call each time, not a blanket rule.
  `gh auth status` already logged in as `arekkozuch`. Cloud CI (macOS
  Debug + Release, Qt 6.8.3) triggers automatically on PR push — wait for
  it (`gh pr checks <n> --watch` or a background Monitor loop) before
  considering something done. **Never merge without the owner's explicit
  go-ahead** — a guardrail blocks `gh pr merge` from an agent session, hit
  and respected (not routed around) repeatedly across this session's
  history; just wait for the owner to say "merged"/"done".
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
  to explore/find bugs, and killing their instance would lose their
  in-progress state. During the KAN-39 work an owner instance was running
  throughout, so no manual interactive launch was possible — the automated
  suite (which does exercise the new QML end-to-end, including a
  zero-QML-warnings assertion) was the only verification. Say so plainly
  rather than claiming interactive confirmation that didn't happen.
- **When the owner interactively finds a bug or requests a tweak while
  running the app themselves**: fix it, rebuild, run the full test gate,
  but **don't assume commit/PR ceremony is wanted immediately** — ask, or
  wait for them to say they're done iterating.
- **macOS only** per AGENTS.md's owner direction — don't start Windows
  builds/CI.
- **When asked "do we have our docs up to date", distinguish doc types.**
  `handover.md` is this file — agent-owned, should be rewritten every
  session. `docs/product-vision.md` and `docs/product-delivery.md` are
  owner/audit-authored with their own stated update cadence (vision is a
  durable contract; delivery's forecast is explicitly deferred to KAN-42) —
  don't silently rewrite their forecasts or vision statements yourself;
  flag staleness to the owner instead of "fixing" it unilaterally.
  `docs/testing.md`/`docs/development-workflow.md` are stable process
  references, rarely need changes.

## Known disclosed gaps (don't silently claim these are done)

- KAN-39: no interactive/real-GoPro-hardware verification — no real video
  was decoded; tests use a synthetic `MediaInfo` via direct friend-class
  member access. The `AnalysisPanel` chart's own click-to-scrub path wasn't
  separately exercised for video-seek (should work transitively through
  the same `outingLapCursor` the section-time slider already uses, but has
  no dedicated test). Cross-run auto-switching is unimplemented **by
  design** (see PR #41 description), not just untested.
- KAN-40/41: no interactive manual verification of the live compare view
  was done by the agent (needs a real project with an A/B pair loaded) —
  the owner did this themselves earlier this session and found the
  marker-size/color issues fixed above, which is itself evidence the
  disclosed gap was real and worth flagging, not just a formality.
- KAN-32 (older): no figure-eight/parallel-section fixtures built on the
  shared `EventProjectFixture::routeVbo()` GPS generator — ambiguity/
  heading tests use a small hand-built synthetic axis instead.
- KAN-35 (older): no "measured vs. calculated" provenance field exists
  anywhere in `TelemetryChannel`/`TelemetrySession`; discrete-channel
  (gear) interpolation isn't integration-tested.
- Real VBO/GoPro media validation is still outstanding across all of M2 —
  only synthetic fixtures have been exercised in automated tests. Report
  real-media results separately per AGENTS.md when that happens.

## Key files

- `native/src/app/AppControllerOuting.cpp` — outing lap detail lifecycle
  (`selectOutingLap`/`closeOutingLap`/`setOutingLapCursor`), now also
  `outingLapVideoAvailable`/`outingLapVideoPositionMilliseconds`/
  `followOutingLapVideoPosition` (KAN-39). `outingLapVideoChanged` is
  emitted both at specific mutation points (lap open/close, cursor move)
  and via two catch-all `connect()`s in `initializeOutingLaps()`
  (`documentStateChanged`, `sourceLoadStateChanged`) — if you add a new way
  the active run, video source, or sync transform can change, prefer
  relying on those catch-alls over threading a new specific emit through
  every call site.
- `native/qml/OutingLapDetailPanel.qml` — now has a lazily-loaded
  (`Loader { active: root.visible }`), muted, position-mirrored
  `MediaPlayer` following the exact pattern `AnalysisWindow.qml`'s legacy
  video pane already used. See the two bug writeups above before touching
  its layout or adding another `MediaPlayer` anywhere in the app.
- `native/src/app/AppControllerComparison.cpp` — comparison feature logic:
  slot lifecycle, `comparisonSlots()` (per-slot `statusText`/
  `statusIsWarning`, KAN-40), `persistComparisonRange`/
  `persistComparisonChannels` (KAN-41), `ensureComparisonSharedGeometry`/
  `ensureComparisonProgressAxis` (memoized, request-id-gated).
- `native/src/app/AppController.h` — `comparisonProgressAxisLength` and
  `outingLapVideoAvailable`/`outingLapVideoPositionMilliseconds` are real
  `Q_PROPERTY`s with `NOTIFY`, not `Q_INVOKABLE` + a forced-dependency
  hack — that pattern caused a real binding-loop bug earlier this session
  (see prior handover / KAN-40 PR description) and should not be
  reintroduced.
- `native/qml/ComparisonDetailPanel.qml` — the full A/B compare view. Any
  new reactive binding here that depends on `comparisonSlotsChanged`
  (directly or via `root.slots`) should prefer reading a plain field
  already present on `comparisonSlots()` over adding a new QML-side JS
  function that re-reads `root.slots` from multiple places.
- `native/qml/ComparisonOverlayChart.qml` / `ComparisonOverlayMap.qml` —
  Lap A = `#55e6a5` (green, app brand accent), Lap B = `#d95926` (orange,
  changed this session from a light blue that was too similar to green).
  If you ever need a third series color anywhere in the comparison
  feature, run it through the `dataviz` skill's validator against this
  app's actual dark surfaces (`#090e14`/`#070b10`) first, don't eyeball it.
- `native/tests/TelemetryTests.cpp` — see
  `linksOutingLapVideoToActiveRunOnly`/
  `followsOutingLapVideoPositionWithinLapBounds` (KAN-39, direct
  friend-class member access to `m_videoSource`/`m_sync`/
  `m_exportSourceInfo` for deterministic video-bound tests without a real
  file), `showsComparisonSlotCompatibilityAndCoverageContext` (KAN-40) and
  `restoresComparisonRangeAndChannelsAfterReopen` (KAN-41).
- `native/tests/EventProjectTests.cpp` —
  `boundsAndPreservesAnalysisDecisions` covers malformed-input rejection
  for `comparisonRange`/`comparisonChannels` (KAN-41).

## Credentials/access already set up this session

- Jira: read/write via the `plugin:atlassian:atlassian` MCP connector,
  cloud ID above. Not durably authorized across sessions/devices, unlike
  GitHub — expect to re-authorize via `/mcp`.
- GitHub: `gh` authenticated as `arekkozuch` (gist, read:org, repo,
  workflow scopes). Git push over HTTPS also works via the macOS keychain
  credential helper independently of `gh`'s own auth.
