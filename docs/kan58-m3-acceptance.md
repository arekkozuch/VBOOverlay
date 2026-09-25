# KAN-58 — M3 acceptance: segmentation, Corner Analyzer and theoretical best

Recorded 25 September 2026. Task 048 accepts the M3 increment, KAN-43 through
KAN-57, which is merged or in review when this record is written. The final
merge SHAs are in the Jira comments. This task does not implement M4 (time
losses, consistency, day report).

All evidence here is **synthetic**: generated VBO route fixtures run through
the real import, lap derivation, review, persistence and comparison code. Real
track feedback is recorded separately below and is still outstanding.

## Acceptance criteria and evidence

| Criterion | Evidence |
| --- | --- |
| Proposal | `TrackSegmentProposalTests` (straight/corner proposals, boundary uncertainty; KAN-45), `CornerPhaseTests` (geometric entry/apex/exit; KAN-46), `BrakingOnsetTests` (KAN-47), `TelemetryTests::reviewsSegmentProposalsForTheOpenLap` (review UI; KAN-48) |
| Correction | `TrackSegmentEditingTests` and `TelemetryTests::editsApprovedSegmentsWithUndo` (edit, split, merge, undo/redo, stable IDs; KAN-49). The workflow test below also splits a segment after a result was shown. |
| Save and reopen | `TelemetryTests::persistsSegmentationAcrossSaveRecoveryAndReopen` (IDs, bounds, rejections, recovery; KAN-50). The workflow test reopens a saved project and gets the same theoretical best (same revision, same sector count, same total). |
| Corner comparison | `TelemetryTests::showsCornerAnalyzerSegmentMetricsForBothLaps`, `::selectsCornerAnalyzerSegmentThroughQml` (KAN-55) |
| Donor-sector navigation | `TelemetryTests::opensTheoreticalBestDonorFromAnotherRun`, `::opensTheoreticalBestSectorThroughQml` (KAN-57), and the workflow test after reopening |
| Changed layouts | `TelemetryTests::persistsSegmentationAcrossSaveRecoveryAndReopen` (stored segments kept but no longer applied). The workflow test changes the canonical run's layout, and the theoretical best becomes unavailable instead of reusing the old segments. |
| Missing sensors | The route fixtures have no speed, throttle or brake channel. `CornerPhaseTests`, `BrakingMetricsTests` and `ExitMetricsTests` cover missing channels at unit level. `showsCornerAnalyzerSegmentMetricsForBothLaps` and the workflow test show corner speeds as `speedChannelMissing` through the full comparison path, never derived from GPS. |
| Known-time sums | `SectorTimingTests` (complete partition sums to lap time within 1 ms; KAN-51), `TelemetryTests::timesApprovedSectorsForTheOpenLap`, `TheoreticalBestTests` (KAN-56), and the workflow test's known-time fixture below |
| Cache invalidation | New in this task: the workflow test shows a ready theoretical best, splits an approved segment, and the result returns to `idle` (not recomputed from stale segments). The recalculated result has a new revision and one more sector. |

## New regression (this task)

`TelemetryTests::acceptsM3SegmentationCornerAndTheoreticalBestWorkflow` runs
the M3 workflow end to end on a known-time fixture, `warpedRouteVbo`. Two
recordings of the same route each lap in exactly 48 s. One drives the first
half of each revolution at 0.9× the nominal time and the second half at 1.1×.
The other does the reverse. No single lap is faster than 48 s, but the best
fragments are.

1. Import both runs and approve every proposal on one run only.
2. Calculate the theoretical best. The actual best covers the whole lap, its
   lap time is 48.000 s and its sector sum matches it within 10 ms. The
   theoretical total is 45.080 s on this fixture. The test requires it to be
   at least 0.5 s below 48 s and above the 43.2 s all-quick bound. Donor laps
   come from **both** runs, and the difference equals actual minus theoretical.
3. Open the review and split an approved segment. The shown result is
   invalidated. The recalculated result has a new revision and 6 sectors
   instead of 5, and its total is not higher (a finer partition can only
   lower a sum of minima). It stays 45.080 s here because the split does not
   cross the pace change.
4. Save, then reopen in a new controller. The result has the same revision,
   the same sector count and the same total.
5. Open a timed corner sector from the theoretical best. The Corner Analyzer
   pair loads the canonical segments and timed A/B sector values, and the
   corner speeds are `speedChannelMissing`.
6. Change the canonical run's layout. The result is invalidated, and a new
   calculation is `unavailable`.

## Known limitations carried into M4

- **Canonical run.** Segments are approved per run, and nothing propagates
  them to other runs. The theoretical best and the Corner Analyzer, when
  opened from it, use the approved segments of the eligible run with the
  lowest run ID. This is deterministic, but arbitrary if several runs have
  their own approved segments.
- **Axis.** Segment bounds are distances along the axis they were approved
  on. The theoretical best times every lap on one axis built from the
  canonical run. The regular Corner Analyzer uses lap A's axis. Boundaries
  can shift by a few metres (the KAN-51 axis limitation).
- **Measured channels.** No fixture in this acceptance has speed, throttle
  or brake channels. "Measured" corner, braking and exit values are covered
  only by the unit fixtures with real channel data.
- **Interactive use.** The QML harness activates rows and buttons with the
  keyboard, but no one has used the full workflow interactively on a screen.

## Real-track feedback (recorded separately)

Outstanding. The owner's next track day is Sunday 27 September 2026. Record
here: the recordings used (not committed; see "Private real fixtures" in
`docs/testing.md`), whether automatic proposals matched the track's corners,
how much correction was needed, whether the theoretical best and donor laps
matched the driver's own view of the day, and any sensor gaps found. Until
then, M3 has synthetic acceptance only.
