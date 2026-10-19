#!/usr/bin/env python3
"""Create a standalone arm64 macOS ZIP without game data or personal settings."""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import plistlib
import re
import shutil
import struct
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def validate_macho(data):
    # Require a thin, little-endian ARM64 executable, not an Intel/Rosetta build.
    if len(data) < 32 or struct.unpack_from('<III', data) != (0xfeedfacf, 0x100000c, 0):
        raise ValueError('Expected a native arm64 Mach-O binary')
    if struct.unpack_from('<I', data, 12)[0] != 2:
        raise ValueError('Expected a Mach-O executable')


def validate_dependencies(output):
    dependencies = []
    for line in output.splitlines()[1:]:
        name = line.strip().split(' (compatibility version', 1)[0]
        if not name:
            continue
        if not name.startswith(('/System/Library/', '/usr/lib/')):
            raise ValueError('Non-system library required; rebuild statically: '+name)
        dependencies.append(name)
    return dependencies


def package(app, build_dir, output):
    info = plistlib.loads((app/'Contents/Info.plist').read_bytes())
    executable = info['CFBundleExecutable']
    if Path(executable).name != executable:
        raise ValueError('Invalid bundle executable name')
    binary = app/'Contents/MacOS'/executable
    validate_macho(binary.read_bytes())
    imports = validate_dependencies(subprocess.check_output(['otool', '-L', str(binary)], text=True))
    # Reuse the release asset allowlist, never copy the installed modern-ui folder.
    spec = importlib.util.spec_from_file_location('windows_package', ROOT/'tools/package-windows.py')
    resources = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(resources)
    output = output.resolve()
    output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='arcanum-macos-', dir=output.parent) as temporary:
        stage = Path(temporary)/'release'
        stage.mkdir()
        bundle = stage/'Arcanum Modern.app'
        shutil.copytree(app, bundle)
        info['CFBundleName'] = 'Arcanum Modern'
        info['CFBundleDisplayName'] = 'Arcanum Modern'
        info['CFBundleIdentifier'] = 'org.arcanummodern.game'
        # SDL_GetBasePath must resolve beside the .app, where the game data lives.
        info['SDL_FILESYSTEM_BASE_DIR_TYPE'] = 'parent'
        (bundle/'Contents/Info.plist').write_bytes(plistlib.dumps(info))
        (bundle/'Contents/MacOS'/executable).chmod(0o755)
        # Apple Silicon requires signed code. This is an ad-hoc signature,
        # not Developer ID signing or notarization.
        subprocess.run(['codesign', '--force', '--sign', '-', str(bundle)], check=True)
        subprocess.run(['codesign', '--verify', '--strict', str(bundle)], check=True)
        for name in resources.ASSETS:
            destination = stage/'modern-ui'/name
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(ROOT/'game/modern-ui'/name, destination)
        notices = stage/'ArcanumModern'
        licenses = notices/'licenses'
        licenses.mkdir(parents=True)
        shutil.copyfile(ROOT/'source/LICENSE.md', licenses/'arcanum-ce.md')
        cache = (build_dir/'CMakeCache.txt').read_text()
        for name, license_file in [('SDL3', 'LICENSE.txt'), ('SDL3_mixer', 'LICENSE.txt'),
                                   ('fpattern', 'LICENSE'), ('zlib', 'LICENSE')]:
            match = re.search(r'^'+name+r'_SOURCE_DIR:[^=]+=(.+)$', cache, re.M)
            if not match:
                raise ValueError('Missing dependency source for license: '+name)
            directory = Path(match[1].strip())
            shutil.copyfile(directory/license_file, licenses/(name+'.txt'))
            extras = {'SDL3': ['src/hidapi/LICENSE-bsd.txt'],
                      'SDL3_mixer': ['src/dr_libs/LICENSE']}
            for extra in extras.get(name, []):
                shutil.copyfile(directory/extra, licenses/(name+'-'+Path(extra).parent.name+'.txt'))
        stb = (ROOT/'source/third_party/stb/stb_truetype.h').read_text()
        (licenses/'stb.txt').write_text(stb[stb.rindex('/*\n---'):])
        shutil.copyfile(ROOT/'tools/macos/INSTALL.txt', notices/'INSTALL.txt')
        (notices/'README.md').write_text((ROOT/'README.md').read_text().replace('(game/modern-ui/', '(../modern-ui/'))
        (notices/'docs').mkdir()
        shutil.copyfile(ROOT/'docs/controls.md', notices/'docs/controls.md')
        git = ['git', '-c', f'safe.directory={ROOT}', '-C', str(ROOT)]
        manifest = {
            'target': 'macos-arm64', 'minimum_macos': '11.0', 'signature': 'ad-hoc',
            'revision': subprocess.check_output(git+['rev-parse', 'HEAD'], text=True).strip(),
            'working_tree_modified': bool(subprocess.check_output(git+['status', '--porcelain'], text=True).strip()),
            'imports': imports,
            'sha256': {p.relative_to(stage).as_posix(): hashlib.sha256(p.read_bytes()).hexdigest()
                       for p in sorted(stage.rglob('*')) if p.is_file()},
        }
        (notices/'manifest.json').write_text(json.dumps(manifest, indent=2)+'\n')
        archive = Path(temporary)/'release.zip'
        subprocess.run(['ditto', '-c', '-k', '--sequesterRsrc', str(stage), str(archive)], check=True)
        archive.replace(output)
    print(f'Created {output} ({output.stat().st_size/1024/1024:.1f} MiB)')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--app', type=Path, required=True)
    parser.add_argument('--build-dir', type=Path, required=True)
    parser.add_argument('--output', type=Path, default=ROOT/'dist/ArcanumModern-macos-arm64.zip')
    args = parser.parse_args()
    try:
        package(args.app, args.build_dir, args.output)
    except (ValueError, OSError, KeyError, struct.error, subprocess.CalledProcessError) as error:
        parser.exit(1, f'Packaging failed: {error}\n')
