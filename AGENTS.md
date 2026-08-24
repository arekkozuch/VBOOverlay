# AGENTS.md

## Engineering rules

- Treat Qt 6, C++, and QML as the only production application architecture.
- Preserve the native `.fetproject` schema and behavioral test fixtures.
- Use C++20, Qt Test, CMake targets, compiler warnings, and explicit ownership in native code.
- Model multiple GoPro chunks as one ordered, time-based media timeline; never reset telemetry time
  at a clip boundary.
- Do not rewrite working architecture without a concrete reason.
- Keep telemetry parsing independent from QML and widget rendering.
- Keep video and media code independent from widget UI.
- Keep synchronization in one central telemetry module.
- Heart Rate comes from VBO; do not add a separate HR source.
- Never invent brake telemetry or substitute another channel silently.
- Never couple telemetry to FPS. All synchronization is time based.
- Preview and export should share scene definitions and rendering logic.
- Always test parser changes, including malformed input.
- Always test synchronization changes with deterministic and ambiguous signals.
- Do not commit API keys, secrets, user paths, generated video, or build artifacts.
- Finish every completed implementation or fix with a focused local commit and a descriptive commit
  message. Do not push or synchronize it unless the user explicitly asks.
- Add no unrelated dependencies. Verify current versions, compatibility, maintenance, and licenses first.
- Do not implement roadmap features unless requested.
- Do not claim functionality works without running the relevant command or integration fixture.
- Preserve ordinary video import when GoPro metadata is absent or malformed.
- Treat VFR as a timing concern: warn about input cadence and preserve the validated CFR conversion policy.
- Documentation is part of every iteration.

## Safety and correctness invariants

- Never write FFmpeg directly to a user-selected export target. Only transaction-owned temporary paths
  may be deleted automatically, and an existing target requires explicit overwrite consent.
- Project saves remain atomic. New, open, and quit actions must respect dirty state, and project open
  document validation/commit must remain transactional even though external sources resolve afterward.
- A project document and its external assets are separate. Missing or moved media must not prevent a
  valid project document from opening. Relative source references are preferred where portable, and a
  source must not be silently accepted solely because a pathname matches.
- The saved `.fetproject` is the authoritative clean document state. Recovery data is separate,
  represents unsaved changes, and must never be silently marked clean. Discard removes unsaved
  recovery state rather than persisting it as the next clean session.
- Source and project async results must be guarded by generation and source identity. Stale results
  must never mutate committed state.
- Long-running source parsing, media probing, and synchronization operations must be cooperatively
  cancellable. Source-generation checks prevent stale commits; cancellation prevents wasted work.
  Both are required.
- Untrusted/imported telemetry and media metadata must be resource-bounded before large allocation
  or recursion.
- Parser output timestamps must remain strictly monotonic. Public telemetry boundaries must not expose
  `NaN` or infinity; outside-range and missing telemetry are no data, and missing gaps are not bridged.
- Static telemetry geometry must not be rebuilt or repainted on playback-time updates. Dynamic
  playback markers must update independently from static geometry.
- Preserve the staged, frame-correct telemetry-overlay export architecture unless evidence establishes a
  safer replacement. Do not restore the unsafe live-overlay FFmpeg approach.
- Qt Quick offscreen frames use an explicit premultiplied-alpha contract from QRhi readback through
  Stage A and Stage B composition. Do not rely on implicit alpha interpretation.
- Raw-frame transport into encoder processes uses bounded byte-oriented backpressure. Never assume
  `QProcess` can buffer complete raw frames. Partial, rejected, or timed-out writes must never be
  treated as successful frame submission.
- Final exports are CFR at one authoritative rational export rate. Preserve that exact rate across
  overlay staging, source conversion, progress, and validation; do not reconstruct it from doubles.
- Cloud CI is intentionally disabled. Local build/tests are the current required gate, and real-media
  validation must be reported separately from synthetic tests.
- QRhi/GuiPrivate use is version-sensitive. A Qt upgrade requires explicit local render/export smoke
  validation.
- Source raster, bit depth, and color characteristics are data, not presets. Never impose a product-level
  4K ceiling; runtime renderer, encoder, and resource capability determine native-export support.
- Ten-bit is not synonymous with HDR. Unsupported HDR/Log material must never be silently converted to
  8-bit SDR or declared preserved by metadata copying alone.

## Required validation

Before handing off a change, run as applicable:

```bash
cmake --build build-native --parallel
ctest --test-dir build-native --output-on-failure
```

Real VBO and GoPro integration results must be reported separately from synthetic tests.
