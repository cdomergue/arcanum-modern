#!/usr/bin/env python3
"""Compile a Windows installer from a verified mod-only ZIP using NSIS 3."""
import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import zipfile

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('windows_package', ROOT/'tools/package-windows.py')
package = importlib.util.module_from_spec(spec)
spec.loader.exec_module(package)
REQUIRED_FILES = {'arcanum-modern.exe', 'Play Arcanum Modern.bat',
                  'ArcanumModern/INSTALL.txt', 'ArcanumModern/README.md',
                  'ArcanumModern/docs/controls.md', 'ArcanumModern/licenses/arcanum-ce.md',
                  'ArcanumModern/licenses/SDL3.txt', 'ArcanumModern/licenses/SDL3_mixer.txt',
                  'ArcanumModern/licenses/SDL3-hidapi.txt', 'ArcanumModern/licenses/SDL3_mixer-dr_libs.txt',
                  'ArcanumModern/licenses/fpattern.txt', 'ArcanumModern/licenses/zlib.txt',
                  'ArcanumModern/licenses/stb.txt'} | {'modern-ui/'+name for name in package.ASSETS}


def extract_payload(archive, target):
    with zipfile.ZipFile(archive) as z:
        names = z.namelist()
        manifest = 'ArcanumModern/manifest.json'
        metadata = json.loads(z.read(manifest))
        if metadata.get('includes_game_data') is not False:
            raise ValueError('Setup requires a mod-only ZIP without game data')
        if metadata.get('target') != 'windows-x86_64':
            raise ValueError('Setup requires a Windows x64 package')
        if len(names) != len(set(names)):
            raise ValueError('Duplicate package paths')
        missing = REQUIRED_FILES - set(names)
        if missing:
            raise ValueError('Missing required files: '+', '.join(sorted(missing)))
        for name in names:
            if name in REQUIRED_FILES or name == manifest:
                continue
            if not re.fullmatch(r'ArcanumModern/licenses/(?:llvm\.txt|COPYING[\w.-]*)', name):
                raise ValueError('Unexpected package file: '+name)
        hashes = metadata.get('sha256', {})
        if set(hashes) != set(names) - {manifest}:
            raise ValueError('Incomplete package hash manifest')
        for name, digest in hashes.items():
            if hashlib.sha256(z.read(name)).hexdigest() != digest:
                raise ValueError('Package hash mismatch: '+name)
        for name in names:
            path = target/name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(z.read(name))
    return sorted(names)


def build(archive, output, makensis):
    output = output.resolve()
    if output == archive.resolve():
        raise ValueError('Setup output must differ from the input ZIP')
    output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='arcanum-setup-', dir=output.parent) as temp:
        work = Path(temp)
        names = extract_payload(archive, work/'payload')
        imports = package.pe_imports((work/'payload/arcanum-modern.exe').read_bytes())
        if any(name not in package.SYSTEM_DLLS and not name.startswith(('api-ms-win-', 'ext-ms-win-'))
               for name in imports):
            raise ValueError('The executable requires non-system DLLs')
        install = []
        uninstall = []
        directories = set()
        for name in names:
            path = Path(name)
            parent = str(path.parent).replace('/', '\\')
            destination = '$INSTDIR' if parent == '.' else '$INSTDIR\\'+parent
            install += [f'SetOutPath "{destination}"', f'File "payload/{name}"']
            uninstall.append('Delete "$INSTDIR\\'+name.replace('/', '\\')+'"')
            directories.update(str(p).replace('/', '\\') for p in path.parents if str(p) != '.')
        uninstall += [f'RMDir "$INSTDIR\\{directory}"'
                      for directory in sorted(directories, key=lambda d: (-d.count('\\'), d))]
        (work/'install.nsh').write_text('\n'.join(install)+'\n', encoding='utf-8')
        (work/'uninstall.nsh').write_text('\n'.join(uninstall)+'\n', encoding='utf-8')
        shutil.copyfile(ROOT/'tools/windows/setup.nsi', work/'setup.nsi')
        prefix = '/' if os.name == 'nt' else '-'
        subprocess.run([makensis, prefix+'V3', prefix+'WX', 'setup.nsi'], cwd=work, check=True)
        (work/'ArcanumModern-windows-x64-setup.exe').replace(output)
    print(f'Created {output} ({output.stat().st_size/1024/1024:.1f} MiB; no game data)')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--zip', type=Path, default=ROOT/'dist/ArcanumModern-windows-x64.zip')
    parser.add_argument('--output', type=Path, default=ROOT/'dist/ArcanumModern-windows-x64-setup.exe')
    parser.add_argument('--makensis', default=os.environ.get('MAKENSIS', 'makensis'))
    args = parser.parse_args()
    try:
        build(args.zip, args.output, args.makensis)
    except (ValueError, OSError, KeyError, zipfile.BadZipFile, subprocess.CalledProcessError) as error:
        parser.exit(1, f'Setup packaging failed: {error}\n')
