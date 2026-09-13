# Flapped Ear Telemetry — audit and delivery ledger

Audit date: 12 September 2026; reconciled for [KAN-12](https://kozucharkadiusz.atlassian.net/browse/KAN-12).
Product scope: [product contract](product-vision.md).
This is the current delivery authority; old checkpoints and narrow beta documents
must not override it. Feature implementation is not the same as runtime acceptance.

## Verified baseline

This reconciliation uses `main` at
[`7138fbd511e3d06ed9b237130d385e1f62bde027`](https://github.com/arekkozuch/VBOOverlay/commit/7138fbd511e3d06ed9b237130d385e1f62bde027).
It is a dated source/evidence snapshot, not a moving reference. Later task PRs,
merge SHAs and CI results belong in their Jira completion records.

| Integrated change | Source state | Verification |
| --- | --- | --- |
| PRs #7–#10: event/source foundations and calculated-G corrections | Original audit `d7e195e`; retained in this baseline | Included in the baseline's full Native CI suite |
| [PR #11](https://github.com/arekkozuch/VBOOverlay/pull/11): whole-outing import, chronological sections and independent detail | Merged at `a0122ab9851d3db8240708b2554d56ab4f5eb5b8`; native-QPA fix `8aeb572` retained rendering/input assertions | [Integrated-main CI](https://github.com/arekkozuch/VBOOverlay/actions/runs/34712830140) passed; also covered by the newer baseline run |
| [PR #12](https://github.com/arekkozuch/VBOOverlay/pull/12), [KAN-5](https://kozucharkadiusz.atlassian.net/browse/KAN-5): GPS reference eligibility, production UUID log retention and delivery contract | Final PR head `6f8479248034e236c533dd855bb517ab08a49e75`; merged at `7138fbd511e3d06ed9b237130d385e1f62bde027` | [PR CI](https://github.com/arekkozuch/VBOOverlay/actions/runs/34715062783) and [main CI](https://github.com/arekkozuch/VBOOverlay/actions/runs/34715471586) passed all four jobs; KAN-5 is complete |

The PR #12 fixture correction preserves gate passages while injecting interior
GPS defects. It retains measured lap timing, reference exclusions and rendering
assertions; see [the regression record](testing.md#kan-5-preserve-gate-evidence-in-interior-defect-fixtures).

## Evidence levels

- **Implementation** describes code present at the baseline. Partial, raw-channel
  and visualization foundations do not satisfy an entire F00–F20 capability.
- **CI verification** means the configured synthetic coverage passed on that
  source state. Baseline Native CI built macOS arm64 and Windows x64 in Debug
  and Release with Qt 6.8.3. Each job passed all seven CTest registrations;
  Release jobs also passed Qt deployment/package startup, and Windows Release
  passed the NSIS lifecycle checks. This is not coverage of missing features.
- **Physical acceptance** requires an identified candidate, environment and
  executed scenario. Earlier private-media/hardware results remain historical
  development evidence. No new physical-Mac or full-day/private-video acceptance
  is claimed for `7138fbd`. M5 and M6 acceptance tasks below remain required.

This coordinator environment has no CMake/Qt. Native execution evidence comes
from CI; the owner's local Codex follows the [build/test handoff](development-workflow.md).
Private telemetry files must not be committed.

## Actual capability audit

Every row refers to the verified baseline above. Existing foundations have the
configured CI coverage; missing analytical outcomes have neither implementation
nor execution acceptance. Jira links identify concrete implementation/acceptance
work, not evidence that a future capability already works. Ranges are inclusive.

| ID | Implementation at `7138fbd` | Remaining outcome and Jira ownership |
| --- | --- | --- |
| F00 | Partial: EventProjectCodec v3, transactional multi-file import, dated pairing and whole-outing workflow | Day identity/metadata/persistence: [KAN-19], [KAN-24], [KAN-26]–[KAN-28]; parser bounds: [KAN-14]–[KAN-16]; folder/drop, reusable profiles and existing-run alternatives: [KAN-87]–[KAN-90] |
| F01 | Partial: LapTiming, live comparison tiles, single-section inspection; GPS eligibility verified in [KAN-5] | Compatible ranking and exclusions: [KAN-19]–[KAN-23]; independent A/B, shared progress and delta: [KAN-29]–[KAN-42] |
| F02 | Missing: source Start gate is only a foundation | Proposed/reviewed/editable sectors and corners: [KAN-43]–[KAN-50]; acceptance: [KAN-58] |
| F03 | Missing | Non-overlapping ranked losses and evidence navigation: [KAN-59]–[KAN-61]; acceptance: [KAN-74] |
| F04 | Missing | Sector theoretical and donor provenance: [KAN-50], [KAN-51], [KAN-56]–[KAN-58]; separately validated realistic potential: [KAN-94]–[KAN-96] |
| F05 | Partial: single trace and independent detail cursor | Shared progress, paired traces/cursor: [KAN-31], [KAN-33], [KAN-37]–[KAN-42]; available-channel map layers: [KAN-97] |
| F06 | Missing | Entry/apex/exit, braking and throttle metrics and Corner Analyzer: [KAN-46], [KAN-47], [KAN-51]–[KAN-55]; acceptance: [KAN-58] |
| F07 | Missing | Measured/inferred coasting duration, distance and locations: [KAN-92] |
| F08 | Missing | Overlapping driving states with prerequisites and provenance: [KAN-91] |
| F09 | Missing | Braking/cornering overlap, with measured/inferred distinction: [KAN-93] |
| F10 | Missing | Eligible populations, timing/braking/exit/line variability and presentation: [KAN-62]–[KAN-64]; acceptance: [KAN-74] |
| F11 | Partial foundation: chronological run/section organization | Within-day progression and conditions: [KAN-21], [KAN-24], [KAN-25], [KAN-28], [KAN-64]; reusable profiles and comparable visits: [KAN-89], [KAN-98], [KAN-99] |
| F12 | Raw channels only: recorded temperatures can be plotted | Covered extrema, thermal trends and recorded recovery: [KAN-67], [KAN-68]; acceptance: [KAN-74] |
| F13 | Raw channels only: recorded temperature/performance inputs | Sample-backed associations, without causal claims: [KAN-100] |
| F14 | Partial visualization: G ball/radar/bar and calculated-G aliases | Timed G-G pairs, scatter and observed peaks: [KAN-65], [KAN-66]; acceptance: [KAN-74] |
| F15 | Partial: central single-video sync and independent video-free detail | Analysis/video navigation: [KAN-39], [KAN-42]; chapter review/timeline/export and side-by-side video: [KAN-104]–[KAN-107]; private-video acceptance: [KAN-80] |
| F16 | Partial foundation: GPS/OBD/HR coexist; alternative files persist | Channel provenance in A/B: [KAN-35]; existing-run alternatives: [KAN-90]; actual cross-file clock alignment/fusion/conflict review: [KAN-101]–[KAN-103] |
| F17 | Raw channels only: imported/plotted HR | Run/lap/sector summaries and comparisons: [KAN-69], [KAN-70]; acceptance: [KAN-74] |
| F18 | Missing | Computed report, best results and click-through observations: [KAN-23], [KAN-71]–[KAN-74] |
| F19 | Missing | Computed observation guidance: [KAN-73]; evidence package and Explain this lap: [KAN-108], [KAN-109] |
| F20 | Substantial implementation: WidgetModel, shared TelemetryScene/FrameRenderer, transactions, recovery and verified UUID log retention | Descendant shutdown implementation: [KAN-13]; time-bound and naming implementation: [KAN-17], [KAN-18]; export/installed-candidate acceptance: [KAN-75]–[KAN-85]; chapter export: [KAN-106] |

Core physical acceptance closes in [KAN-86]; full F00–F20 acceptance closes in
[KAN-110]. Neither is complete at this baseline. Reuse PR #11's independent
verified-source loader, bounded row service and detail view for subsequent work.

## Correctness blockers and ownership

| ID | Finding | Required closure / current action |
| --- | --- | --- |
| C01 | PR #11 Qt 6.8 Mac QML crash | Closed by native-QPA fix `8aeb572`, integrated at `a0122ab`; PR and main Native CI passed with rendering/input coverage retained |
| C02 | Flat best-lap trace bridges missing GPS | Closed in [KAN-5] / PR #12 at `7138fbd`: timings retained, invalid references excluded before trace/ranking, reason/no-delta exposed; focused regressions and PR/main CI passed |
| C03 | Log retention ignores canonical UUIDs | Closed in [KAN-5] / PR #12 at `7138fbd`: actual IDs matched; unrelated/active files and symlinks protected; retention regressions and PR/main CI passed |
| C04 | Unix descendants outlive group leader | Implemented in [KAN-13] / PR #14: retain group ownership after leader exit, bound escalation, gate controller commit/cleanup and stale recovery on group termination; exact CI/integration evidence is recorded in KAN-13 |
| C05 | VBO bulk split/derived time budgets | [KAN-14] / PR #15 implements bounded line/field scanning (integration evidence in Jira); [KAN-15] implements derived-time/conversion checks (integration evidence in Jira); [KAN-17] adds checked bidirectional transforms and bounded auto-sync search (integration evidence in Jira) |
| C06 | Coordinate interpretation ambiguity | [KAN-16] implements explicit exporter/header evidence shared by samples and gates; unresolved units withhold GPS with a warning. Quadrant, zero-crossing and conflict regressions; exact integration evidence in Jira |
| C07 | Slow/full export destination behavior | Open: [KAN-75] exercises cancellation, scan and transaction cleanup under slow/filling volume |
| C08 | User-visible name/package drift | [KAN-18] standardizes Flapped Ear Telemetry display/About/bundle/package names with settings/recovery preservation checks; macOS integration evidence in Jira; Windows execution paused |

Event ranking needs additional compatibility and exclusion rules beyond C02.
Do not present a GPS-continuous interval as proof of a comparable racing lap.

## Dependency-ordered delivery

The backlog contains **100 separate numbered Tasks plus seven milestone Epics**.
At this reconciliation's start, [KAN-5] was complete, [KAN-12] was selected and
steps 003–100 remained open. Jira holds live status and task acceptance criteria.

| Milestone epic | Steps | Task keys | Count |
| --- | --- | --- | --- |
| [M0 — reliable baseline][KAN-4] | 001–008 | [KAN-5], [KAN-12]–[KAN-18] | 8 |
| [M1 — day results][KAN-6] | 009–018 | [KAN-19]–[KAN-28] | 10 |
| [M2 — A/B comparison][KAN-7] | 019–032 | [KAN-29]–[KAN-42] | 14 |
| [M3 — sectors and corners][KAN-8] | 033–048 | [KAN-43]–[KAN-58] | 16 |
| [M4 — losses and report][KAN-9] | 049–064 | [KAN-59]–[KAN-74] | 16 |
| [M5 — core acceptance][KAN-10] | 065–076 | [KAN-75]–[KAN-86] | 12 |
| [M6 — full vision][KAN-11] | 077–100 | [KAN-87]–[KAN-110] | 24 |

The effort ranges below are **historical estimates from the original audit**, not
remaining-work estimates or delivery dates. M0's Jira scope includes additional
hardening, so its original range must not be treated as a forecast for all eight
tasks. Ranges include implementation, regressions and integration; they are not
elapsed-time guarantees and are not divided by agent count.

| Milestone | User-visible completion | Acceptance / dependencies | Original effort estimate |
| --- | --- | --- | --- |
| M0 — regain a reliable baseline | Existing outing workflow integrated; master vision/status truthful | KAN-5 and KAN-12–KAN-18: C01–C03 verified, descendant/parser/coordinate fixes, sync-bound verification and naming; C07 retained in M5 | 1–3 working days for the original narrower audit slice |
| M1 — day results | Best eligible run/day, run progression, notes/conditions and exclusions | Reuse outing service; compatible layout/direction/gate groups; missing files and GPS incomplete states; persistence/recovery | 2–4 days |
| M2 — comparison evidence | Independent A/B, distance delta, speed/available channels, two traces, common cursor | M1; deterministic shared track-progress alignment including crossings/gaps; no editor mutation | 6–10 days |
| M3 — corner analysis | Automatic sector/corner proposals with review/editing, entry/apex/exit/braking metrics, sector theoretical | M2; stable editable boundaries, metric prerequisites/provenance and downstream straight effects | 7–12 days |
| M4 — useful conclusions | Ranked losses, consistency, G-G, available thermal/HR summary and clickable report | M3; non-overlapping losses, sample counts, missing-data semantics, measured/inferred distinction | 5–9 days |
| M5 — complete core acceptance | A tested Mac app covering the core product journey and overlay export | M4 + C04–C08; full-day/private-video walkthrough, reopen/recovery, installed candidate, short/lap/full exports | 4–7 days plus external access |
| M6 — remaining original vision | Realistic potential, expanded state/coasting/trail analysis, thermal correlation, multi-event history, fusion, chapter/comparison video and explanations; remaining F00 folder/drop, reusable vehicle/track references and alternatives on existing runs; remaining F05 channel map layers | Individually validated algorithms, source/clock provenance; explicit acceptance per F00–F20; earlier core remains usable | Additional 25–45 days, low confidence |

The original core workflow estimate was **25–45 focused working days** from
the `d7e195e` audit; the full-vision estimate was **50–90 working days total**.
Confidence is low until actual cycle times and acceptance availability are known.
These are retained planning history, not a claim that autonomous work continues between turns.
A defensible calendar date needs a demonstrated execution rate and Mac acceptance
availability. Reforecast in [KAN-28] after M1 and [KAN-42] after M2 using actual cycle time; report scope
changes explicitly rather than silently dropping F00–F20 items to meet a date.

The owner's near-term benefit arrives incrementally: integrated PR #11 gives day/lap inspection;
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

The [task delivery workflow](development-workflow.md) requires one active Jira
task, an implementation PR, passing PR and integrated-main CI, and an exact-SHA
handoff for local Codex compilation. All Jira content is English. KAN-5 starts
the numbered backlog; milestone containers do not count towards its 100 tasks.

Primary agent owns this ledger, integration, PR verification and reporting. Delegate
bounded non-overlapping tasks, check current main/open PRs before work, and keep
implementation commits focused. Each handoff must state completed evidence,
remaining blockers, current milestone and next acceptance outcome. Do not hand
the owner a fresh list of prompts in place of executing authorized work.

[KAN-4]: https://kozucharkadiusz.atlassian.net/browse/KAN-4
[KAN-5]: https://kozucharkadiusz.atlassian.net/browse/KAN-5
[KAN-6]: https://kozucharkadiusz.atlassian.net/browse/KAN-6
[KAN-7]: https://kozucharkadiusz.atlassian.net/browse/KAN-7
[KAN-8]: https://kozucharkadiusz.atlassian.net/browse/KAN-8
[KAN-9]: https://kozucharkadiusz.atlassian.net/browse/KAN-9
[KAN-10]: https://kozucharkadiusz.atlassian.net/browse/KAN-10
[KAN-11]: https://kozucharkadiusz.atlassian.net/browse/KAN-11
[KAN-12]: https://kozucharkadiusz.atlassian.net/browse/KAN-12
[KAN-13]: https://kozucharkadiusz.atlassian.net/browse/KAN-13
[KAN-14]: https://kozucharkadiusz.atlassian.net/browse/KAN-14
[KAN-15]: https://kozucharkadiusz.atlassian.net/browse/KAN-15
[KAN-16]: https://kozucharkadiusz.atlassian.net/browse/KAN-16
[KAN-17]: https://kozucharkadiusz.atlassian.net/browse/KAN-17
[KAN-18]: https://kozucharkadiusz.atlassian.net/browse/KAN-18
[KAN-19]: https://kozucharkadiusz.atlassian.net/browse/KAN-19
[KAN-21]: https://kozucharkadiusz.atlassian.net/browse/KAN-21
[KAN-23]: https://kozucharkadiusz.atlassian.net/browse/KAN-23
[KAN-24]: https://kozucharkadiusz.atlassian.net/browse/KAN-24
[KAN-25]: https://kozucharkadiusz.atlassian.net/browse/KAN-25
[KAN-26]: https://kozucharkadiusz.atlassian.net/browse/KAN-26
[KAN-28]: https://kozucharkadiusz.atlassian.net/browse/KAN-28
[KAN-29]: https://kozucharkadiusz.atlassian.net/browse/KAN-29
[KAN-31]: https://kozucharkadiusz.atlassian.net/browse/KAN-31
[KAN-33]: https://kozucharkadiusz.atlassian.net/browse/KAN-33
[KAN-35]: https://kozucharkadiusz.atlassian.net/browse/KAN-35
[KAN-37]: https://kozucharkadiusz.atlassian.net/browse/KAN-37
[KAN-39]: https://kozucharkadiusz.atlassian.net/browse/KAN-39
[KAN-42]: https://kozucharkadiusz.atlassian.net/browse/KAN-42
[KAN-43]: https://kozucharkadiusz.atlassian.net/browse/KAN-43
[KAN-46]: https://kozucharkadiusz.atlassian.net/browse/KAN-46
[KAN-47]: https://kozucharkadiusz.atlassian.net/browse/KAN-47
[KAN-50]: https://kozucharkadiusz.atlassian.net/browse/KAN-50
[KAN-51]: https://kozucharkadiusz.atlassian.net/browse/KAN-51
[KAN-55]: https://kozucharkadiusz.atlassian.net/browse/KAN-55
[KAN-56]: https://kozucharkadiusz.atlassian.net/browse/KAN-56
[KAN-58]: https://kozucharkadiusz.atlassian.net/browse/KAN-58
[KAN-59]: https://kozucharkadiusz.atlassian.net/browse/KAN-59
[KAN-61]: https://kozucharkadiusz.atlassian.net/browse/KAN-61
[KAN-62]: https://kozucharkadiusz.atlassian.net/browse/KAN-62
[KAN-64]: https://kozucharkadiusz.atlassian.net/browse/KAN-64
[KAN-65]: https://kozucharkadiusz.atlassian.net/browse/KAN-65
[KAN-66]: https://kozucharkadiusz.atlassian.net/browse/KAN-66
[KAN-67]: https://kozucharkadiusz.atlassian.net/browse/KAN-67
[KAN-68]: https://kozucharkadiusz.atlassian.net/browse/KAN-68
[KAN-69]: https://kozucharkadiusz.atlassian.net/browse/KAN-69
[KAN-70]: https://kozucharkadiusz.atlassian.net/browse/KAN-70
[KAN-71]: https://kozucharkadiusz.atlassian.net/browse/KAN-71
[KAN-73]: https://kozucharkadiusz.atlassian.net/browse/KAN-73
[KAN-74]: https://kozucharkadiusz.atlassian.net/browse/KAN-74
[KAN-75]: https://kozucharkadiusz.atlassian.net/browse/KAN-75
[KAN-80]: https://kozucharkadiusz.atlassian.net/browse/KAN-80
[KAN-85]: https://kozucharkadiusz.atlassian.net/browse/KAN-85
[KAN-86]: https://kozucharkadiusz.atlassian.net/browse/KAN-86
[KAN-87]: https://kozucharkadiusz.atlassian.net/browse/KAN-87
[KAN-89]: https://kozucharkadiusz.atlassian.net/browse/KAN-89
[KAN-90]: https://kozucharkadiusz.atlassian.net/browse/KAN-90
[KAN-91]: https://kozucharkadiusz.atlassian.net/browse/KAN-91
[KAN-92]: https://kozucharkadiusz.atlassian.net/browse/KAN-92
[KAN-93]: https://kozucharkadiusz.atlassian.net/browse/KAN-93
[KAN-94]: https://kozucharkadiusz.atlassian.net/browse/KAN-94
[KAN-96]: https://kozucharkadiusz.atlassian.net/browse/KAN-96
[KAN-97]: https://kozucharkadiusz.atlassian.net/browse/KAN-97
[KAN-98]: https://kozucharkadiusz.atlassian.net/browse/KAN-98
[KAN-99]: https://kozucharkadiusz.atlassian.net/browse/KAN-99
[KAN-100]: https://kozucharkadiusz.atlassian.net/browse/KAN-100
[KAN-101]: https://kozucharkadiusz.atlassian.net/browse/KAN-101
[KAN-103]: https://kozucharkadiusz.atlassian.net/browse/KAN-103
[KAN-104]: https://kozucharkadiusz.atlassian.net/browse/KAN-104
[KAN-106]: https://kozucharkadiusz.atlassian.net/browse/KAN-106
[KAN-107]: https://kozucharkadiusz.atlassian.net/browse/KAN-107
[KAN-108]: https://kozucharkadiusz.atlassian.net/browse/KAN-108
[KAN-109]: https://kozucharkadiusz.atlassian.net/browse/KAN-109
[KAN-110]: https://kozucharkadiusz.atlassian.net/browse/KAN-110
