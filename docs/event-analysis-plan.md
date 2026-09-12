# Flapped Ear Telemetry — event analysis delivery

Updated 12 September 2026. The complete scope is now maintained in
[product-vision.md](product-vision.md); actual status, blockers, milestones,
forecasts and acceptance live in [product-delivery.md](product-delivery.md).
This page is an entry point, not a competing roadmap.

## Foundation already implemented

- Bounded VBO/RCZ preparation, provenance, duplicates and cancellation.
- Version 3 Event/Run/source persistence, recovery, rebasing and explicit relinking.
- Multi-file transactional review, create/append and optional per-run video/sync.
- Whole-outing import and chronological section list with independent map/channel
  inspection: PR #11 merged at `a0122ab`, with passing PR/main CI.
- GPS-reference eligibility and quality/no-delta presentation: PR #12 merged at
  `7138fbd`, with passing PR/main CI; [KAN-5](https://kozucharkadiusz.atlassian.net/browse/KAN-5)
  is complete. Exact source and run links are in the delivery ledger.

The manual review workflow leaves source grouping explicit. Whole-outing import
can group a unique dated VBO/RCZ GPS match and use VBO for calculated G. These are
different entry points, documented in [batch-import.md](batch-import.md).
Neither combines raw channels across files. HR stays within the imported session.

## Next product outcomes

1. Quality-aware run/event results and progression, using the existing outing service.
2. Independent cross-run A/B, shared track progress, delta and paired map/channels.
3. Reviewed corners/sectors, metrics and sector theoretical.
4. Ranked losses, consistency, G-G, available thermal/HR data and automatic report.
5. Full Mac journey/overlay-export acceptance, then remaining advanced F00–F20 work.

A timed interval, usable spatial reference and comparable event lap are distinct.
GPS continuity alone cannot establish layout/direction/conditions compatibility.
Do not extend the old live nearest-segment overlay delta into cross-run analysis
without the alignment and eligibility gates in the product contract.

The [previous plan](history/2026-09-12-event-analysis-plan.md) is a historical
implementation record. Its old baseline, not-wired statements and beta scope are
not current instructions.
