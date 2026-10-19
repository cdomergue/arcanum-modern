#!/usr/bin/env python3
"""Build the LGPL Bink decoder libraries used by the native movie player."""
import argparse
import hashlib
import os
from pathlib import Path
import shutil
import subprocess
import tarfile
import urllib.request

VERSION = '8.1.3'
SHA256 = '7138d28c96d9d3e3af4ee3d8cad72741f8ffb40da90c1112235dea3ecd3178a3'
URL = f'https://ffmpeg.org/releases/ffmpeg-{VERSION}.tar.xz'


def build(args):
    work = args.work.resolve()
    prefix = args.prefix.resolve()
    work.mkdir(parents=True, exist_ok=True)
    archive = work/f'ffmpeg-{VERSION}.tar.xz'
    if not archive.exists():
        temporary = archive.with_suffix('.download')
        urllib.request.urlretrieve(URL, temporary)
        temporary.replace(archive)
    if hashlib.sha256(archive.read_bytes()).hexdigest() != SHA256:
        raise ValueError('FFmpeg source checksum mismatch')
    source = work/f'ffmpeg-{VERSION}'
    if not source.exists():
        with tarfile.open(archive) as tar:
            tar.extractall(work, filter='data')
    output = work/'objects'
    output.mkdir(exist_ok=True)
    flags = [f'--prefix={prefix}', '--disable-autodetect', '--disable-everything',
             '--disable-programs', '--disable-doc', '--disable-debug', '--enable-small',
             '--disable-network', '--disable-static', '--enable-shared', '--disable-asm',
             '--disable-avdevice', '--disable-avfilter', '--disable-swscale', '--disable-swresample',
             '--enable-avcodec', '--enable-avformat', '--enable-avutil',
             '--enable-decoder=bink,binkaudio_rdft,binkaudio_dct', '--enable-demuxer=bink',
             '--enable-protocol=file', f'--cc={args.cc}', f'--arch={args.arch}']
    if args.msvc:
        flags = [flag for flag in flags if not flag.startswith('--cc=')]
        flags += ['--target-os=win32', '--toolchain=msvc', '--cc=cl', '--ld=link', '--ar=lib', '--nm=dumpbin']
    elif args.system == 'Windows':
        flags += ['--enable-cross-compile', '--target-os=mingw32',
                  '--cross-prefix='+str(Path(args.cc).parent/'x86_64-w64-mingw32-')]
    elif args.system == 'Darwin':
        flags += ['--target-os=darwin', '--install-name-dir=@rpath',
                  '--extra-cflags=-mmacosx-version-min=11.0', '--extra-ldflags=-mmacosx-version-min=11.0']
    elif args.system == 'Linux':
        flags += ['--target-os=linux']
    else:
        raise ValueError('Unsupported movie decoder platform: '+args.system)
    subprocess.run(['sh', str(source/'configure'), *flags], cwd=output, check=True)
    subprocess.run(['make', '-j'+str(min(os.cpu_count() or 2, 8))], cwd=output, check=True)
    subprocess.run(['make', 'install'], cwd=output, check=True)
    notices = prefix/'share/arcanum-ffmpeg'
    notices.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(archive, notices/archive.name)
    shutil.copyfile(source/'COPYING.LGPLv2.1', notices/'COPYING.LGPLv2.1')
    shutil.copyfile(__file__, notices/'build-ffmpeg.py')
    (notices/'README.txt').write_text(
        f'FFmpeg {VERSION}, https://ffmpeg.org/\n'
        'Licensed under LGPL 2.1 or later. No GPL or nonfree components are enabled.\n'
        'These shared libraries may be replaced with compatible builds.\n'
        'Exact source and the build script are included here.\n'
        'Configure arguments for this build:\n'+'\n'.join(flags)+'\n')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--work', type=Path, required=True)
    parser.add_argument('--prefix', type=Path, required=True)
    parser.add_argument('--cc', required=True)
    parser.add_argument('--system', required=True)
    parser.add_argument('--arch', required=True)
    parser.add_argument('--msvc', action='store_true')
    build(parser.parse_args())
