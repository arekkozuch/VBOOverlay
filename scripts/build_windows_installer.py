"""Compile an NSIS installer from a verified Windows candidate staging tree."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess


def nsis_literal(value):
    # Stage paths become NSIS source, not shell text. Reject line injection and
    # escape the NSIS variable/quote syntax even for unusual repository filenames.
    if any(c in str(value) for c in '\r\n\x00'):
        raise ValueError("Invalid character in payload path")
    return str(value).replace('$', '$$').replace('"', '$\\"')


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--stage', type=Path, required=True)
    parser.add_argument('--makensis', required=True)
    args = parser.parse_args()
    repo = Path(__file__).resolve().parents[1]
    stage = args.stage.resolve()
    manifest = json.loads((stage / 'candidate-manifest.json').read_text(encoding='utf-8'))
    if manifest['platform'] != 'Windows' or manifest['buildType'] != 'Release':
        raise ValueError('A Windows Release candidate is required')
    sha = manifest['commit']
    if not re.fullmatch(r'[0-9a-f]{40}', sha):
        raise ValueError('Invalid candidate commit')
    version = re.search(r'MACOSX_BUNDLE_SHORT_VERSION_STRING "(\d+\.\d+\.\d+)"',
                        (repo / 'native/CMakeLists.txt').read_text()).group(1)
    files = sorted(p for p in stage.rglob('*') if p.is_file())
    expected = {entry['path']: entry for entry in manifest['files']}
    if {p.relative_to(stage).as_posix() for p in files} != set(expected) | {'candidate-manifest.json'}:
        raise ValueError('Candidate file inventory differs from manifest')
    for path in files:
        if path.is_symlink():
            raise ValueError('Windows installer does not support symlinks')
        entry = expected.get(path.relative_to(stage).as_posix())
        if entry and hashlib.sha256(path.read_bytes()).hexdigest() != entry['sha256']:
            raise ValueError(f'Candidate hash mismatch: {path.name}')
    generated = repo / 'native-dist/nsis'
    generated.mkdir(parents=True, exist_ok=True)
    payload, remove = [], []
    directories = set()
    for path in files:
        relative = path.relative_to(stage)
        parent = str(relative.parent).replace('/', '\\')
        name = str(relative).replace('/', '\\')
        payload.extend([f'SetOutPath "$INSTDIR\\{nsis_literal(parent)}"',
                        f'File "{nsis_literal(path)}"'])
        remove.extend(['ClearErrors', f'Delete "$INSTDIR\\{nsis_literal(name)}"', 'IfErrors 0 +2', 'StrCpy $RemovalFailed 1'])
        directories.update(relative.parents)
    for directory in sorted(directories, key=lambda p: (-len(p.parts), str(p))):
        if directory != Path('.'):
            name = str(directory).replace('/', '\\')
            remove.append(f'RMDir "$INSTDIR\\{nsis_literal(name)}"')
    for name, lines in [('payload.nsh', payload), ('remove.nsh', remove)]:
        (generated / name).write_text('\n'.join(lines) + '\n', encoding='utf-8-sig')
    output = repo / f'native-dist/artifacts/FlappedEar-Telemetry-Windows-x64-{sha[:12]}-setup.exe'
    output.parent.mkdir(parents=True, exist_ok=True)
    defines = {'OUTPUT_FILE': output, 'PAYLOAD_INCLUDE': generated / 'payload.nsh',
               'REMOVE_INCLUDE': generated / 'remove.nsh', 'APP_VERSION': version, 'COMMIT': sha[:12]}
    subprocess.run([args.makensis, '/V3', *[f'/D{k}={v}' for k, v in defines.items()],
                    str(repo / 'packaging/windows/installer.nsi')], check=True)
    digest = hashlib.sha256(output.read_bytes()).hexdigest()
    output.with_suffix('.exe.sha256').write_text(f'{digest}  {output.name}\n', encoding='utf-8')
    print(f'Unsigned internal installer created: {output.name}')


if __name__ == '__main__':
    main()
