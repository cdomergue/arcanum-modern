"""Keep dependency source delivery separate without losing matching source or notices."""
import hashlib
import importlib.util
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch
import zipfile

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('packager', ROOT/'tools/package-windows.py')
p = importlib.util.module_from_spec(spec)
spec.loader.exec_module(p)
PREFIX = 'ArcanumModern/licenses/'
REMOVED = {PREFIX+'ffmpeg/ffmpeg-8.1.3.tar.xz', PREFIX+'ffmpeg/build-ffmpeg.py',
           PREFIX+'7zip/7z2604-src.tar.xz'}


def verify_pair(runtime, sources):
    with zipfile.ZipFile(runtime) as game, zipfile.ZipFile(sources) as source:
        names = set(game.namelist())
        assert not REMOVED & names
        assert not any(n.startswith(PREFIX) and n.endswith(('.tar.xz', '.py', '.c', '.h', '.zip')) for n in names)
        manifest = json.loads(game.read('ArcanumModern/manifest.json'))
        assert set(manifest['sha256']) == names-{'ArcanumModern/manifest.json'}
        for name, digest in manifest['sha256'].items():
            assert hashlib.sha256(game.read(name)).hexdigest() == digest, name
        assert REMOVED <= set(source.namelist())
        sums = dict(line.split('  ', 1)[::-1] for line in source.read('SHA256SUMS.txt').decode().splitlines())
        assert set(sums) == set(source.namelist())-{'SHA256SUMS.txt'}
        for name, digest in sums.items():
            assert hashlib.sha256(source.read(name)).hexdigest() == digest, name
            if Path(name).name in p.DEPENDENCY_SOURCE_HASHES:
                assert digest == p.DEPENDENCY_SOURCE_HASHES[Path(name).name]
        for component, license_file in (('ffmpeg', 'COPYING.LGPLv2.1'), ('7zip', 'LICENSE.txt')):
            name = PREFIX+component+'/'+license_file
            assert game.read(name) == source.read(name)
            notice = game.read(PREFIX+component+'/README.txt').decode()
            assert '/v'+manifest['version']+'/'+sources.name in notice
            assert 'source is included' not in notice and 'included here' not in notice
        assert 'Configure arguments' in source.read(PREFIX+'ffmpeg/README.txt').decode()
        assert game.testzip() is None and source.testzip() is None


class DependencySourceTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.output = Path(self.temp.name)/'game.zip'
        self.files = {name: name.encode() for name in REMOVED}
        self.files.update({
            'arcanum-modern.exe': b'unchanged executable',
            PREFIX+'ffmpeg/COPYING.LGPLv2.1': b'FFmpeg license',
            PREFIX+'7zip/LICENSE.txt': b'7-Zip license',
            PREFIX+'ffmpeg/README.txt': b'Exact source and the build script are included here.\nConfigure arguments:\n--enable-shared\n',
            PREFIX+'7zip/README.txt': b'Corresponding source is included.\n',
        })
        hashes = {Path(n).name: hashlib.sha256(data).hexdigest()
                  for n, data in self.files.items() if n.endswith('.tar.xz')}
        self.patch = patch.dict(p.DEPENDENCY_SOURCE_HASHES, hashes)
        self.patch.start()
        self.addCleanup(self.patch.stop)

    def test_matching_sources_separate_from_runtime_on_all_platforms(self):
        original = dict(self.files)
        for target in ('windows-x64', 'linux-x64', 'macos-arm64'):
            with self.subTest(target=target):
                files = dict(original)
                p.separate_dependency_sources(files, self.output, target)
                version = p.re.search(r'ARCANUM_RENAISSANCE_VERSION "([^"]+)"',
                                      (ROOT/'source/src/ui/modern_version.h').read_text()).group(1)
                p.write_archive(self.output, files, {'version': version})
                sources = self.output.parent/f'ArcanumModern-dependency-sources-{target}.zip'
                verify_pair(self.output, sources)
                for name, data in original.items():
                    if name not in REMOVED and not name.endswith('README.txt'):
                        self.assertEqual(files[name], data)
                with zipfile.ZipFile(sources) as z:
                    for name in REMOVED:
                        self.assertEqual(z.read(name), original[name])

    def test_invalid_source_preserves_payload_and_previous_archive(self):
        self.files[PREFIX+'7zip/7z2604-src.tar.xz'] = b'corrupt'
        before = dict(self.files)
        sources = self.output.parent/'ArcanumModern-dependency-sources-windows-x64.zip'
        sources.write_bytes(b'previous source package')
        with self.assertRaisesRegex(ValueError, 'checksum mismatch'):
            p.separate_dependency_sources(self.files, self.output, 'windows-x64')
        self.assertEqual(self.files, before)
        self.assertEqual(sources.read_bytes(), b'previous source package')

    def test_local_editor_sources_need_no_7zip(self):
        files = {n: data for n, data in self.files.items() if '/7zip/' not in n}
        p.separate_dependency_sources(files, self.output, 'worlded-windows', ('ffmpeg',))
        self.assertFalse(REMOVED & files.keys())
        self.assertIn(PREFIX+'ffmpeg/COPYING.LGPLv2.1', files)


if __name__ == '__main__':
    if len(sys.argv) == 3:
        verify_pair(Path(sys.argv[1]), Path(sys.argv[2]))
        print('PASS: runtime contains notices, sources delivered separately with matching hashes')
    else:
        unittest.main()
