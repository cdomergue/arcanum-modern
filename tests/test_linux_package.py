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

    def test_sdl_dlopen_names_do_not_require_development_packages(self):
        spec = importlib.util.spec_from_file_location('sdl_runtime', ROOT/'tools/check-sdl-runtime.py')
        checker = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(checker)
        with tempfile.TemporaryDirectory() as temp:
            config = Path(temp)/'_deps/sdl3-build/build_config/SDL_build_config.h'
            config.parent.mkdir(parents=True)
            config.write_text('#define SDL_VIDEO_DRIVER_X11_DYNAMIC "libX11.so.6"\n'
                              '#define SDL_AUDIO_DRIVER_ALSA_DYNAMIC "libasound.so.2"\n')
            checker.validate(temp)
            for name in ('libX11.so', 'libasound.so'):
                config.write_text(f'#define SDL_VIDEO_DRIVER_X11_DYNAMIC "{name}"\n')
                with self.assertRaisesRegex(ValueError, 'development-only'):
                    checker.validate(temp)

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
            launcher = game/'Play Arcanum Renaissance.sh'
            launcher.write_bytes((ROOT/'tools/linux/Play Arcanum Renaissance.sh').read_bytes())
            result = subprocess.run(['sh', str(launcher)], capture_output=True, text=True)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn('arcanum1.dat', result.stderr)
            (game/'arcanum1.dat').write_bytes(b'fixture')
            (game/'modern-ui').mkdir()
            (game/'modern-ui/Book.ttf').write_bytes(b'fixture')
            engine = game/'renaissance-launcher'
            engine.write_text('#!/bin/sh\npwd\nprintf "%s\\n" "$ARCANUM_IGNORE_DISPLAY_SETTINGS" "$@"\n')
            engine.chmod(0o755)
            result = subprocess.run(['sh', str(launcher), '-window', '-geometry=1280x720', 'argument with spaces'], check=True, capture_output=True, text=True)
            lines = result.stdout.splitlines()
            self.assertEqual(Path(lines[0]).resolve(), game.resolve())
            self.assertEqual(lines[1:], ['1', '-window', '-geometry=1280x720', 'argument with spaces'])

    def test_bundled_decoder_search_paths(self):
        self.p.validate_dependencies('0x1 (NEEDED) Shared library: [libavcodec.so.62]\n'
                                     '0x1 (RUNPATH) Library runpath: [$ORIGIN/ArcanumModern/lib]')
        self.p.validate_dependencies('0x1 (RUNPATH) Library runpath: [$ORIGIN]', '$ORIGIN')
        with self.assertRaises(ValueError):
            self.p.validate_dependencies('0x1 (RUNPATH) Library runpath: [$ORIGIN:/tmp/libraries]')

    def test_development_launcher_forwards_to_updater(self):
        with tempfile.TemporaryDirectory(prefix='renaissance dev ') as temp:
            root = Path(temp)
            game = root/'game'
            game.mkdir()
            launcher = root/'play.sh'
            launcher.write_bytes((ROOT/'play.sh').read_bytes())
            helper = game/'renaissance-launcher'
            helper.write_text('#!/bin/sh\npwd\nprintf "%s\\n" "$@"\n')
            helper.chmod(0o755)
            result = subprocess.run(['sh', str(launcher), '-window', 'argument with spaces'], capture_output=True, text=True, check=True)
            self.assertEqual(result.stdout.splitlines(), [str(game), '-window', 'argument with spaces'])


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
        required = {'arcanum-modern', 'renaissance-launcher', 'ArcanumModern/launcher/renaissance-launcher-ui', 'ArcanumModern/launcher/background.png', 'Play Arcanum Renaissance.sh'} | {'modern-ui/'+s for s in p.resources.ASSETS}
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
        for name in ('arcanum-modern', 'renaissance-launcher', 'ArcanumModern/launcher/renaissance-launcher-ui', 'Play Arcanum Renaissance.sh'):
            assert (z.getinfo(name).external_attr >> 16) & 0o111 == 0o111, name
    print('PASS: Linux x64 package, glibc baseline, executable permissions, hashes and no game data')


if __name__ == '__main__':
    if len(sys.argv) == 2 and sys.argv[1].endswith('.zip'):
        verify_archive(Path(sys.argv[1]))
    else:
        unittest.main()
