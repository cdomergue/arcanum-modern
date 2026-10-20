#!/usr/bin/env python3
"""Build the portable UAP preparation helper and bundle its extraction tools."""
import argparse
import hashlib
import os
from pathlib import Path
import platform
import shutil
import subprocess
import tarfile
import tempfile
import urllib.request

ROOT = Path(__file__).resolve().parents[1]
GO = '1.27.3'
GO_HASHES = {
    'darwin-arm64': 'dd3908cf20959d798c1e06ebe5b72213f0505ce01373b40ce3110975a6cd99c2',
    'darwin-amd64': 'b6f8e931e4ab497fcde6da8ae4499a58f9d258e55c8a80824a516eae5950a750',
    'linux-amd64': '65efbc80fdc1fa2cd2631cad5b481777a68e8725f34e89f50fc6ac714e379b40',
    'linux-arm64': 'a06e99309dbcbdca1a8ccd2282c3ee146e55930886e26b4f1193269587165f49',
}
SEVEN_HASHES = {
    '7z2604-mac.tar.xz': 'bee04358cbcbc7106273cee0e8d72916db2696c48067a3538c34d8cd6fd16578',
    '7z2604-linux-x64.tar.xz': 'fc0327ba27e89bd086cf426dff17d77de582953cdbbc10a6576540a06853ffcd',
    '7z2604-linux-arm64.tar.xz': '5b0ac3aa91c1e3499f011d6af69d7f6b4de3ef2f011d9e2ff44c58ebd04aa388',
    '7z2604-x64.exe': 'd54bf805f9f3704d1e8db2fa3498ae7ef2df0312b40b558e7c71c734430a665d',
    '7z2604-src.tar.xz': '9691944c0fe0d01bb49373a704fb983fd33bc98b1738695179dfbf99ac1734f6',
}


def download(cache, name, url, digest):
    path = cache/name
    if path.exists() and hashlib.sha256(path.read_bytes()).hexdigest() == digest:
        return path
    cache.mkdir(parents=True, exist_ok=True)
    with tempfile.NamedTemporaryFile(dir=cache, delete=False) as output:
        temporary = Path(output.name)
        try:
            with urllib.request.urlopen(url, timeout=60) as source:
                shutil.copyfileobj(source, output)
            output.flush()
            if hashlib.sha256(temporary.read_bytes()).hexdigest() != digest:
                raise ValueError('Download checksum mismatch: '+name)
            temporary.replace(path)
        finally:
            temporary.unlink(missing_ok=True)
    return path


def seven_archive(cache, name):
    return download(cache, name, 'https://github.com/ip7z/7zip/releases/download/26.04/'+name, SEVEN_HASHES[name])


def extract(archive, destination):
    destination.mkdir(parents=True, exist_ok=True)
    with tarfile.open(archive) as source:
        source.extractall(destination, filter='data')


def build(target, destination, cache):
    host_os = platform.system().lower()
    host_arch = {'aarch64': 'arm64', 'arm64': 'arm64', 'x86_64': 'amd64', 'AMD64': 'amd64'}[platform.machine()]
    host = host_os+'-'+host_arch
    filename = f'go{GO}.{host}.tar.gz'
    compiler_archive = download(cache, filename, 'https://go.dev/dl/'+filename, GO_HASHES[host])
    compiler = cache/('go-'+GO+'-'+host)
    if not (compiler/'go/bin/go').exists():
        extract(compiler_archive, compiler)
    host_seven = '7z2604-mac.tar.xz' if host_os == 'darwin' else f'7z2604-linux-{"x64" if host_arch == "amd64" else "arm64"}.tar.xz'
    extract(seven_archive(cache, host_seven), cache/'7zip-host')
    os_name, arch = {'windows': ('windows', 'amd64'), 'macos': ('darwin', 'arm64'), 'linux': ('linux', 'amd64')}[target]
    destination.mkdir(parents=True, exist_ok=True)
    helper = destination/'ArcanumModern/uap'
    helper.mkdir(parents=True, exist_ok=True)
    env = dict(os.environ, GOOS=os_name, GOARCH=arch, CGO_ENABLED='0', GOCACHE=str(cache/'go-cache'), GOTOOLCHAIN='local')
    binary = helper/('uap-helper.exe' if target == 'windows' else 'uap-helper')
    subprocess.run([str(compiler/'go/bin/go'), 'test', './...'], cwd=ROOT/'tools/uap',
                   env=dict(env, GOOS=host_os, GOARCH=host_arch), check=True)
    subprocess.run([str(compiler/'go/bin/go'), 'build', '-trimpath', '-ldflags=-s -w', '-o', str(binary), '.'], cwd=ROOT/'tools/uap', env=env, check=True)
    with tempfile.TemporaryDirectory(dir=cache) as work:
        unpack = Path(work)
        if target == 'windows':
            archive = seven_archive(cache, '7z2604-x64.exe')
            subprocess.run([str(cache/'7zip-host/7zz'), 'x', '-y', '-o'+str(unpack), str(archive)], check=True, stdout=subprocess.DEVNULL)
            names = ['7z.exe', '7z.dll']
        else:
            archive = seven_archive(cache, '7z2604-mac.tar.xz' if target == 'macos' else '7z2604-linux-x64.tar.xz')
            extract(archive, unpack)
            names = ['7zz']
        for name in names:
            shutil.copyfile(unpack/("7zzs" if target == "linux" and name == "7zz" else name), helper/name)
            (helper/name).chmod(0o755)
        notices = destination/'ArcanumModern/licenses/7zip'
        notices.mkdir(parents=True, exist_ok=True)
        license_file = next(p for p in unpack.iterdir() if p.name.lower() == 'license.txt')
        shutil.copyfile(license_file, notices/'LICENSE.txt')
        shutil.copyfile(seven_archive(cache, '7z2604-src.tar.xz'), notices/'7z2604-src.tar.xz')
        (notices/'README.txt').write_text('7-Zip 26.04, https://www.7-zip.org/\nUnmodified extraction binaries. Corresponding source is included.\n')
    shutil.copyfile(compiler/'go/LICENSE', destination/'ArcanumModern/licenses/Go.txt')
    docs = destination/'ArcanumModern/docs'
    docs.mkdir(parents=True, exist_ok=True)
    for name in ('uap.md', 'uap-fr.md'):
        shutil.copyfile(ROOT/'docs'/name, docs/name)
    if target == 'macos':
        for name in ('uap-helper', '7zz'):
            subprocess.run(['codesign', '--force', '--sign', '-', str(helper/name)], check=True)
    return {p.relative_to(destination).as_posix(): p.read_bytes() for p in destination.rglob('*') if p.is_file()}


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--target', required=True, choices=('windows', 'macos', 'linux'))
    parser.add_argument('--output', required=True, type=Path)
    parser.add_argument('--cache', type=Path, default=ROOT/'build/uap-tools')
    args = parser.parse_args()
    build(args.target, args.output.resolve(), args.cache.resolve())
