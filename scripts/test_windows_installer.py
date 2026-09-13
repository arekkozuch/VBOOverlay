"""Exercise the actual NSIS candidate on an isolated Windows CI runner."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import winreg

from package_candidate import smoke_installed

KEY = r'Software\Microsoft\Windows\CurrentVersion\Uninstall\FlappedEarTelemetry'


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--stage', type=Path, required=True)
    parser.add_argument('--qt-root', type=Path, required=True)
    args = parser.parse_args()
    repo = Path(__file__).resolve().parents[1]
    manifest = json.loads((args.stage / 'candidate-manifest.json').read_text(encoding='utf-8'))
    sha = manifest['commit'][:12]
    installer = repo / f'native-dist/artifacts/Flapped-Ear-Telemetry-Windows-x64-{sha}-setup.exe'
    root = Path(os.environ['LOCALAPPDATA']) / 'Programs/FlappedEar Telemetry'
    shortcuts = Path(os.environ['APPDATA']) / 'Microsoft/Windows/Start Menu/Programs/Flapped Ear Telemetry'
    if root.exists() or shortcuts.exists():
        raise RuntimeError('Installer smoke requires an isolated runner without an existing installation')
    try:
        winreg.OpenKey(winreg.HKEY_CURRENT_USER, KEY, 0, winreg.KEY_READ | winreg.KEY_WOW64_64KEY).Close()
    except FileNotFoundError:
        pass
    else:
        raise RuntimeError('Existing installation registration; refusing to modify it')
    events = []
    for cycle in range(2):
        subprocess.run([str(installer), '/S'], check=True, timeout=120)
        for entry in manifest['files']:
            path = root / entry['path']
            if hashlib.sha256(path.read_bytes()).hexdigest() != entry['sha256']:
                raise RuntimeError(f'Installed payload differs: {entry["path"]}')
        with winreg.OpenKey(winreg.HKEY_CURRENT_USER, KEY, 0, winreg.KEY_READ | winreg.KEY_WOW64_64KEY) as key:
            assert winreg.QueryValueEx(key, 'InstallLocation')[0] == str(root)
            assert winreg.QueryValueEx(key, 'BuildCommit')[0] == sha
        assert (shortcuts / 'Flapped Ear Telemetry.lnk').is_file()
        smoke_installed(root / 'bin/Flapped Ear Telemetry.exe', args.qt_root.resolve(),
                        repo / f'ci-logs/installer-startup-{cycle}.txt')
        # Rerunning must fail without changing the installed build.
        rejected = subprocess.run([str(installer), '/S'], timeout=60)
        assert rejected.returncode == 2, rejected.returncode
        sentinel = root / 'user-added-file.txt'
        sentinel.write_text('preserve this user file', encoding='utf-8')
        # NSIS _?= runs synchronously without the self-copy/relaunch mechanism.
        # Execute a test-owned copy so removal can delete the installed uninstaller.
        with tempfile.TemporaryDirectory() as temporary:
            uninstaller = Path(temporary) / 'Uninstall.exe'
            shutil.copyfile(root / 'Uninstall.exe', uninstaller)
            command = subprocess.list2cmdline([str(uninstaller), '/S']) + ' _?=' + str(root)
            subprocess.run(command, check=True, timeout=120)
        assert sentinel.read_text(encoding='utf-8') == 'preserve this user file'
        assert not (root / 'bin/Flapped Ear Telemetry.exe').exists()
        assert not (root / 'Uninstall.exe').exists()
        assert not shortcuts.exists()
        assert [p.relative_to(root).as_posix() for p in root.rglob('*') if p.is_file()] == ['user-added-file.txt']
        try:
            winreg.OpenKey(winreg.HKEY_CURRENT_USER, KEY, 0, winreg.KEY_READ | winreg.KEY_WOW64_64KEY).Close()
        except FileNotFoundError:
            pass
        else:
            raise AssertionError('Uninstall registration survived removal')
        events.append(f'Cycle {cycle + 1}: install hashes, registration, shortcut, SDK-hidden startup, overwrite rejection, uninstall and user-file preservation passed')
    sentinel.unlink()
    root.rmdir()
    (repo / 'ci-logs/installer-lifecycle.txt').write_text('\n'.join(events) + '\n', encoding='utf-8')
    print('\n'.join(events))


if __name__ == '__main__':
    main()
