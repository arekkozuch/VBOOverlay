# Application identity and upgrades

The visible product name is **Flapped Ear Telemetry**. It appears in the main and
analysis window titles, Help > About, project/template dialogs, macOS bundle,
candidate manifest and archive names. Internal identifiers retain existing data.

| Purpose | Value |
| --- | --- |
| User-facing name and executable | `Flapped Ear Telemetry` |
| macOS bundle | `Flapped Ear Telemetry.app` |
| macOS bundle identifier | `com.flappedear.telemetry` (unchanged) |
| Qt storage application name | `FlappedEar Telemetry` (unchanged) |
| Qt organization / domain | `FlappedEar` / `flappedear.com` (unchanged) |
| Project / template extensions | `.fetproject` / `.fettemplate` (unchanged) |
| Candidate archive prefix | `Flapped-Ear-Telemetry-` |
| Windows installation directory | `%LOCALAPPDATA%\Programs\FlappedEar Telemetry` (unchanged) |
| Windows uninstall key (HKCU) | `Software\Microsoft\Windows\CurrentVersion\Uninstall\FlappedEarTelemetry` (unchanged) |

`ApplicationIdentity::initialize()` sets the visible Qt display name separately
from the application name used by QSettings and QStandardPaths. The original
preferences, recovery snapshots and GUI session lock therefore keep their paths.
No project rewrite, preference migration or second data directory is required.
The internal CMake target, C++ namespace, QML URI and icon resource remain compatible.

## Replacing a macOS candidate

1. Quit the running app. Keep your projects and source media in their existing locations.
2. Extract the new candidate and place `Flapped Ear Telemetry.app` in the directory
   where you keep the application. Remove the old `FlappedEar Telemetry.app` bundle
   after replacing it; remove only the application bundle, not application data.
3. Launch the new bundle. Existing preferences and the last project remain available;
   unsaved recovery still requires the same explicit Recover or Discard choice.
4. Update any Dock item or script that names the old bundle/executable path.

The application version remains 0.2.0. These are internal candidates; signing,
notarization and clean-machine acceptance remain separate gates. There is no
automatic updater. See [candidate acceptance](beta-acceptance.md).

## Project opening and Windows status

The baseline registers no Finder document association and has no Finder file-open
handler. The Windows installer also deliberately registers no file association.
This rename preserves that behavior: use **File > Open Project** for `.fetproject`
files. It does not introduce a double-click association or change project schemas.

Windows source display names, executable and shortcuts follow the new product
name. The install directory and uninstall key remain stable, so the existing
uninstall-before-reinstall policy still detects an older candidate. Windows
builds, tests, packaging and installer execution are paused by owner direction
since 13 September 2026. Runtime upgrade validation there remains deferred; see
[the retained installer procedure](windows-installer.md).
