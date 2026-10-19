#!/usr/bin/env python3
"""Package Windows resources, optionally including an installed game's data."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import struct
import subprocess
import tempfile
import zipfile

ROOT = Path(__file__).resolve().parents[1]
ASSETS = ('Book.ttf', 'Heading.ttf', 'FONT-LICENSE.txt', 'menu-background.png',
          'death-background.png', 'inventory-actions.png', 'skill-icons.png',
          'control-icons.png', 'splash/splash1.png', 'splash/splash2.png', 'splash/splash3.png')
SYSTEM_DLLS = set('kernel32.dll user32.dll winmm.dll advapi32.dll ole32.dll setupapi.dll shell32.dll '
                 'gdi32.dll oleaut32.dll imm32.dll version.dll ws2_32.dll bcrypt.dll comdlg32.dll '
                 'comctl32.dll shlwapi.dll secur32.dll rpcrt4.dll ntdll.dll ucrtbase.dll '
                 'msvcrt.dll winspool.drv cfgmgr32.dll dwmapi.dll'.split())
REQUIRED_GAME_FILES = {'arcanum1.dat', 'arcanum2.dat', 'arcanum3.dat', 'tig.dat', 'modules/arcanum.dat'}
MOVIE_LIBRARIES = {'windows': ('avformat-62.dll', 'avcodec-62.dll', 'avutil-60.dll'),
                   'macos': ('libavformat.62.dylib', 'libavcodec.62.dylib', 'libavutil.60.dylib'),
                   'linux': ('libavformat.so.62', 'libavcodec.so.62', 'libavutil.so.60')}


def movie_files(build_dir, platform):
    cache = (build_dir/'CMakeCache.txt').read_text()
    match = re.search(r'^ARCANUM_FFMPEG_ROOT:PATH=(.+)$', cache, re.M)
    if not match:
        raise ValueError('Missing FFmpeg decoder build')
    root = Path(match[1].strip())
    library_dir = root/('bin' if platform == 'windows' else 'lib')
    destination = '' if platform == 'windows' else 'ArcanumModern/lib/'
    files = {destination+name: (library_dir/name).read_bytes() for name in MOVIE_LIBRARIES[platform]}
    for name in ('ffmpeg-8.1.3.tar.xz', 'COPYING.LGPLv2.1', 'build-ffmpeg.py', 'README.txt'):
        files['ArcanumModern/licenses/ffmpeg/'+name] = (root/'share/arcanum-ffmpeg'/name).read_bytes()
    if hashlib.sha256(files['ArcanumModern/licenses/ffmpeg/ffmpeg-8.1.3.tar.xz']).hexdigest() != '7138d28c96d9d3e3af4ee3d8cad72741f8ffb40da90c1112235dea3ecd3178a3':
        raise ValueError('FFmpeg source archive checksum mismatch')
    return files


def validate_pe_dependencies(files):
    bundled = set(MOVIE_LIBRARIES['windows'])
    for name in ('arcanum-modern.exe', *bundled):
        imports = pe_imports(files[name])
        missing = sorted(n for n in imports if n not in SYSTEM_DLLS | bundled
                         and not n.startswith(('api-ms-win-', 'ext-ms-win-')))
        if missing:
            raise ValueError(name+' requires unbundled DLLs: '+', '.join(missing))


def game_files(directory):
    directory = directory.resolve(strict=True)
    files = {}
    excluded = {'save', 'saves', 'players', 'tigcache', 'screenshots', '__macosx'}

    def collect(folder, prefix):
        for base, dirs, names in os.walk(folder):
            dirs[:] = sorted(d for d in dirs if not d.startswith('.')
                             and d.lower() not in excluded and not d.lower().endswith('.app')
                             and not (Path(base)/d).is_symlink())
            for name in sorted(names):
                path = Path(base)/name
                if (name.startswith('.') or path.is_symlink()
                        or path.suffix.lower() in {'.sav', '.gsi', '.tfaf', '.tfai', '.log', '.exe', '.dll'}
                        or re.fullmatch(r'screen\d+\.bmp', name, re.I)):
                    continue
                files[prefix+'/'+path.relative_to(folder).as_posix()] = path

    for path in sorted(directory.iterdir()):
        if path.is_file() and not path.is_symlink() and path.suffix.lower() == '.dat':
            files[path.name] = path
        elif path.is_dir() and not path.is_symlink() and path.name.lower() in {'data', 'modules', 'proto', 'movies', 'sound'}:
            collect(path, path.name)
    missing = REQUIRED_GAME_FILES - {name.lower() for name in files}
    if missing:
        raise ValueError('Incomplete game installation; missing: '+', '.join(sorted(missing)))
    # The HD cache may live outside the installation via a directory symlink.
    maps = directory/'modern-ui/local-maps'
    if maps.exists():
        collect(maps.resolve(strict=True), 'modern-ui/local-maps')
    return files


def write_archive(output, files, metadata):
    output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.NamedTemporaryFile(dir=output.parent, suffix='.zip', delete=False) as f:
        temporary = Path(f.name)
    hashes = {}
    try:
        with zipfile.ZipFile(temporary, 'w', zipfile.ZIP_DEFLATED, compresslevel=6) as archive:
            for index, (name, data) in enumerate(sorted(files.items()), 1):
                if isinstance(data, Path):
                    info = zipfile.ZipInfo(name)
                    info.compress_type = (zipfile.ZIP_STORED if data.suffix.lower() in {'.png', '.bik', '.ogg'}
                                          else zipfile.ZIP_DEFLATED)
                    digest = hashlib.sha256()
                    with data.open('rb') as source, archive.open(info, 'w', force_zip64=True) as target:
                        while block := source.read(1024*1024):
                            target.write(block)
                            digest.update(block)
                    hashes[name] = digest.hexdigest()
                else:
                    archive.writestr(name, data)
                    hashes[name] = hashlib.sha256(data).hexdigest()
                if index % 2000 == 0:
                    print(f'Packed {index}/{len(files)} files', flush=True)
            archive.writestr('ArcanumModern/manifest.json',
                             json.dumps({**metadata, 'sha256': hashes}, indent=2)+'\n')
        temporary.replace(output)
    finally:
        temporary.unlink(missing_ok=True)


def pe_imports(data):
    """Reject non-x64 binaries and inspect normal and delay-loaded DLL imports."""
    if data[:2] != b'MZ':
        raise ValueError('Expected a Windows PE executable, not a Linux binary')
    pe = struct.unpack_from('<I', data, 60)[0]
    if data[pe:pe+4] != b'PE\0\0':
        raise ValueError('Invalid PE signature')
    machine, count = struct.unpack_from('<HH', data, pe+4)
    opt_size = struct.unpack_from('<H', data, pe+20)[0]
    opt = pe+24
    if machine != 0x8664 or struct.unpack_from('<H', data, opt)[0] != 0x20b:
        raise ValueError('The package requires a Windows x86-64 executable')
    sections = []
    for i in range(count):
        size, rva, raw_size, offset = struct.unpack_from('<IIII', data, opt+opt_size+i*40+8)
        sections.append((rva, raw_size, offset))

    def offset(rva):
        for start, size, raw in sections:
            if start <= rva < start+size:
                return raw+rva-start
        raise ValueError(f'Invalid PE address: {rva:x}')

    names = set()
    directory_count = struct.unpack_from('<I', data, opt+108)[0]
    for directory, stride, name_offset in ((1,20,12),(13,32,4)):
        if directory >= directory_count:
            continue
        rva, size = struct.unpack_from('<II', data, opt+112+directory*8)
        if not rva:
            continue
        pos = offset(rva)
        for at in range(pos, pos+size, stride):
            if not any(data[at:at+stride]):
                break
            if directory == 13 and struct.unpack_from('<I',data,at)[0] != 1:
                raise ValueError('Unsupported delay import format')
            name = offset(struct.unpack_from('<I',data,at+name_offset)[0])
            end = data.index(0,name)
            names.add(data[name:end].decode('ascii').lower())
    return names


def package(exe, build_dir, output, include_game=None):
    binary = exe.read_bytes()
    imports = pe_imports(binary)
    files = {'arcanum-modern.exe': binary,
             'Play Arcanum Modern.bat': (ROOT/'tools/windows/Play Arcanum Modern.bat').read_bytes().replace(b'\r\n',b'\n').replace(b'\n',b'\r\n'),
             'ArcanumModern/INSTALL.txt': (ROOT/'tools/windows/INSTALL.txt').read_bytes(),
             'ArcanumModern/licenses/arcanum-ce.md': (ROOT/'source/LICENSE.md').read_bytes()}
    files.update(movie_files(build_dir, 'windows'))
    validate_pe_dependencies(files)
    for name in ASSETS:
        files['modern-ui/'+name] = (ROOT/'game/modern-ui'/name).read_bytes()
    if include_game:
        files.update(game_files(include_game))
        files['ArcanumModern/INSTALL.txt'] = (ROOT/'tools/windows/INSTALL-FULL.txt').read_bytes()
    readme = ROOT/'README.md'
    if readme.exists():
        files['ArcanumModern/README.md'] = readme.read_text().replace('(game/modern-ui/', '(../modern-ui/').replace('(docs/screenshots/', '(https://raw.githubusercontent.com/cdomergue/arcanum-modern/main/docs/screenshots/').encode()
    files['ArcanumModern/docs/controls.md'] = (ROOT/'docs/controls.md').read_bytes()
    cache = (build_dir/'CMakeCache.txt').read_text()
    dependencies = [('SDL3', 'LICENSE.txt'), ('SDL3_mixer','LICENSE.txt'), ('fpattern','LICENSE'), ('zlib','LICENSE')]
    for name, license_file in dependencies:
        match = re.search(r'^'+name+r'_SOURCE_DIR:[^=]+=(.+)$',cache,re.M)
        if not match:
            raise ValueError('Missing dependency source for license: '+name)
        directory = Path(match[1].strip())
        files['ArcanumModern/licenses/'+name+'.txt'] = (directory/license_file).read_bytes()
        extras = {'SDL3':['src/hidapi/LICENSE-bsd.txt'], 'SDL3_mixer':['src/dr_libs/LICENSE']}
        for extra in extras.get(name,[]):
            files['ArcanumModern/licenses/'+name+'-'+Path(extra).parent.name+'.txt'] = (directory/extra).read_bytes()
    stb = (ROOT/'source/third_party/stb/stb_truetype.h').read_text()
    files['ArcanumModern/licenses/stb.txt'] = stb[stb.rindex('/*\n---'):].encode()
    toolchain = os.environ.get('LLVM_MINGW')
    if toolchain:
        tc = Path(toolchain)
        files['ArcanumModern/licenses/llvm.txt'] = (tc/'LICENSE.TXT').read_bytes()
        for p in (tc/'x86_64-w64-mingw32/share/mingw32').glob('COPYING*'):
            files['ArcanumModern/licenses/'+p.name] = p.read_bytes()
    git = ['git','-c',f'safe.directory={ROOT}','-C',str(ROOT)]
    revision = subprocess.check_output(git+['rev-parse','HEAD'],text=True).strip()
    dirty = bool(subprocess.check_output(git+['status','--porcelain'],text=True).strip())
    write_archive(output, files, {'revision':revision,'working_tree_modified':dirty,
        'target':'windows-x86_64','imports':sorted(imports),'includes_game_data':bool(include_game)})
    print(f'Created {output} ({output.stat().st_size/1024/1024:.1f} MiB, {len(files)+1} files)')


if __name__ == '__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--exe',type=Path,required=True)
    parser.add_argument('--build-dir',type=Path,required=True)
    parser.add_argument('--output',type=Path)
    parser.add_argument('--include-game',type=Path,metavar='DIRECTORY',
                        help='Include game data and HD maps from this installation; exclude saves and personal preferences')
    args=parser.parse_args()
    output = args.output or ROOT/('dist/ArcanumModern-windows-x64-full.zip' if args.include_game
                                 else 'dist/ArcanumModern-windows-x64.zip')
    try:
        package(args.exe,args.build_dir,output,args.include_game)
    except (ValueError,OSError,struct.error) as error:
        parser.exit(1,f'Packaging failed: {error}\n')
