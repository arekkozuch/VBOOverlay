# KAN-42 — M2 acceptance and delivery reforecast

Recorded 25 September 2026. Task 032 accepts the M2 A/B-comparison increment
(KAN-29 through KAN-41, all merged) and reforecasts remaining scope using
measured M2 cycle time, as required by the task's own acceptance criteria. It
does not implement M3 (sectors/corners) or later backlog.

## Scope of this acceptance task

KAN-42's acceptance criteria ask for crossings, gaps, different-lines,
missing-sensors and known-delta fixture coverage, then a two-run-comparison-
with-optional-video exercise. Four of those five fixture categories already
have dedicated regressions from the tickets that implemented the shared
progress axis and delta computation (KAN-31 through KAN-34); this task does
not duplicate them, and instead adds the one category with a real, disclosed
gap plus the video/comparison independence exercise the M2 milestone had not
yet tested end-to-end.

| Acceptance criterion | Status at KAN-42 start | This task's contribution |
| --- | --- | --- |
| Crossings (hairpin/parallel-section ambiguity) | Covered: `TrackProgressTests::ambiguousEquidistantCrossingIsRejected`, `::headingRejectsOppositeDirectionParallelSection` (KAN-32) | Referenced, not duplicated |
| Gaps (GPS outlier + real time gap) | Covered: `TrackProgressTests::outlierAndGapBreakSegmentsWithoutBridging`, `::deltaSeriesOnlyCoversSharedValidRange` (KAN-32/34) | Referenced, not duplicated |
| Different lines (route/layout/direction differences) | Covered: `TelemetryTests::infersRoutesFromOrderedCompleteLaps`, `OutingLaps::lapCompatibilityReasons` (`changed-layout`, `opposite-direction`, `different-recorded-route`) | Referenced, not duplicated |
| Known delta (deterministic pace difference, correct sign/magnitude) | Covered at the `TrackProgress` unit level only: `TrackProgressTests::knownDelayHasCorrectSignAndFinishLineMagnitude` | **Elevated to the full AppController import → comparison-slot → shared-axis pipeline**: `TelemetryTests::comparesKnownDeltaThroughFullComparisonPipeline` |
| Missing sensors (one recording lacks a channel the other has) | **Gap**: no fixture exercised this; `comparisonAvailableChannels()`'s intersection logic was only exercised with identical channel sets on both sides | New: `TelemetryTests::excludesChannelMissingFromOneComparisonSlot` |
| Two-run comparison with optional video | **Gap**: `ComparisonDetailPanel.qml` / `AppControllerComparison.cpp` have no video code at all; KAN-39 wired video only into the single-lap `OutingLapDetailPanel` | New: `TelemetryTests::keepsComparisonAndOutingLapVideoIndependent` |

The video finding is a scope clarification worth stating plainly: this app
has exactly one central video slot (`m_videoSource`/`m_sync`/
`m_exportSourceInfo`), gated to the currently open single lap's run (KAN-39).
There is no dual, side-by-side comparison video — that capability is
separately scoped to KAN-104 through KAN-107 in the delivery ledger's F15
row and is not implemented by this task. "Exercise two-run comparison with
optional video" is satisfied here as an **editor-independence** regression:
proving that populating/clearing the A/B comparison pair and
opening/advancing video on the single-lap side cannot corrupt or be
corrupted by each other's state, since the two production surfaces do not
read one another today.

## New regressions (this task)

All three live in `native/tests/TelemetryTests.cpp`, alongside the existing
KAN-40/41 comparison tests:

- `excludesChannelMissingFromOneComparisonSlot` — two imported runs, one with
  a synthetic `speed` column appended to its VBO text (`withSyntheticSpeedChannel`),
  the other without. Asserts `comparisonAvailableChannels()` excludes the
  channel the second recording lacks, the slot that has it still serves real
  progress-series segments, and the slot that lacks it reports
  `{"reason": "channelMissing"}` rather than empty-but-silent or borrowed data.
- `comparesKnownDeltaThroughFullComparisonPipeline` — two **separate**
  imported runs of the identical physical path (same coordinates; only time
  uniformly rescaled by a known 10% via `routeVboWithTimeScale`), taken
  through real import, `selectComparisonLap` and the shared progress axis
  end to end. Asserts the delta series never reads the faster lap as behind,
  is clearly ahead somewhere along the lap, and its finish-line value
  approximates the true known duration difference within 1.0s — the same
  tolerance style `TrackProgressTests::knownDelayHasCorrectSignAndFinishLineMagnitude`
  already established at the unit level.
- `keepsComparisonAndOutingLapVideoIndependent` — populates the A/B
  comparison pair, then opens the single-lap outing detail on the active run
  and attaches a synthetic video via the same friend-class member injection
  KAN-39's tests use (no real decodable file needed). Confirms neither
  direction disturbs the other: the comparison slots' sessions and delta
  series are unchanged by opening video, attaching it, or advancing the
  video-driven cursor; and the outing lap's video availability and cursor
  are unchanged by clearing, reselecting or the comparison pair being ready.

## Native build/test evidence

This coordinator workspace has `cmake`/`ctest` but no Qt 6 installation, so
these changes could not be compiled or run locally. Per the existing task
delivery workflow, native verification runs on this PR's macOS arm64
Debug/Release Cloud CI (Qt 6.8.3) and on the resulting `main` revision after
merge.

PR #42 final head `59d9f847d9a6b7ec89d9d2efdec923c136f4e75c`: macOS arm64
Debug and Release, Qt 6.8.3, both passed — [Debug run](https://github.com/arekkozuch/VBOOverlay/actions/runs/36100000454/job/107960267831),
[Release run](https://github.com/arekkozuch/VBOOverlay/actions/runs/36100000454/job/107960268001).
All 9 CTest registrations passed on both configurations; `flappedear_native_tests`
(which includes the three new comparison regressions) reported 385 passed,
0 failed, 7 skipped.

The PR's first push (head `1dabd9f`) actually failed CI and surfaced a real
bug in this task's own new test: `comparesKnownDeltaThroughFullComparisonPipeline`
passed `turns=2` to `routeVbo()`, but `inferTrack()` requires at least 2
complete laps to resolve a route, and `turns=2` on this closed-circuit
fixture yields only 1 complete lap (OUT + one lap + IN) — so the route never
resolved and both imported runs stayed ineligible for comparison
(`fixture must resolve one lap from each run` assertion failure). Every
other two-run comparison test in this file relies on `routeVbo()`'s default
`turns=4`; switching to that default fixed it. Recorded here rather than
silently amended away, since a test author's own fixture bug is exactly the
kind of gap this acceptance record exists to catch.

Integrated-`main` CI evidence will be recorded in this record's Jira
counterpart ([KAN-42](https://kozucharkadiusz.atlassian.net/browse/KAN-42))
once the owner merges and that revision's CI is verified.

## Measured M2 cycle time and reforecast

Source: Jira status histories for KAN-29 through KAN-41 (the 13 M2
implementation tickets; KAN-42 itself is this acceptance/reforecast step and
is excluded from implementation-duration statistics, matching the KAN-28
precedent for step 018). Duration is first recorded In Progress ("W toku") to
Done ("Gotowe"), the same workflow-status-interval measure KAN-28 used for
M0/M1 — this is elapsed workflow time including CI/review/administration and
any pause between activation and actual work, not measured engineering
effort.

| Step / Jira | Recorded cycle, minutes |
| --- | ---: |
| 019 / KAN-29 | 29.53 |
| 020 / KAN-30 | 37.76 |
| 021 / KAN-31 | 74.11 |
| 022 / KAN-32 | 75.88 |
| 023 / KAN-33 | 75.88 |
| 024 / KAN-34 | 75.86 |
| 025 / KAN-35 | 725.97 |
| 026 / KAN-36 | 725.92 |
| 027 / KAN-37 | 725.85 |
| 028 / KAN-38 | 725.79 |
| 029 / KAN-39 | 43.87 |
| 030 / KAN-40 | 35.04 |
| 031 / KAN-41 | 38.56 |

Thirteen measured cycles, median **75.86 minutes**, range 29.53–725.97,
sum 3,390.02 minutes. **KAN-35 through KAN-38's ~726-minute cycles are a
disclosed same-day pause**, not four tickets' worth of continuous work: each
was moved to "W toku" around 09:54 on 24 September and completed around
22:00 the same day, a roughly twelve-hour gap between activation and
completion for all four essentially simultaneously. This is the opposite
pattern from M0/M1, where every measured cycle stayed under 45 minutes.

The more decision-relevant number for this reforecast is calendar elapsed
time, not the per-ticket status interval: M2's first recorded start (KAN-29,
13 September 21:12:36.772 UTC+2) to its last completion (KAN-39, 25
September 07:18:09.610 UTC+2) spans **approximately 11.4 elapsed days** for
13 completed tickets — roughly **1.1 completed items per calendar day**.
That is close to the *original, human-estimate* M2 range of 6–10 working
days in the delivery ledger's effort table, and an order of magnitude slower
than the 4–8-items/day capacity scenarios KAN-28 modeled from M0/M1's much
faster, mostly synthetic-only cycles. M2 is the first milestone in this
project where the harder alignment/geometry work (KAN-31 through KAN-34) and
real gaps in continuous execution actually show up in the numbers, which is
exactly the risk KAN-28 flagged as unresolved ("two-week full-scope
confidence is low until M2 demonstrates the harder analysis work").

**Reforecast**: after step 032 (this task), 68 numbered tasks remain
(033–100: M3's 16, M4's 16, M5's 12, M6's 24), plus the three additional UX
tickets (KAN-113–115) outside the 100-task count. At M2's observed ~1.1
items/calendar-day, 68 remaining numbered tasks alone would take on the
order of **60 elapsed calendar days** — far beyond the 27 September planning
target set on 13 September, which is now essentially arrived (2 days out at
the time this is recorded). This is an honest arithmetic consequence of the
measured data, not a scope or effort judgment about M3–M6's actual
difficulty, and it does not by itself imply the owner should extend the
deadline, narrow scope, or accept the current baseline as final — that
decision needs the owner, informed by this evidence, per the disclosed-gaps
principle this ledger already applies elsewhere. The owner has already
treated the current `main` as an acceptable Sunday checkpoint and made a
personal binary copy of it; this reforecast does not change that decision,
it only supplies the M2 evidence the 13 September plan explicitly deferred
to this step.

Full F00–F20 scope, the M3 corner-analysis milestone, and any deadline or
scope decision beyond this acceptance record remain for the owner to decide
with this evidence in hand — this document does not itself authorize
dropping features, extending the deadline, or starting M3.

## Known disclosed gaps

- No interactive/real-GoPro/real-VBO verification was performed for the new
  tests; all three use the existing synthetic `EventProjectFixture::routeVbo()`
  generator and friend-class member injection, the same pattern KAN-39/40/41
  established.
- Dual, side-by-side comparison video remains unimplemented by design (see
  "Scope of this acceptance task" above) — tracked separately at KAN-104
  through KAN-107.
- The M2 cycle-time reforecast is workflow-status elapsed time, not a
  measurement of pure engineering effort; the ~726-minute outliers are a
  same-day pause, disclosed above, not four tickets of continuous work.
- Integrated-`main` CI evidence for the merged revision is not yet recorded;
  see "Native build/test evidence" above.
