# AGENTS.md

## Engineering rules

- Treat the Electron application as a prototype and behavior reference. New production architecture
  uses Qt 6, C++, and QML as recorded in `ROADMAP.md`.
- Preserve `.fetproject` compatibility and behavioral test fixtures during the native migration.
- Use C++20, Qt Test, CMake targets, compiler warnings, and explicit ownership in native code.
- Model multiple GoPro chunks as one ordered, time-based media timeline; never reset telemetry time
  at a clip boundary.
- Do not rewrite working architecture without a concrete reason.
- Keep telemetry parsing independent from React.
- Keep video and media code independent from widget UI.
- Keep synchronization in one central telemetry module.
- Heart Rate comes from VBO; do not add a separate HR source.
- Never invent brake telemetry or substitute another channel silently.
- Never couple telemetry to FPS. All synchronization is time based.
- Preview and export should share scene definitions and rendering logic.
- Always test parser changes, including malformed input.
- Always test synchronization changes with deterministic and ambiguous signals.
- Use strict TypeScript and avoid `any`.
- Keep Electron IPC typed and narrow. Never expose generic filesystem, shell, or process APIs.
- Do not commit API keys, secrets, user paths, generated video, or build artifacts.
- Finish every completed implementation or fix with a focused local commit and a descriptive commit
  message. Do not push or synchronize it unless the user explicitly asks.
- Add no unrelated dependencies. Verify current versions, compatibility, maintenance, and licenses first.
- Do not implement roadmap features unless requested.
- Do not claim functionality works without running the relevant command or integration fixture.
- Preserve ordinary video import when GoPro metadata is absent or malformed.
- Treat VFR as a timing concern and warn until a validated VFR export exists.

## Required validation

Before handing off a change, run as applicable:

```bash
npm run format:check
npm run lint
npm run typecheck
npm test
npm run build
```

Real VBO and GoPro integration results must be reported separately from synthetic tests.
