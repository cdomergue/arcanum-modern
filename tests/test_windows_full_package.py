from pathlib import Path
import hashlib
import importlib.util
import json
import tempfile
import unittest
import zipfile

root = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('packager', root/'tools/package-windows.py')
p = importlib.util.module_from_spec(spec)
spec.loader.exec_module(p)


class FullPackageTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='arcanum-full-package-')
        self.addCleanup(self.temp.cleanup)
        self.work = Path(self.temp.name)
        self.game = self.work/'game'
        for name in p.REQUIRED_GAME_FILES:
            self.put(name, b'game data')

    def put(self, name, data=b'private'):
        path = self.game/name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)
        return path

    def test_contents_and_personal_exclusions(self):
        public = ['Arcanum4.dat', 'modules/Arcanum.PATCH9', 'data/Proto/item.pro',
                  'modules/Arcanum/maps/Tarant/map.prp', 'Proto/weapon.pro']
        private = ['arcanum.cfg', 'HighRes/config.ini', 'modern-ui/graphics.ini',
                   'modern-ui/notebook-person.txt', 'modules/Arcanum/Save/SlotAuto.tfaf',
                   'modules/Arcanum/TIGCache/maps/file.bin', 'data/Players/hero.txt',
                   'modules/Arcanum/screen0000.bmp', 'modules/Arcanum/.DS_Store',
                   'modules/Arcanum/Screenshots/shot.png', 'Arcanum Modern.app/Contents/file',
                   'arcanum.exe', 'modules/Arcanum/private.gsi']
        for name in public + private:
            self.put(name)
        files = p.game_files(self.game)
        self.assertEqual(set(files), p.REQUIRED_GAME_FILES | set(public))

    def test_hd_symlink_becomes_real_files(self):
        cache = self.work/'cache/Arcanum/21'
        cache.mkdir(parents=True)
        (cache/'complete').write_text('1 1024 1024 1024 4\n')
        tile = bytes(range(256))*12289
        (cache/'0_0_0.png').write_bytes(tile)
        (cache/'.DS_Store').write_bytes(b'private')
        (self.game/'modern-ui').mkdir()
        (self.game/'modern-ui/local-maps').symlink_to(self.work/'cache', target_is_directory=True)
        self.put('data/file.mes', b'public')
        (self.game/'data/private-link').symlink_to(self.put('secret.txt'))
        files = p.game_files(self.game)
        self.assertNotIn('data/private-link', files)
        archive = self.work/'full.zip'
        files['arcanum-modern.exe'] = b'fixture'
        p.write_archive(archive, files, {'includes_game_data': True})
        with zipfile.ZipFile(archive) as z:
            self.assertIsNone(z.testzip())
            name = 'modern-ui/local-maps/Arcanum/21/0_0_0.png'
            self.assertEqual(z.read(name), tile)
            self.assertEqual(z.getinfo(name).compress_type, zipfile.ZIP_STORED)
            metadata = json.loads(z.read('ArcanumModern/manifest.json'))
            for name, digest in metadata['sha256'].items():
                self.assertEqual(hashlib.sha256(z.read(name)).hexdigest(), digest)
            self.assertTrue(metadata['includes_game_data'])

    def test_incomplete_installation_rejected(self):
        (self.game/'arcanum2.dat').unlink()
        with self.assertRaisesRegex(ValueError, 'arcanum2.dat'):
            p.game_files(self.game)

    def test_failure_preserves_previous_archive(self):
        archive = self.work/'previous.zip'
        archive.write_bytes(b'previous release')
        with self.assertRaises(FileNotFoundError):
            p.write_archive(archive, {'missing': self.work/'missing'}, {})
        self.assertEqual(archive.read_bytes(), b'previous release')
        self.assertEqual(list(self.work.glob('*.zip')), [archive])


if __name__ == '__main__':
    unittest.main()
