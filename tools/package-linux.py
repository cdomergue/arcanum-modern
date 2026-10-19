#!/usr/bin/env python3
"""Package a Linux x64 engine with static SDL dependencies and no game data."""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import re
import struct
import subprocess
import tempfile
import zipfile

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('windows_package', ROOT/'tools/package-windows.py')
resources = importlib.util.module_from_spec(spec)
spec.loader.exec_module(resources)
SYSTEM_LIBRARIES = {'libc.so.6', 'libm.so.6', 'libdl.so.2', 'libpthread.so.0',
                    'librt.so.1', 'libgcc_s.so.1'}


def validate_elf(data):
    if (len(data) < 64 or data[:7] != b'\x7fELF\x02\x01\x01'
            or struct.unpack_from('<H', data, 16)[0] not in (2, 3)
            or struct.unpack_from('<H', data, 18)[0] != 62):
        raise ValueError('Expected a Linux x86-64 ELF executable')


def validate_dependencies(output):
    imports = set(re.findall(r'\(NEEDED\).*\[([^\]]+)\]', output))
    if imports - SYSTEM_LIBRARIES:
        raise ValueError('Non-system libraries required; rebuild statically: '+', '.join(sorted(imports - SYSTEM_LIBRARIES)))
    if re.search(r'\((?:RPATH|RUNPATH)\)', output):
        raise ValueError('Executable contains a library search path')
    return sorted(imports)


def package(exe, build_dir, output):
    binary = exe.read_bytes()
    validate_elf(binary)
    imports = validate_dependencies(subprocess.check_output(['readelf', '-d', str(exe)], text=True))
    versions = subprocess.check_output(['readelf', '--version-info', str(exe)], text=True)
    glibc = re.findall(r'GLIBC_(\d+(?:\.\d+)+)', versions)
    if not glibc:
        raise ValueError('Expected a glibc-linked Linux executable')
    minimum_glibc = max(glibc, key=lambda v: tuple(map(int, v.split('.'))))
    files = {'arcanum-modern': binary,
             'Play Arcanum Modern.sh': (ROOT/'tools/linux/Play Arcanum Modern.sh').read_bytes(),
             'ArcanumModern/INSTALL.txt': (ROOT/'tools/linux/INSTALL.txt').read_bytes(),
             'ArcanumModern/README.md': (ROOT/'README.md').read_text().replace('(game/modern-ui/', '(../modern-ui/').encode(),
             'ArcanumModern/docs/controls.md': (ROOT/'docs/controls.md').read_bytes(),
             'ArcanumModern/licenses/arcanum-ce.md': (ROOT/'source/LICENSE.md').read_bytes()}
    for name in resources.ASSETS:
        files['modern-ui/'+name] = (ROOT/'game/modern-ui'/name).read_bytes()
    cache = (build_dir/'CMakeCache.txt').read_text()
    for name, license_file in [('SDL3', 'LICENSE.txt'), ('SDL3_mixer', 'LICENSE.txt'),
                               ('fpattern', 'LICENSE'), ('zlib', 'LICENSE')]:
        match = re.search(r'^'+name+r'_SOURCE_DIR:[^=]+=(.+)$', cache, re.M)
        if not match:
            raise ValueError('Missing dependency source for license: '+name)
        directory = Path(match[1].strip())
        files['ArcanumModern/licenses/'+name+'.txt'] = (directory/license_file).read_bytes()
        for extra in {'SDL3': ['src/hidapi/LICENSE-bsd.txt'], 'SDL3_mixer': ['src/dr_libs/LICENSE']}.get(name, []):
            files['ArcanumModern/licenses/'+name+'-'+Path(extra).parent.name+'.txt'] = (directory/extra).read_bytes()
    stb = (ROOT/'source/third_party/stb/stb_truetype.h').read_text()
    files['ArcanumModern/licenses/stb.txt'] = stb[stb.rindex('/*\n---'):].encode()
    git = ['git', '-c', f'safe.directory={ROOT}', '-C', str(ROOT)]
    metadata = {'target': 'linux-x86_64', 'minimum_glibc': minimum_glibc,
                'includes_game_data': False, 'imports': imports,
                'revision': subprocess.check_output(git+['rev-parse', 'HEAD'], text=True).strip(),
                'working_tree_modified': bool(subprocess.check_output(git+['status', '--porcelain'], text=True).strip()),
                'sha256': {name: hashlib.sha256(data).hexdigest() for name, data in sorted(files.items())}}
    files['ArcanumModern/manifest.json'] = (json.dumps(metadata, indent=2)+'\n').encode()
    output = output.resolve()
    output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='arcanum-linux-', dir=output.parent) as temp:
        archive = Path(temp)/'release.zip'
        with zipfile.ZipFile(archive, 'w', zipfile.ZIP_DEFLATED, compresslevel=6) as z:
            for name, data in sorted(files.items()):
                info = zipfile.ZipInfo(name)
                info.create_system = 3
                info.compress_type = zipfile.ZIP_DEFLATED
                executable = name in {'arcanum-modern', 'Play Arcanum Modern.sh'}
                info.external_attr = (0o100755 if executable else 0o100644) << 16
                z.writestr(info, data)
        archive.replace(output)
    print(f'Created {output} ({output.stat().st_size/1024/1024:.1f} MiB; glibc >= {minimum_glibc}; no game data)')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--exe', type=Path, required=True)
    parser.add_argument('--build-dir', type=Path, required=True)
    parser.add_argument('--output', type=Path, default=ROOT/'dist/ArcanumModern-linux-x64.zip')
    args = parser.parse_args()
    try:
        package(args.exe, args.build_dir, args.output)
    except (ValueError, OSError, struct.error, subprocess.CalledProcessError) as error:
        parser.exit(1, f'Packaging failed: {error}\n')
