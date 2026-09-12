# Flapped Ear Telemetry — audit and delivery ledger

Audit date: 12 September 2026. Product scope: [product contract](product-vision.md).
This is the current delivery authority; old checkpoints and narrow beta documents
must not override it. Feature implementation is not the same as runtime acceptance.

## Baseline and incoming work

- Audited main: `d7e195e`, containing PRs #7–#10 (events, import and UI/G fixes).
- Incoming PR #11: `2e973b5`, chronological whole-outing import/list/detail. It is
  included in this audit so it is not assigned again. At audit time macOS CI run
  `34711964074` crashes in `startsOutingThroughAnalysisQml` on Qt 6.8.3. Root cause:
  offscreen QPA supplies synthetic ID 1, while Metal expects a native NSView.
  Follow-up `8aeb572` uses native Cocoa/Windows QPA for real window interaction,
  retaining QRhi and pixel/export assertions; execution evidence is tracked in
  [PR #11](https://github.com/arekkozuch/VBOOverlay/pull/11). Local
  Qt 6.11.1 success recorded in that PR is not a pass for the CI configuration.
- Current coordinated work: repair that blocker, harden GPS-reference eligibility,
  correct production export-log retention and reconcile product documentation.
- This Work environment has no CMake/Qt. Attempting the native commands cannot
  establish a local pass; use exact-head CI and record physical Mac acceptance
  separately. Private telemetry files must not be committed.

## Actual capability audit

| IDs | State at audited baseline including incoming PR11 | Evidence / next missing result |
| --- | --- | --- |
| F00 | Partial | EventProjectCodec v3 + AppControllerImport; PR11 adds dated pairing and whole-outing workflow. Folder/drop, run metadata editing and source alternatives on existing runs remain |
| F01 | Partial | LapTiming, live comparison tiles, PR11 single-section inspection. No independent A/B distance comparison or compatible event ranking |
| F02 | Missing | Start gate exists; no reviewed sector/corner model or detection |
| F03 | Missing | No ranked time-loss observations or evidence navigation from a result |
| F04 | Missing | Neither sector theoretical nor realistic potential exists |
| F05 | Partial | TrackMapPanel + PR11 detail cursor. No paired traces or delta/channel layers |
| F06–F09 | Missing | No corner metrics, coasting, driving-state or trail-braking analysis |
| F10 | Missing | No analysis population, variability or consistency metrics |
| F11 | Partial foundation | PR11 chronological sections; no run-performance progression or multi-event history |
| F12–F13 | Raw channels only | Recorded OBD values can be plotted; no thermal aggregates, recovery or correlations |
| F14 | Partial visualization | G ball/radar/bar and VBO calculated G aliases; no analytical G-G scatter |
| F15 | Partial | Single-video central sync + independent video-free lap detail; no comparison video or GoPro chapter assembly |
| F16 | Partial foundation | Recorded GPS/OBD/HR coexist; alternative files persist, but no actual cross-file channel fusion |
| F17 | Raw channels only | HR imported/plotted; no lap/run/segment physiological summaries |
| F18–F19 | Missing | No automatic report or computed explanation pipeline |
| F20 | Substantial implementation | WidgetModel, shared TelemetryScene/FrameRenderer, export transactions/recovery; remaining hardening and exact Mac candidate acceptance |

PR11's independent verified-source loader, bounded row service and detail view
must be extended, not replaced with another summary-loading architecture.

## Correctness blockers and ownership

| ID | Finding | Required closure / current action |
| --- | --- | --- |
| C01 | PR11 Qt6.8 Mac QML crash | Fix lifecycle/compatibility cause; retain UI coverage and production renderer tests; CI must pass before integration |
| C02 | Flat best-lap trace bridges missing GPS | Implemented in PR12: preserve timings, exclude invalid references before trace construction/ranking, expose reason/no-delta; focused tests await exact-head verification |
| C03 | Log retention ignores canonical UUIDs | Implemented in PR12: match actual IDs, preserve unrelated/active files and symlinks; focused tests await exact-head verification |
| C04 | Unix descendants outlive group leader | Supervise group through leader exit and stop descendants before owned-artifact cleanup; regression for TERM-resistant descendant; open |
| C05 | VBO bulk split/derived time budgets | Bound before allocation and reject nonfinite/overflow derived times; open |
| C06 | Coordinate interpretation ambiguity | Explicit exporter evidence near equator/prime meridian; open |
| C07 | Slow/full export destination behavior | Exercise cancellation, scan and transaction cleanup under slow/filling volume; open |
| C08 | User-visible name/package drift | Coordinate display/bundle/package changes to Flapped Ear Telemetry, preserving internal storage identity; open |

Event ranking needs additional compatibility and exclusion rules beyond C02.
Do not present a GPS-continuous interval as proof of a comparable racing lap.

## Dependency-ordered delivery

Ranges are rough engineering effort for focused work, including implementation,
regressions and integration. They are not elapsed-time guarantees and are not
divided by agent count: critical algorithm and acceptance work is sequential.

| Milestone | User-visible completion | Acceptance / dependencies | Effort forecast |
| --- | --- | --- | --- |
| M0 — regain a reliable baseline | Existing outing workflow integrated; master vision/status truthful | PR11 green, C01–C03 closed; other blockers explicitly retained | 1–3 working days |
| M1 — day results | Best eligible run/day, run progression, notes/conditions and exclusions | Reuse outing service; compatible layout/direction/gate groups; missing files and GPS incomplete states; persistence/recovery | 2–4 days |
| M2 — comparison evidence | Independent A/B, distance delta, speed/available channels, two traces, common cursor | M1; deterministic shared track-progress alignment including crossings/gaps; no editor mutation | 6–10 days |
| M3 — corner analysis | Automatic sector/corner proposals with review/editing, entry/apex/exit/braking metrics, sector theoretical | M2; stable editable boundaries, metric prerequisites/provenance and downstream straight effects | 7–12 days |
| M4 — useful conclusions | Ranked losses, consistency, G-G, available thermal/HR summary and clickable report | M3; non-overlapping losses, sample counts, missing-data semantics, measured/inferred distinction | 5–9 days |
| M5 — complete core acceptance | A tested Mac app covering the core product journey and overlay export | M4 + C04–C08; full-day/private-video walkthrough, reopen/recovery, installed candidate, short/lap/full exports | 4–7 days plus external access |
| M6 — remaining original vision | Realistic potential, expanded state/coasting/trail analysis, thermal correlation, multi-event history, fusion, chapter/comparison video and explanations; remaining F00 folder/drop, reusable vehicle/track references and alternatives on existing runs; remaining F05 channel map layers | Individually validated algorithms, source/clock provenance; explicit acceptance per F00–F20; earlier core remains usable | Additional 25–45 days, low confidence |

First core workflow forecast: approximately **25–45 focused working days** from
this baseline. Full original vision: roughly **50–90 working days total**. These
are planning ranges, not a claim that autonomous work continues between turns.
A defensible calendar date needs a demonstrated execution rate and Mac acceptance
availability. Reforecast after M1 and M2 using actual cycle time; report scope
changes explicitly rather than silently dropping F00–F20 items to meet a date.

The owner's near-term benefit arrives incrementally: PR11 gives day/lap inspection;
M1 gives day results; M2 gives actionable comparison; M4 gives the original
loss-to-corner-to-evidence experience. None is called full completion prematurely.

## Whole-product acceptance record

For each milestone record commit, PR, exact CI head/test-merge SHA, executed test
counts and limitations. For M5 also record candidate archive hash and Mac/Qt/OS.

- Import all provided runs, paired exports, malformed/duplicate files and cancel.
- Save/reopen/move/relink the whole event; preserve IDs, notes, choices and sync.
- Select laps from different compatible runs without video. Show unavailable
  metrics honestly and exclude incomplete data from best/potential calculations.
- Follow a report loss into its corner, delta, map and channel evidence.
- Attach matching video; verify sync and editor/analysis independence; render
  short/lap/full supported SDR outputs; test cancel/failure with existing targets.
- Inspect minimum-size Mac UI, keyboard navigation, no-data/loading/error states
  and installed startup. Preserve explicit hardware/private fixture skips.
- Public distribution/signing is a separate authorization and acceptance record;
  existing candidate smoke tests are not a release approval.

## Coordination and handoff

Primary agent owns this ledger, integration, PR verification and reporting. Delegate
bounded non-overlapping tasks, check current main/open PRs before work, and keep
implementation commits focused. Each handoff must state completed evidence,
remaining blockers, current milestone and next acceptance outcome. Do not hand
the owner a fresh list of prompts in place of executing authorized work.
