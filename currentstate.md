# Flapped Ear Telemetry — current state

Updated 12 September 2026. Version remains 0.2.0. **Development; full track-day product incomplete.**

The authoritative scope is [product vision](docs/product-vision.md); the current
code audit, blockers, milestones, estimates and acceptance record are in
[product delivery](docs/product-delivery.md). These documents supersede the
historical single-session beta scope as the definition of the requested product.

## Implemented and incoming

Main `d7e195e` includes the native overlay editor/export, VBO/RCZ parsers,
source-defined lap timing, v3 Event → Run → Lap/source ownership, transactional
multi-file review, active-run selection, no-video lap listing and corrected
RaceChrono calculated-G selection/presentation.

PR #11 adds whole-outing import, dated GPS-evidence pairing, chronological
OUT/LAP/IN sections and independent selected-section map/charts/cursor. The
coordinator is resolving its Qt6.8 CI native-window platform mismatch; consult
its exact-head checks before treating it as integrated.

Current coordinated work also excludes incomplete GPS laps from reference
ranking while keeping measured timings visible, shows quality/best-of-run
information, and corrects export-log retention for production UUIDs. These
changes require their own CI evidence; source implementation is not a test pass.

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
PR #11 records local Qt6.11.1 tests and private telemetry-only UI checks. Its
Qt6.8 cloud results are a separate gate. This coordinator workspace lacks native
CMake/Qt and cannot claim local native execution.

CI covers macOS/Windows Debug/Release synthetic tests and internal candidates.
It does not certify the exact installed Mac candidate on the owner's full-day
telemetry and matching GoPro media. [Candidate acceptance](docs/beta-acceptance.md)
remains required; no release is approved by these implementation changes.
