# KAN-26 local validation — 13 September 2026

Task 016 and the owner's automatic-grouping correction were implemented together
on local `main`, starting from clean commit `65342b1`. No cloud or Windows jobs,
publication, PR, merge or release actions were performed. Private recordings and
review artifacts are not repository fixtures.

## Behavior and regression coverage

Complete repeated GPS routes establish supported layout/direction groups and
automatically populate best-day results, rankings and progression. Multiple
groups retain independent results. Recorded timing definitions must still match
exactly. Manual correction can apply to matching recordings together; the dialog
can open an existing GPS trace for inspection. Ambiguity is explained per run,
and OUT/IN sections remain ordinary inspectable, untimed sections.

Explicit group choices, manual configuration and complete-reference exclusions
use project Save, Save As, relocation, reopen and unsaved recovery. Unavailable
choices remain saved without selecting a substitute. Full-content source checks,
per-run cache dependencies and detail request guards preserve independent run
analysis while rejecting changed or stale inputs. Unchanged automatic
recomputation does not repeatedly advance the document revision.

Regression coverage includes noise, sampling/phase differences, reverse travel,
alternative routes with identical timing gates, pit detours, sparse/conflicting
geometry, ambiguous matches between groups, source replacement, retained group
IDs, metadata edits, exclusions, relocation/relink, recovery and stale async
completion. Production QML keyboard/mouse flows exercise import, automatic Best
day, group selection, correction, progression and lap inspection.

The initial decision regression reproduced a comparison selection that left the
document clean. Review also reproduced an automatic ID collision after source
replacement and a selector count that included route-ineligible intervals. The
final regression gate checks the corrected boundaries.

Matching thresholds, provenance fields and dependency rules are documented in
[the project format](event-project-format.md#gps-route-evidence-and-tolerances-gps-route-v1).

## Executed local gate

Existing native CMake configuration: Debug, macOS arm64, Qt 6.11.0, Apple LLVM
21.0.0. No toolchain or dependency upgrade was introduced.

```bash
cmake --build build-native --parallel
ctest --test-dir build-native --output-on-failure
git diff --check
open "build-native/native/Flapped Ear Telemetry.app"
```

The Debug build passed. Full CTest passed **7/7 suites**, zero failures, in
56.24 seconds. Qt Test totals within those suites were:

| Suite | Passed | Skipped |
| --- | ---: | ---: |
| Native controller, QML, telemetry, render/export | 370 | 6 |
| RCZ | 29 | 1 |
| Import | 21 | 0 |
| Event project and recovery | 52 | 0 |
| Lap eligibility | 25 | 0 |
| Persistent export log | 4 | 0 |

Production startup smoke also passed. QRhi/software-FFmpeg coverage remained
enabled. The synthetic Main10 VideoToolbox full-range color regression passed;
no hardware-skip variable was set. Diff whitespace checks passed. The rebuilt
application was launched locally after validation.

## Private recordings and visible QML

The optional `automaticallyGroupsPrivateTrackDay` integration imported all six
VBO files from ignored `jastrzab/`. It produced one automatic clockwise group,
37 visible sections, 25 complete intervals and 23 eligible laps, with rankings
and progression for all six runs and no grouping notices. Two recorded-route
outliers remained visible and inspectable but were excluded from timed results.
No layout, direction or comparison-group clicks were needed to obtain results.

The native integration passed in 12.221 seconds. It saved a review project and
captured the production Analysis window at 1180×720, Progression, correction
dialog and lap GPS traces in a local temporary directory. Captures were visually
inspected: the selector and progression both report 23/25 eligible laps, the
normal OUT section has no configuration warning, correction controls fit, and
the trace inspection action opens the source lap map. Private data/captures are
not committed. See [optional private test commands](testing.md#private-real-fixtures).

The separate real-VBO parser and lap-derivation tests also passed: 15,578 samples,
49 channels, zero parser warnings/non-finite values, five gate passages and four
complete laps in the first recording. Their combined run took 1.557 seconds.

## Limits of this validation

The default CTest skips optional private-day/VBO/GoPro tests when their environment
variables are unset, and the RCZ suite skips its unavailable private pair. The
day and VBO parser/lap cases were then exercised separately as recorded above.
No private GoPro synchronization/export, RCZ/VBO pair, or optional real-source
lookup benchmark was run. GUI evidence is automated native interaction and
capture inspection, not an owner-operated end-to-end session. Release packaging,
Windows and Cloud CI remain untested and paused for this task.
