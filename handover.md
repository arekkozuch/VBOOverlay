# Session handover — 25 September 2026 (late)

Written for: the next Claude Code session continuing this work. Read this
first, then `AGENTS.md` (engineering rules and safety invariants).

**Several sessions work this repo, sometimes at the same time** (a
coordinator workspace without a Qt toolchain did KAN-42–54; another session
merged PR #56). Before starting, run `git fetch origin && git log --oneline
origin/main -20` and check Jira for tickets you assume are still open. Trust
that check over this file.

## Project

VBOOverlay / "Flapped Ear Telemetry": a Qt 6 / C++20 / QML macOS desktop app
combining a video overlay editor with track-day telemetry analysis (VBO/RCZ,
GoPro). Repo `arekkozuch/VBOOverlay`. Jira project KAN, cloud ID
`315ac5b8-6fd1-4518-8b5f-4433bcc33447`.

## Where M3/M4 stand (end of this session)

Merged to `main` today, all with hosted macOS Debug + Release green:

| Ticket | What | PR |
| --- | --- | --- |
| KAN-56 | Sector theoretical best across a compatible population, donor lap per sector | #57 |
| KAN-57 | Theoretical-best view, donor → Corner Analyzer | #58 |
| KAN-58 | M3 acceptance record `docs/kan58-m3-acceptance.md` (synthetic) | #59 |
| KAN-59 | Non-overlapping time-loss windows (`TimeLoss.h`) | #60 |
| KAN-60 | Ranked time losses (Day results → Time losses…) | #61 |
| KAN-61 | Loss → Corner Analyzer navigation, lap A/B "here" with video | #62 |
| KAN-116 | Connected corners proposed as one corner chain (proposal algorithm v2) | #63 |
| KAN-117 | Corner Analyzer usable on real recordings (side column, speeds, fixes) | #64 |
| KAN-118 | Accelerator pedal, not throttle plate, is the `throttle` alias | #65 |
| KAN-119 | Imported runs named "Session N" in recording order | #66 (merging when written) |
| KAN-120 | "Where your best lap can improve" map; gate-crossing segments timed | #67 (merging when written) |

KAN-116–120 came from the **owner testing the real Jastrząb day**
(`jastrzab/`, git-ignored). That testing exposed problems synthetic fixtures
never showed, so develop against real data from now on. See "Real data"
below.

Next M4 tickets: KAN-62 (eligible populations and timing consistency) and
onward. `docs/kan58-m3-acceptance.md` still needs its real-track section
filled in from the owner's day at the track (27 September 2026).

## Owner direction and preferences (this session)

- **Merging:** the owner authorised merging for this session ("you can
  merge — keep working until I say stop"). Without that, never merge; post
  green CI and wait. See memory `feedback-merge-and-ship-loop`.
- **The owner reads the app as a driver.** Tables of numbers are not enough.
  Lead with the best lap and where time is, on the track map. Say "Session 3 ·
  LAP 2", not filenames. Times of a minute or more are `m:ss.mmm`
  (`AppController::formatElapsedTime`).
- **Two apps:** the owner is considering splitting the product into a desktop
  video-overlay app and a (tablet-capable) analysis app. I recommended two
  apps on the shared `flappedear_core` library, as the milestone after M4.
  No epic has been created yet; wait for the owner's go-ahead.
- One focused PR per ticket; the owner creates or asks for Jira tickets for
  feedback-driven work (KAN-116–120 were created this way).

## Real data

```bash
FLAPPEDEAR_REAL_DAY="$PWD/jastrzab" FLAPPEDEAR_CORNER_REVIEW_DIR=/some/scratch/dir \
  ./build-native/native/tests/flappedear_native_tests analyzesPrivateTrackDayCorners
```

It imports the six recordings, approves every proposal on the best lap's
run (without a manual split, as a driver would), prints proposals,
theoretical best, ranked losses and per-segment metrics, and saves
screenshots of the real Analysis window, the Corner Analyzer and the
theoretical-best window. **Look at the screenshots** before claiming UI
work is done. Last result: 1:49.898 best → 1:47.905 theoretical, 1.993 s
available.

Facts learned from the owner's car (RaceChrono Pro VBO/RCZ with OBD):
`accelerator_pos-obd` is the pedal (0–100 %); `throttle_pos-obd` is the
plate (13.3 % idle, 80.4 % fully open, fully open from about 70 % pedal,
rev-match blips on downshifts with the pedal at 0). Speed is `velocity`
(km/h), brake is `brake_pos-obd`.

## Known gaps and traps

- **VBO channel units are not recorded.** RaceChrono declares them in
  `[header]`, but `channel.unit` is part of the recording fingerprint
  (`ProjectSourceReference.cpp`). Adding units would make every saved project
  demand a relink, so it needs a fingerprint migration first. Speeds therefore
  show without a unit.
- **Canonical segmentation:** segments are approved per run. The theoretical
  best, the loss ranking and the Corner Analyzer, when opened from them, use
  the lowest-run-ID eligible run with approved segments. The axis is built
  from that run's fastest lap.
- Corner/braking/exit metrics for a gate-crossing segment stay unavailable;
  only its sector time is computed.
- **Stacked PRs:** `gh pr merge --delete-branch` on a base branch
  **auto-closes** PRs stacked on it (GitHub did not retarget them). Point
  every PR at `main` before merging, and delete branches only at the end.
- QML: under `pragma ComponentBehavior: Bound`, qualify child reads through
  an explicit `id`. `Dialog.result` is a FINAL property, so don't name a
  property `result`. `slots` is a Qt macro in C++ tests. ListView delegates
  are not reachable with `findChild`; use `itemAtIndex`. Register every new
  QML file in `native/CMakeLists.txt`'s `QML_FILES`, or startup smoke fails.

## Working conventions (still in force)

- Jira: "W toku" (transition `21`) when starting; comment with branch, PR,
  what landed, evidence and gaps; "Gotowe" (`41`) after merge with the final
  SHA and green main CI. All Jira content in English.
- Build/test gate every time: `cmake --build build-native --parallel` and
  `ctest --test-dir build-native --output-on-failure` (20 suites). Reconfigure
  with `cmake -S . -B build-native -DCMAKE_BUILD_TYPE=Debug
  -DCMAKE_PREFIX_PATH=/opt/homebrew/opt/qt` after adding source files.
- Cloud CI: macOS Debug + Release on every PR push and on push to `main`
  (the main concurrency group cancels superseded runs, so verify the latest
  main SHA). macOS only; no Windows work.
- Documentation ships in the same PR (`docs/testing.md`,
  `docs/telemetry-semantics.md`, the implementation column of
  `docs/product-delivery.md`). `docs/product-vision.md` and delivery forecasts
  are owner-authored: flag staleness rather than rewriting them.
- Never kill an app process you didn't start; the owner may be using it.

## Credentials/access

- Jira via the `plugin:atlassian:atlassian` MCP connector; expect to
  re-authorize with `/mcp` in a new session.
- GitHub: `gh` is authenticated for `arekkozuch/VBOOverlay`.
