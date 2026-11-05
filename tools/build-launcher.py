#!/usr/bin/env python3
"""Build the self-contained desktop launcher with the pinned Go toolchain."""
import importlib.util
import os
from pathlib import Path
import platform
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def build(destination, target="windows", cache=ROOT/'build/uap-tools'):
    spec = importlib.util.spec_from_file_location('uap_build', ROOT/'tools/build-uap.py')
    helper = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(helper)
    host_os = platform.system().lower()
    host_arch = {'aarch64': 'arm64', 'arm64': 'arm64', 'x86_64': 'amd64', 'AMD64': 'amd64'}[platform.machine()]
    host = host_os+'-'+host_arch
    archive = helper.download(cache, f'go{helper.GO}.{host}.tar.gz',
                              f'https://go.dev/dl/go{helper.GO}.{host}.tar.gz', helper.GO_HASHES[host])
    compiler = cache/('go-'+helper.GO+'-'+host)
    if not (compiler/'go/bin/go').exists():
        helper.extract(archive, compiler)
    version = re.search(r'ARCANUM_RENAISSANCE_VERSION "([^"]+)"',
                        (ROOT/'source/src/ui/modern_version.h').read_text()).group(1)
    env = dict(os.environ, CGO_ENABLED='0', GOTOOLCHAIN='local', GOCACHE=str(cache/'go-cache'))
    go = str(compiler/'go/bin/go')
    subprocess.run([go, 'test', './...'], cwd=ROOT/'tools/launcher', env=env, check=True)
    destination.parent.mkdir(parents=True, exist_ok=True)
    target_os, target_arch = {'windows': ('windows', 'amd64'), 'linux': ('linux', 'amd64'), 'macos': ('darwin', 'arm64')}[target]
    flags = '-s -w -X main.buildVersion='+version + (' -H=windowsgui' if target == 'windows' else '')
    subprocess.run([go, 'build', '-buildvcs=false', '-trimpath',
                    '-ldflags='+flags,
                    '-o', str(destination), '.'], cwd=ROOT/'tools/launcher',
                   env=dict(env, GOOS=target_os, GOARCH=target_arch), check=True)
    test_binary = ROOT/('build/launcher/launcher-tests.exe' if target == 'windows' else 'build/launcher/launcher-tests-'+target)
    test_binary.parent.mkdir(parents=True, exist_ok=True)
    subprocess.run([go, 'test', '-c', '-o', str(test_binary)], cwd=ROOT/'tools/launcher',
                   env=dict(env, GOOS=target_os, GOARCH=target_arch), check=True)
    return destination


def files(target="windows", build_dir=None):
    with tempfile.TemporaryDirectory(prefix='renaissance-launcher-') as directory:
        name = 'renaissance-launcher.exe' if target == 'windows' else 'renaissance-launcher'
        binary = build(Path(directory)/name, target)
        result = {name: binary.read_bytes()}
        if target != 'windows':
            ui = build_dir/'renaissance-launcher-ui'
            if not ui.is_file(): ui = build_dir/'Release/renaissance-launcher-ui'
            result['ArcanumModern/launcher/renaissance-launcher-ui'] = ui.read_bytes()
            result['ArcanumModern/launcher/background.png'] = (ROOT/'tools/launcher/assets/background.png').read_bytes()
        return result


if __name__ == '__main__':
    import argparse
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, default=ROOT/'build/launcher/renaissance-launcher.exe')
    parser.add_argument('--target', choices=('windows', 'linux', 'macos'), default='windows')
    args = parser.parse_args()
    build(args.output.resolve(), args.target)
