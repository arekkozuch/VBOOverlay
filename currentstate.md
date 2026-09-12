# Flapped Ear Telemetry — current state

Updated 12 September 2026. Version remains 0.2.0. **Development; full track-day product incomplete.**

The authoritative scope is [product vision](docs/product-vision.md); the current
code audit, blockers, milestones, estimates and acceptance record are in
[product delivery](docs/product-delivery.md). These documents supersede the
historical single-session beta scope as the definition of the requested product.

## Integrated baseline

Reconciled for [KAN-12](https://kozucharkadiusz.atlassian.net/browse/KAN-12) against
`main` at [`7138fbd511e3d06ed9b237130d385e1f62bde027`](https://github.com/arekkozuch/VBOOverlay/commit/7138fbd511e3d06ed9b237130d385e1f62bde027).
This dated snapshot includes the native overlay editor/export, VBO/RCZ parsers,
source-defined lap timing, v3 Event → Run → Lap/source ownership, transactional
multi-file review, active-run selection, no-video lap listing and corrected
RaceChrono calculated-G selection/presentation.

[PR #11](https://github.com/arekkozuch/VBOOverlay/pull/11), merged at `a0122ab`,
provides whole-outing import, dated GPS-evidence pairing, chronological OUT/LAP/IN
sections and independent selected-section map/charts/cursor. Its native-QPA fix
retained rendering/input assertions and passed the PR and main CI gates.

[PR #12](https://github.com/arekkozuch/VBOOverlay/pull/12), merged at `7138fbd`,
excludes incomplete GPS laps from reference ranking while retaining measured
timings, exposes quality/best-of-run information, and corrects export-log
retention for production UUIDs. [KAN-5](https://kozucharkadiusz.atlassian.net/browse/KAN-5)
is complete: both [PR CI](https://github.com/arekkozuch/VBOOverlay/actions/runs/34715062783)
and [integrated-main CI](https://github.com/arekkozuch/VBOOverlay/actions/runs/34715471586)
passed macOS arm64 and Windows x64, Debug/Release, Qt 6.8.3. Each job passed all
seven CTest registrations; Release packaging and Windows NSIS checks also passed.

The [delivery ledger](docs/product-delivery.md#actual-capability-audit) records
all 21 F00–F20 states and Jira references. The queue has 100 individual Tasks
plus seven milestone Epics. Live status and subsequent task PR/merge/CI evidence
belong in Jira; this snapshot does not predeclare later tasks complete.

## Still needed for the core product

Compatible event ranking and progression, metadata/exclusions, independent A/B
track-progress comparison, dual traces/delta, reviewed corners/sectors, Corner
Analyzer, sector theoretical, consistency, G-G, ranked losses and the automatic
evidence-linked report. Raw OBD/HR charts do not yet constitute vehicle/driver
analysis. The full original vision remains itemized as F00–F20.

The editor/export foundation should be preserved. Remaining hardening includes
Unix descendants after leader exit, VBO allocation/time budgets, coordinate-unit
ambiguity, slow/full destinations and consistent user-facing package naming.

## Evidence boundaries

Historical development results are preserved in
[the pre-audit checkpoint](docs/history/2026-09-12-before-product-audit.md) and
[the September 1 checkpoint](docs/history/2026-09-01-currentstate.md).
PR #11 records local Qt 6.11.1 tests and private telemetry-only UI checks. Those
earlier results do not certify a newly built candidate. This coordinator workspace
lacks native CMake/Qt and cannot claim local native execution; the baseline build
and synthetic test evidence above comes from CI.

CI covers macOS/Windows Debug/Release synthetic tests and internal candidates.
It does not certify the exact installed Mac candidate on the owner's full-day
telemetry and matching GoPro media. [Candidate acceptance](docs/beta-acceptance.md)
remains required, tracked through [M5](https://kozucharkadiusz.atlassian.net/browse/KAN-10)
and [M6](https://kozucharkadiusz.atlassian.net/browse/KAN-11). No release is approved
by these implementation changes. The owner's local Codex can use the
[update/build/test handoff](docs/development-workflow.md#local-codex-update-compile-and-test-current-main).
