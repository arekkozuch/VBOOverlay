# KAN-28 — M1 acceptance and delivery reforecast

Recorded 13 September 2026. Task 018 accepts the implemented day-results
increment and updates the forecast. It does not implement the subsequent A/B,
corner, report or additional chart UX backlog.

## Executed source and acceptance matrix

Code baseline: `5d71a1a5e5d31579102e93b6912838a122baa763` on `main`,
tree `d257c924b870864374bc88f2671fe481b3b91d6e`.
PR #32 final head `8a2cb2564d27f68a0f0938c2ee51962961ba1f0f` and tested merge
`420c7ffa6eb912e608cce9828fc7446757f15b71` have this same tree.

- [PR execution, run 34775248771](https://github.com/arekkozuch/VBOOverlay/actions/runs/34775248771).
- [Integrated-main execution, run 34775598217](https://github.com/arekkozuch/VBOOverlay/actions/runs/34775598217).
- [Test assertions at the accepted source](https://github.com/arekkozuch/VBOOverlay/blob/5d71a1a5e5d31579102e93b6912838a122baa763/native/tests/TelemetryTests.cpp).

The existing full suite was executed for the selector change and its integration
during this acceptance task. The following checks were inspected in source and
matched to actual PASS results; this is not a list of proposed tests.

| Required scenario | Executed regression | Assertions supporting acceptance |
| --- | --- | --- |
| Multiple runs, automatic results and incompatible configurations | `automaticallyGroupsRunsThroughProductionQml`, `groupsOutingLapsAfterExplicitConfiguration` | Matching routes produce automatic best-day/progression results without fabricated manual overrides; reversed and alternative routes produce separate groups; actual QML opens the winning lap |
| OUT/IN sections | `derivesOutingLapSections`, `opensRankedLapsAndRecomputesAfterExclusion` | Three complete laps coexist with OUT/IN; excluding all complete laps removes the winner while all five sections remain visible |
| Exclusions and ranking recomputation | `opensRankedLapsAndRecomputesAfterExclusion` | Excluding the winner selects a different eligible lap; all-excluded groups have no winner; restoring eligibility recomputes the result; stale refreshes cannot retain winner badges |
| Notes, conditions and progression | `editsRunMetadataWithoutChangingAnalysis`, `showsRunProgressionWithLiveContext` | Metadata editing and live progression retain source/analysis context and apply current eligibility |
| Save, Save As, move, reopen, missing/relinked sources and recovery | `restoresDayDecisionsAfterMoveMissingRelinkAndRecovery` | IDs, exclusions, manual configurations, notes and group choices persist; unavailable choices remain saved without choosing a substitute; relink restores the exact choice and unsaved recovery remains distinct |
| Dependency invalidation and independent lap detail | `persistsDayDecisionsAndKeepsIndependentDetail`, `automaticallyGroupsRunsThroughProductionQml` | A changed run invalidates its stale reference while preserving the unrelated run's cached derivation, selected detail, track and cursor |
| Video-free ready/empty/loading/missing/error states and bounded retry | `presentsDayResultStatesWithoutVideo` | Production QML remains usable with one missing run; restore/retry recovers; changed content at the same path is an identity error; all-missing and empty states differ; stale workers cannot publish obsolete notices; editor sync and document state remain unchanged |

macOS arm64, Qt 6.8.3, Debug and Release: **7/7 CTest registrations passed**
per configuration. Qt Test totals per configuration: native 370 passed/7 skipped,
RCZ 29/1, import 21/0, event project 52/0, lap eligibility 25/0 and export log
4/0: **501 passed, zero failed, eight skipped**. Startup is the seventh CTest
registration. Release additionally verifies deployment/package startup with the
Qt SDK isolated. No native toolchain is available in the coordinator container;
these are hosted macOS executions, not local container test claims.

The eight skips are the private-day test, two real-VBO cases, real-video decode,
real-source lookup benchmark, private synchronization, private RCZ/VBO pair and
the explicitly excluded VideoToolbox hardware-encoder case. Software FFmpeg,
QRhi rendering and production QML interactions remain enabled. Windows remains
paused under current owner direction.

## Separate local/private evidence and remaining acceptance

The owner supplied local commit
`ddb095a514c06cd7ad090257b9a74108e85d810d` with the
[KAN-26 validation record](kan26-local-validation.md). macOS arm64 Debug,
Qt 6.11.0 and Apple LLVM 21 passed 7/7 CTest suites in 56.24 seconds, including
the synthetic hardware-encoder case. Separate private integrations imported six
VBO recordings: one automatic clockwise group, 37 sections, 25 complete laps,
23 eligible laps and two inspectable route outliers. Real-VBO parser and lap
derivation checks also passed. Automated production QML captures were inspected.

That evidence is retained from task 016; private recordings were not rerun here
and were not committed. It is not an owner-operated walkthrough of the accepted
baseline. Private GoPro synchronization/export, the private RCZ pair and the
full installed-candidate track-day journey remain open for M5/M6 acceptance.
The wider selector's code and QML interaction are covered; owner visual acceptance
on the actual Mac display is still separate. KAN-113 (more than four channels),
KAN-114 (interval zoom with map highlighting) and KAN-115 (numeric axes/units)
remain recorded backlog work, as requested.

## Measured cycle time and reforecast

Source: Jira status histories retrieved on 13 September for KAN-5 and KAN-12
through KAN-27. For these short histories, the returned history counts matched
their reported totals. Duration is first recorded In Progress to Done, including
CI/review/administration. This is workflow elapsed time, not engineering effort.

| Step / Jira | Recorded cycle, minutes |
| --- | ---: |
| 001 / KAN-5 | 23.24 |
| 002 / KAN-12 | 23.51 |
| 003 / KAN-13 | 29.93 |
| 004 / KAN-14 | 26.04 |
| 005 / KAN-15 | 23.89 |
| 006 / KAN-16 | 28.10 |
| 007 / KAN-17 | 28.31 |
| 008 / KAN-18 | 24.83 |
| 009 / KAN-19 | 21.36 |
| 010 / KAN-20 | 27.01 |
| 011 / KAN-21 | 25.15 |
| 012 / KAN-22 | 19.61 |
| 013 / KAN-23 | 20.68 |
| 014 / KAN-24 | 43.73 |
| 015 / KAN-25 | 20.82 |
| 016 / KAN-26 | Unknown: direct To Do → Done; no recorded start |
| 017 / KAN-27 | 33.20 |

M0: eight measured tasks, median **25.43 minutes**, range 23.24–29.93,
sum 207.86 minutes. Its first recorded start was 12 September 19:38:21.894 UTC;
last completion was 13 September 08:22:52.261 UTC: **12.74 elapsed hours**,
including the overnight gap.

M1 implementation (009–017): nine completed tasks, eight measurable cycles,
median **23.26 minutes**, range 19.61–43.73, sum 211.57 minutes. The first start
was 13 September 09:43:12.947 UTC and step 017 closed at 18:27:30.404 UTC:
**8.74 elapsed hours**. Step 018 is this acceptance/reforecast and is excluded
from implementation-duration statistics. KAN-26's unknown duration is not zero.
Some work preceded Jira activation (notably PR #12), so these status intervals
understate total development time. Side fixes and pauses are not numbered-task
effort measurements.

The owner now requires the planned changes in two weeks before returning to the
track. **Planning deadline: 27 September 2026**, derived from the 13 September
instruction. The exact track-event date was not separately supplied.

The original F00–F20 scope remains intact: after 018, **82 numbered tasks**
remain, plus the **three additional UX items**. Reserve the final two days,
26–27 September, for candidate regression, the owner's Mac/private GoPro journey
and track preparation. That leaves twelve delivery days; 85 items require
**7.08 completed items/day**. Task counts are an unweighted capacity check:
algorithm tasks and physical acceptance do not have equal cost.

| Sustained completed items/day (scenario) | Delivery days, rounded up | With two acceptance days |
| ---: | ---: | ---: |
| 4 | 22 | 24 days |
| 6 | 15 | 17 days |
| 8 | 11 | 13 days |

These scenarios replace the old human-working-day ranges for near-term capacity
discussion; they are not measured sustained throughput or statistical forecast
intervals. The sample establishes fast delivery of M0/M1 increments, but does
not validate the complexity of shared-progress alignment, corner inference,
source fusion or physical acceptance. **Two-week full-scope confidence is low**
until M2 supplies that evidence. Do not convert a short median into a promise
of continuous unattended work, drop scope silently or count private skips as
accepted functionality.

Execute step 019 / KAN-29 next, following the existing dependency order. Reforecast
at step 032 / KAN-42 using M2 cycle durations, remaining weighted work and private
acceptance availability. Escalate a forecast miss with explicit remaining scope
when evidence shows it; continue delivering usable increments meanwhile.

M1's synthetic acceptance criteria are satisfied by the recorded matrix. This
closes the day-results increment; full-product and owner physical acceptance
remain separately tracked in [the delivery ledger](product-delivery.md).
