import hashlib
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest
import zipfile

ROOT = Path(__file__).resolve().parents[1]


class SetupPayloadTests(unittest.TestCase):
    def setUp(self):
        script = ROOT/'tools/package-windows-setup.py'
        self.assertTrue(script.exists(), 'The Windows setup generator is missing')
        spec = importlib.util.spec_from_file_location('setup_packager', script)
        self.p = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(self.p)
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.work = Path(self.temp.name)
        self.files = {name: b'fixture' for name in self.p.REQUIRED_FILES}

    def archive(self, full=False, wrong_hash=False):
        path = self.work/'package.zip'
        hashes = {name: hashlib.sha256(data).hexdigest() for name, data in self.files.items()}
        if wrong_hash:
            hashes['arcanum-modern.exe'] = '0'*64
        with zipfile.ZipFile(path, 'w') as z:
            for name, data in self.files.items():
                z.writestr(name, data)
            z.writestr('ArcanumModern/manifest.json', json.dumps({
                'includes_game_data': full, 'target': 'windows-x86_64', 'sha256': hashes}))
        return path

    def test_extracts_only_verified_mod_payload(self):
        target = self.work/'payload'
        names = self.p.extract_payload(self.archive(), target)
        self.assertIn('arcanum-modern.exe', names)
        self.assertEqual((target/'modern-ui/Book.ttf').read_bytes(), b'fixture')
        self.assertEqual(set(names), set(self.files) | {'ArcanumModern/manifest.json'})

    def test_refuses_full_package(self):
        with self.assertRaisesRegex(ValueError, 'game data'):
            self.p.extract_payload(self.archive(full=True), self.work/'payload')

    def test_refuses_game_data_preferences_and_unsafe_paths(self):
        for name in ['arcanum1.dat', 'modern-ui/settings.ini', 'modules/Arcanum.dat',
                     '../escape', '/absolute', 'modern-ui\\Book.ttf',
                     'ArcanumModern/licenses/../../escape', 'ArcanumModern/licenses/$bad.txt']:
            with self.subTest(name=name):
                self.files[name] = b'excluded'
                with self.assertRaises(ValueError):
                    self.p.extract_payload(self.archive(), self.work/'payload')
                del self.files[name]
        self.assertFalse((self.work/'escape').exists())

    def test_refuses_corrupt_payload_before_extraction(self):
        with self.assertRaisesRegex(ValueError, 'hash'):
            self.p.extract_payload(self.archive(wrong_hash=True), self.work/'payload')
        self.assertFalse((self.work/'payload/arcanum-modern.exe').exists())

    def test_refuses_missing_assets(self):
        del self.files['modern-ui/Book.ttf']
        with self.assertRaisesRegex(ValueError, 'Missing'):
            self.p.extract_payload(self.archive(), self.work/'payload')

    def test_rejected_package_preserves_existing_setup(self):
        output = self.work/'setup.exe'
        output.write_bytes(b'previous setup')
        with self.assertRaisesRegex(ValueError, 'game data'):
            self.p.build(self.archive(full=True), output, 'unused-compiler')
        self.assertEqual(output.read_bytes(), b'previous setup')
        self.assertEqual(list(self.work.glob('arcanum-setup-*')), [])


if __name__ == '__main__':
    unittest.main()
