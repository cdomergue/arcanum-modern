#!/usr/bin/env python3
"""Build and verify the portable WorldED Windows archive without game assets."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import zipfile

ROOT = Path(__file__).resolve().parents[1]
PROGRAMS = ('worlded-explorer.exe', 'worlded.exe', 'worlded-house-export.exe')
HELPER = 'worlded-native-inventory.exe'
MOVIES = ('avformat-62.dll', 'avcodec-62.dll', 'avutil-60.dll')
REQUIRED = (*PROGRAMS, HELPER, *MOVIES, 'Qt6Core.dll', 'Qt6Gui.dll',
            'Qt6Widgets.dll', 'msvcp140.dll', 'vcruntime140.dll', 'vcruntime140_1.dll', 'platforms/qwindows.dll', 'worlded-tutorial.html',
            'tools/world-updates/package.py', 'tools/world-updates/FORMAT.md',
            'docs/worlded.md', 'docs/housing.md', 'docs/world-updates.md', 'README.txt')


def verify_archive(path):
    with zipfile.ZipFile(path) as archive:
        names = archive.namelist()
        if len(names) != len(set(name.casefold() for name in names)):
            raise ValueError('Duplicate or case-colliding archive entries')
        for name in names:
            parts = name.split('/')
            if name.startswith('/') or '\\' in name or ':' in name or any(p in ('', '.', '..') for p in parts):
                raise ValueError('Unsafe archive path: ' + name)
            if name.lower().endswith(('.dat', '.sav', '.tfaf', '.tfai', '.gsi', '.pdb')):
                raise ValueError('Game data, saves or debug files in editor archive: ' + name)
        missing = set(REQUIRED) - set(names)
        if missing:
            raise ValueError('Missing package files: ' + ', '.join(sorted(missing)))
        manifest = json.loads(archive.read('package-manifest.json'))
        expected = set(names) - {'package-manifest.json'}
        if set(manifest['files']) != expected:
            raise ValueError('Package manifest file list differs from ZIP')
        for name, digest in manifest['files'].items():
            if hashlib.sha256(archive.read(name)).hexdigest() != digest:
                raise ValueError('Package digest mismatch: ' + name)
        if archive.testzip():
            raise ValueError('ZIP checksum failure')


def smoke_test(path):
    if os.name != 'nt':
        raise RuntimeError('Windows is required for executable smoke tests')
    verify_archive(path)
    with tempfile.TemporaryDirectory(prefix='worlded-smoke-') as temporary:
        with zipfile.ZipFile(path) as archive:
            archive.extractall(temporary)  # All names were bounded by verify_archive.
        env = dict(os.environ)
        for key in list(env):
            if key.upper().startswith('QT_') or key.upper() in ('QML_IMPORT_PATH', 'QML2_IMPORT_PATH'):
                del env[key]
        system = Path(env.get('SystemRoot', 'C:/Windows'))
        env['PATH'] = str(system/'System32') + os.pathsep + str(system)
        for name, args, expected in ((PROGRAMS[0], ['--help'], 0),
                                     (PROGRAMS[1], ['--help'], 0),
                                     (PROGRAMS[2], [], 2), (HELPER, [], 2)):
            result = subprocess.run([str(Path(temporary)/name), *args], cwd=temporary,
                                    env=env, capture_output=True, timeout=30)
            if result.returncode != expected:
                raise RuntimeError(f'{name}: exit {result.returncode}: {result.stderr.decode(errors="replace")}')


def copy_msvc_runtime(stage):
    """Ship app-local CRT libraries even outside a VS developer prompt."""
    vswhere = Path(os.environ.get('ProgramFiles(x86)', 'C:/Program Files (x86)'))/'Microsoft Visual Studio/Installer/vswhere.exe'
    roots = []
    if os.environ.get('VCINSTALLDIR'):
        roots.append(Path(os.environ['VCINSTALLDIR'])/'Redist/MSVC')
    if vswhere.is_file():
        result = subprocess.run([str(vswhere), '-latest', '-products', '*', '-requires',
                                 'Microsoft.VisualStudio.Component.VC.Tools.x86.x64',
                                 '-property', 'installationPath'], check=True, capture_output=True, text=True)
        if result.stdout.strip():
            roots.append(Path(result.stdout.strip())/'VC/Redist/MSVC')
    for root in roots:
        versions = sorted((p for p in root.glob('*') if p.name[0].isdigit()),
                          key=lambda p: tuple(int(n) for n in p.name.split('.')), reverse=True)
        for version in versions:
            libraries = list(version.glob('x64/Microsoft.VC*.CRT/*.dll'))
            if libraries:
                for library in libraries:
                    shutil.copy2(library, stage/library.name)
                return
    raise RuntimeError('MSVC x64 redistributable DLLs not found; install Visual Studio C++ build tools')


def package(args):
    build = args.build_dir.resolve()
    programs = build/'Release' if (build/'Release'/PROGRAMS[0]).is_file() else build
    helper = args.inventory_helper.resolve(strict=True)
    qtbin = args.qt_bin.resolve(strict=True)
    deploy = qtbin/'windeployqt.exe'
    if not deploy.is_file():
        raise ValueError('Choose the matching Qt MSVC bin directory')
    output = args.output.resolve()
    output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='.worlded-package-', dir=output.parent) as temporary:
        stage = Path(temporary)
        for name in PROGRAMS:
            shutil.copy2(programs/name, stage/name)
        shutil.copy2(helper, stage/HELPER)
        if args.engine_package:
            with zipfile.ZipFile(args.engine_package) as engine:
                for name in MOVIES:
                    (stage/name).write_bytes(engine.read(name))
                prefix = 'ArcanumModern/licenses/ffmpeg/'
                for name in engine.namelist():
                    if name.startswith(prefix) and not name.endswith('/'):
                        relative = Path(name[len(prefix):])
                        if len(relative.parts) != 1 or relative.name in ('.', '..'):
                            raise ValueError('Unexpected engine license path')
                        target = stage/'licenses/ffmpeg'/relative
                        target.parent.mkdir(parents=True, exist_ok=True)
                        target.write_bytes(engine.read(name))
        else:
            # Local native builds emit these DLLs beside the helper.
            for name in MOVIES:
                shutil.copy2(helper.parent/name, stage/name)
            cache = next((p/'CMakeCache.txt' for p in (helper.parent, helper.parent.parent)
                          if (p/'CMakeCache.txt').is_file()), None)
            if cache:
                for line in cache.read_text(encoding='utf-8').splitlines():
                    if line.startswith('ARCANUM_FFMPEG_ROOT:PATH='):
                        notices = Path(line.split('=', 1)[1])/'share/arcanum-ffmpeg'
                        shutil.copytree(notices, stage/'licenses/ffmpeg')
                        break
        for path in args.extra_dll:
            if path.suffix.lower() != '.dll':
                raise ValueError('--extra-dll accepts DLL files only')
            shutil.copy2(path, stage/path.name)
        subprocess.run([str(deploy), '--release', '--compiler-runtime', '--no-translations',
                        str(stage/PROGRAMS[0])], check=True)
        copy_msvc_runtime(stage)
        for path in sorted((ROOT/'docs').glob('*.md')):
            (stage/'docs').mkdir(exist_ok=True)
            shutil.copy2(path, stage/'docs'/path.name)
        for path in (ROOT/'tools/world-updates/package.py', ROOT/'tools/world-updates/FORMAT.md',
                     ROOT/'tools/worlded/TERRAIN.md', ROOT/'tools/worlded/COVERAGE.md'):
            if path.is_file():
                target = stage/path.relative_to(ROOT)
                target.parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(path, target)
        shutil.copy2(ROOT/'worlded-tutorial.html', stage/'worlded-tutorial.html')
        (stage/'README.txt').write_text(
            'WorldED Renaissance for Windows x64\n\n'
            'Open worlded-tutorial.html in a browser for the complete offline guide.\n'
            'Run worlded-explorer.exe and choose your Arcanum installation.\n'
            'Keep every DLL and plugin folder beside the executables.\n'
            'The Microsoft C++ runtime DLLs are included beside the editor.\n'
            'Playtests require a matching Renaissance game executable and your game files.\n'
            'Save projects outside the game installation. No game archives or saves are included.\n', encoding='utf-8')
        if (ROOT/'source/LICENSE.md').is_file():
            (stage/'licenses').mkdir(exist_ok=True)
            shutil.copy2(ROOT/'source/LICENSE.md', stage/'licenses/arcanum-ce-LICENSE.md')
        files = {p.relative_to(stage).as_posix(): hashlib.sha256(p.read_bytes()).hexdigest()
                 for p in sorted(stage.rglob('*')) if p.is_file()}
        manifest = {'format': 'worlded-windows-package', 'version': 1, 'files': files}
        (stage/'package-manifest.json').write_text(json.dumps(manifest, indent=2)+'\n', encoding='utf-8')
        archive = stage/'output.zip'
        with zipfile.ZipFile(archive, 'w', zipfile.ZIP_DEFLATED, compresslevel=6) as out:
            for path in sorted(stage.rglob('*')):
                if path.is_file() and path != archive:
                    out.write(path, path.relative_to(stage).as_posix())
        verify_archive(archive)
        if args.smoke_test:
            smoke_test(archive)
        os.replace(archive, output)
    print(output)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build-dir', type=Path, default=ROOT/'build/worlded-windows')
    parser.add_argument('--inventory-helper', type=Path)
    parser.add_argument('--qt-bin', type=Path)
    parser.add_argument('--engine-package', type=Path)
    parser.add_argument('--extra-dll', type=Path, action='append', default=[])
    parser.add_argument('--output', type=Path, default=ROOT/'dist/worlded-renaissance-win.zip')
    parser.add_argument('--smoke-test', action='store_true')
    parser.add_argument('--verify', type=Path)
    args = parser.parse_args()
    if args.verify:
        verify_archive(args.verify)
        if args.smoke_test: smoke_test(args.verify)
        print('Verified', args.verify)
    else:
        if not args.inventory_helper or not args.qt_bin:
            parser.error('--inventory-helper and --qt-bin are required when packaging')
        package(args)

if __name__ == '__main__':
    main()
