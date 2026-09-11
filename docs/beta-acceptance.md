# Single-session beta acceptance

Updated 11 September 2026. Status: internal candidate preparation; invited beta is
not approved until the checks below have results for the exact delivered archive.
Application version remains 0.2.0. Multi-session work is deferred.

## Scope

One MP4/MOV video and one VBO or uninterrupted version-1 RCZ telemetry session per
project. Candidate targets are macOS ARM64 and Windows x64, Qt 6.8.3. Advertise only
the OS versions and hardware configurations actually entered in the acceptance record.
Cloud runner OS versions are build/test environments, not a declared user-support matrix.

Export supports the documented 8-bit/10-bit SDR policy. HDR/Log, non-zero rotation
and non-square sample aspect ratio are rejected at export preflight. RCZ support and
channel limitations are listed in [rcz-format.md](rcz-format.md). Identified RaceChrono
Pro 10.2.4 VBO gates use verified centre/direction conversion; other identified RaceChrono
versions retain telemetry but omit unverified gates with a warning. Generic VBO gate
endpoints retain their existing interpretation. Multiple videos, resumed/multiple RCZ
sessions, sectors and theoretical best laps are outside this beta scope.

## Candidate builds

Native CI runs Debug and Release tests on both platforms. Only successful Release
jobs deploy Qt with CMake, run the installed application from a separate working
directory with the build Qt SDK hidden, then upload `candidate-<runner>-<commit>`.
The deployment uses [Qt's script API](https://doc.qt.io/qt-6.8/qt-generate-deploy-script.html), QML import scanning and runtime dependency deployment. Executable paths are explicitly quoted because Qt 6.8's convenience generator splits names containing spaces.
The archive includes `candidate-manifest.json` with the checkout commit, architecture,
build type and file hashes; a sidecar SHA-256 identifies the archive. On a PR run the
checkout commit can be GitHub's test merge commit. Record the manifest value, not just
the branch name. Artifacts expire after 14 days; retain an accepted candidate separately.

These are internal test packages, not published releases. macOS uses an ad-hoc signature;
Developer ID signing/notarization and Windows publisher signing remain pending. Qt
deployment can include multimedia codec libraries supplied by the Qt SDK; it does not
install the external `ffmpeg`/`ffprobe` command-line tools. Exact redistribution notices,
license/source obligations, installer UX and signing must be completed before distribution
is approved. See [third-party inventory](../THIRD_PARTY_NOTICES.md).

To reproduce the candidate build with the same platform/compiler and Qt 6.8.3 SDK:

```sh
cmake -S . -B build-native -G Ninja -DCMAKE_BUILD_TYPE=Release -DFLAPPEDEAR_DEPLOY_QT=ON -DCMAKE_PREFIX_PATH=<qt-sdk>
cmake --build build-native --parallel
ctest --test-dir build-native --build-config Release --output-on-failure
cmake --install build-native --prefix native-dist/stage --config Release
```

The build process records dependency versions. FFmpeg is currently resolved through
the platform package manager, so this is a reproducible procedure, not a promise of
byte-identical rebuilds. Run `scripts/package_candidate.py` only in the isolated build
environment: its `--qt-root` directory is temporarily renamed during the startup check.

## Installation and prerequisites for acceptance

1. Download a candidate from a successful workflow and verify its SHA-256 sidecar.
2. Extract the inner archive. macOS contains `stage/FlappedEar Telemetry.app`; Windows
   contains `stage/bin/FlappedEar Telemetry.exe`. Keep the complete extracted tree.
3. Install compatible external FFmpeg and ffprobe. Both must resolve on PATH; macOS
   also searches `/opt/homebrew/bin` and `/usr/local/bin`. Record `ffmpeg -version` and
   `ffprobe -version`. Export checks a working HEVC encoder and the actual composition
   filters, including explicit `setparams` alpha mode. FFmpeg 6.1.1 fails that filter
   check. A successful `ffmpeg -version` alone does not establish compatibility.
4. Launch from Finder/Explorer, then run the tests below on a machine without the
   development Qt SDK. Record any Gatekeeper/SmartScreen prompt exactly; do not disable
   operating-system protection as an installation procedure.

## Acceptance record

Copy this table into the candidate's acceptance issue and attach evidence. Blank or
pending entries do not pass. A scope reduction must be explicit; do not silently skip
an advertised platform or capability.

| Gate | Required evidence | Current status |
|---|---|---|
| Identity | Archive SHA-256, manifest commit, OS build, CPU/GPU, driver, FFmpeg/ffprobe versions | Pending exact candidate |
| Automated tests | Green Debug/Release CI, RCZ suite, startup, deployed startup | Required on candidate |
| Clean installation | Finder/Explorer launch without Qt SDK; media playback; missing/incompatible FFmpeg message | Pending physical machines |
| Real media | Supplied RCZ/VBO plus matching real video through the full workflow below | Telemetry pair passed locally; matching video unavailable in this workspace |
| Hardware export | Actual selected HEVC encoder, output metadata, decoded frames and audio alignment | Pending candidate run |
| Persistence/recovery | Save, reopen, move/relink, mismatch confirmation, unsaved recovery/discard | Synthetic coverage exists; candidate UI check pending |
| Cancellation/output safety | Cancel both stages; existing output bytes preserved on failure/cancel; overwrite requires consent | Synthetic coverage exists; candidate UI check pending |
| Distribution | Exact dependency notices/source access, package/install review, signing decision, retained candidate | Pending |
| Approval | All required evidence linked and beta scope accepted by Arek | Pending |

## Real-media walkthrough

1. Import the video and VBO; repeat using the matching RCZ. Check duration, channel
   availability, map orientation, lap count and lap times. The supplied pair derives
   five complete laps; its parser comparison is documented in `rcz-format.md`.
2. Run auto-sync when the GoPro has usable GPS. Review weak candidates. While it runs,
   edit offset and scale: those edits must remain after completion. For video without
   GPS, check ordinary import and manual synchronization.
3. Compare a visible driving event with telemetry near the beginning, middle and end.
   Record the chosen offset/scale and observed timing error; assess drift explicitly.
4. Edit widgets, open/close Analysis, navigate laps and check all sidebar controls at
   1180×720. Verify missing channels stay missing and ordinary playback remains usable.
5. Save, close and reopen. Move each source and relink it. Try a different same-name
   source and require explicit mismatch confirmation. Exercise recovery and discard.
6. Export a short range, a complete lap with handles, then the full recording. Verify
   first/last frame, CFR count, HUD alignment, 8/10-bit color, sound and delayed/short
   audio where present. Compare preview with decoded output at matching timestamps.
7. Cancel during rendering and composition. Repeat against an existing destination;
   check its SHA-256 before/after cancellation or failure. Successful overwrite must
   require confirmation. A `SuccessWithWarning` result needs review, not an automatic pass.

## Reporting a bug

Include candidate archive hash/commit, OS/GPU, FFmpeg version, exact steps, expected and
actual result, and whether VBO or RCZ was used. Export support logs are retained under
the application-data `exports` directory; use the path shown in diagnostics. Include
the terminal error and relevant log, with personal paths redacted. Share private media
only by explicit agreement. A small synthetic reproducer is preferable when available.

The remaining audit hardening items stay tracked in [ROADMAP.md](../ROADMAP.md).
No data-loss, incorrect timing or unsafe-output defect is acceptable merely because
the build has a beta label.
