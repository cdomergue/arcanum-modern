import importlib.util
import hashlib
import json
import os
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import unittest
import zipfile

ROOT = Path(__file__).resolve().parents[1]


class LinuxPackageTests(unittest.TestCase):
    def setUp(self):
        script = ROOT/'tools/package-linux.py'
        self.assertTrue(script.exists(), 'Linux package generator is missing')
        spec = importlib.util.spec_from_file_location('linux_package', script)
        self.p = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(self.p)

    def test_rejects_wrong_architecture_and_non_executable_elf(self):
        valid = b'\x7fELF\x02\x01\x01'+bytes(9)+struct.pack('<HHI', 3, 62, 1)+bytes(40)
        self.p.validate_elf(valid)
        for data in (b'MZ'+bytes(100), b'', valid[:16]+struct.pack('<HH', 3, 183)+valid[20:],
                     valid[:16]+struct.pack('<HH', 1, 62)+valid[20:]):
            with self.subTest(data=data[:20]), self.assertRaises(ValueError):
                self.p.validate_elf(data)

    def test_requires_only_system_libraries(self):
        valid = '0x1 (NEEDED) Shared library: [libm.so.6]\n0x1 (NEEDED) Shared library: [libc.so.6]\n'
        self.assertEqual(self.p.validate_dependencies(valid), ['libc.so.6', 'libm.so.6'])
        for extra in ('0x1 (NEEDED) Shared library: [libSDL3.so.0]',
                      '0x1 (NEEDED) Shared library: [libz.so.1]',
                      '0x1 (RUNPATH) Library runpath: [/home/builder/lib]'):
            with self.subTest(extra=extra), self.assertRaises(ValueError):
                self.p.validate_dependencies(valid+extra)

    def test_launcher_uses_game_folder_and_forwards_arguments(self):
        with tempfile.TemporaryDirectory(prefix='arcanum linux ') as temp:
            game = Path(temp)
            launcher = game/'Play Arcanum Modern.sh'
            launcher.write_bytes((ROOT/'tools/linux/Play Arcanum Modern.sh').read_bytes())
            result = subprocess.run(['sh', str(launcher)], capture_output=True, text=True)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn('arcanum1.dat', result.stderr)
            (game/'arcanum1.dat').write_bytes(b'fixture')
            (game/'modern-ui').mkdir()
            (game/'modern-ui/Book.ttf').write_bytes(b'fixture')
            engine = game/'arcanum-modern'
            engine.write_text('#!/bin/sh\npwd\nprintf "%s\\n" "$ARCANUM_IGNORE_DISPLAY_SETTINGS" "$@"\n')
            engine.chmod(0o755)
            subprocess.run(['sh', str(launcher), '-window', '-geometry=1280x720', 'argument with spaces'], check=True)
            lines = (game/'modern-launch.log').read_text().splitlines()
            self.assertEqual(Path(lines[0]).resolve(), game.resolve())
            self.assertEqual(lines[1:], ['1', '-window', '-geometry=1280x720', 'argument with spaces'])

    def test_bundled_decoder_search_paths(self):
        self.p.validate_dependencies('0x1 (NEEDED) Shared library: [libavcodec.so.62]\n'
                                     '0x1 (RUNPATH) Library runpath: [$ORIGIN/ArcanumModern/lib]')
        self.p.validate_dependencies('0x1 (RUNPATH) Library runpath: [$ORIGIN]', '$ORIGIN')
        with self.assertRaises(ValueError):
            self.p.validate_dependencies('0x1 (RUNPATH) Library runpath: [$ORIGIN:/tmp/libraries]')

    def test_launcher_crash_recovery(self):
        for script in ('tools/linux/Play Arcanum Modern.sh', 'play.sh'):
            for mode, renderer, attempts, status in (
                ('crash', '', 2, 0), ('ready', '', 1, 139),
                ('error', '', 1, 24), ('ok', '', 1, 0),
                ('crash', 'vulkan', 1, 139), ('ok', 'software', 1, 0),
                ('both_crash', '', 2, -11),
            ):
                with self.subTest(script=script, mode=mode, renderer=renderer), tempfile.TemporaryDirectory() as temp:
                    root = Path(temp)
                    game = root/'game'
                    game.mkdir()
                    (game/'modern-ui').mkdir()
                    (game/'modern-ui/Book.ttf').touch()
                    (game/'arcanum1.dat').touch()
                    launcher = (root if script == 'play.sh' else game)/'launch.sh'
                    launcher.write_bytes((ROOT/script).read_bytes())
                    engine = game/'arcanum-modern'
                    engine.write_text('''#!/bin/sh
ulimit -c 0
printf '%s|%s|%s|%s\\n' "$SDL_RENDER_DRIVER" "$SDL_VIDEO_DRIVER" "$SDL_FRAMEBUFFER_ACCELERATION" "$*" >> attempts
echo "renderer=$SDL_RENDER_DRIVER"
if [ "$SDL_RENDER_DRIVER" = software ] && [ "$TEST_MODE" != both_crash ]; then exit 0; fi
case "$TEST_MODE" in
    ready) echo 'Arcanum Modern: startup complete'; kill -SEGV $$ ;;
    crash|both_crash) kill -SEGV $$ ;;
    error) exit 24 ;;
esac
''')
                    engine.chmod(0o755)
                    env = {k: v for k, v in os.environ.items() if not k.startswith('SDL_')}
                    env.update(TEST_MODE=mode)
                    if renderer:
                        env['SDL_RENDER_DRIVER'] = renderer
                    result = subprocess.run(['sh', str(launcher), '-window', 'argument with spaces'], env=env, capture_output=True)
                    self.assertEqual(result.returncode, status)
                    calls = (game/'attempts').read_text().splitlines()
                    self.assertEqual(len(calls), attempts)
                    if attempts == 2 or renderer == 'software':
                        self.assertEqual(calls[-1], 'software|x11|0|-window argument with spaces')
                    self.assertEqual((game/'modern-launch-vulkan.log').exists(), attempts == 2)
                    if attempts == 2:
                        self.assertIn('renderer=vulkan', (game/'modern-launch-vulkan.log').read_text())
                        self.assertIn('renderer=software', (game/'modern-launch.log').read_text())


def verify_archive(path):
    spec = importlib.util.spec_from_file_location('linux_package', ROOT/'tools/package-linux.py')
    p = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(p)
    with zipfile.ZipFile(path) as z:
        names = z.namelist()
        assert len(names) == len(set(names))
        metadata = json.loads(z.read('ArcanumModern/manifest.json'))
        assert metadata['target'] == 'linux-x86_64' and metadata['includes_game_data'] is False
        assert tuple(map(int, metadata['minimum_glibc'].split('.'))) <= (2, 35)
        required = {'arcanum-modern', 'Play Arcanum Modern.sh'} | {'modern-ui/'+s for s in p.resources.ASSETS}
        required |= {'ArcanumModern/lib/'+name for name in p.resources.MOVIE_LIBRARIES['linux']}
        assert required.issubset(names)
        for name in names:
            assert name in required or name.startswith('ArcanumModern/'), name
            assert not name.startswith('/') and '..' not in Path(name).parts
            assert not name.lower().endswith(('.dat', '.ini', '.cfg', '.sav', '.gsi', '.tfaf', '.tfai'))
        p.validate_elf(z.read('arcanum-modern'))
        assert set(metadata['sha256']) == set(names) - {'ArcanumModern/manifest.json'}
        for name, digest in metadata['sha256'].items():
            assert hashlib.sha256(z.read(name)).hexdigest() == digest, name
        for name in ('arcanum-modern', 'Play Arcanum Modern.sh'):
            assert (z.getinfo(name).external_attr >> 16) & 0o111 == 0o111, name
    print('PASS: Linux x64 package, glibc baseline, executable permissions, hashes and no game data')


if __name__ == '__main__':
    if len(sys.argv) == 2 and sys.argv[1].endswith('.zip'):
        verify_archive(Path(sys.argv[1]))
    else:
        unittest.main()
