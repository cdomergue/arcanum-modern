"""Portable editor archive and offline manual contracts."""
import hashlib
from html.parser import HTMLParser
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest
import zipfile

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('package_worlded', ROOT/'tools/package-worlded-windows.py')
package = importlib.util.module_from_spec(spec)
spec.loader.exec_module(package)

class ManualParser(HTMLParser):
    def __init__(self):
        super().__init__()
        self.ids, self.links, self.sections = [], [], 0
    def handle_starttag(self, tag, attributes):
        attrs = dict(attributes)
        if 'id' in attrs: self.ids.append(attrs['id'])
        if tag == 'a': self.links.append(attrs.get('href', ''))
        if tag == 'section': self.sections += 1
        if tag in ('script', 'img', 'link'):
            assert not attrs.get('src', attrs.get('href', '')).startswith(('http:', 'https:'))

class PackageTests(unittest.TestCase):
    def make_archive(self, path, omit=None, corrupt=False, extra=None):
        files = {name: b'fixture' for name in package.REQUIRED if name != omit}
        if extra: files[extra] = b'fixture'
        manifest = {'files': {n: hashlib.sha256(v).hexdigest() for n, v in files.items()}}
        if corrupt: files['worlded-tutorial.html'] = b'changed'
        with zipfile.ZipFile(path, 'w') as z:
            for name, data in files.items(): z.writestr(name, data)
            z.writestr('package-manifest.json', json.dumps(manifest))
    def test_archive_contract(self):
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary)/'test.zip'
            self.make_archive(path)
            package.verify_archive(path)
            for kwargs in ({'omit': 'worlded-tutorial.html'}, {'omit': 'vcruntime140.dll'},
                           {'corrupt': True}, {'extra': '../escape'},
                           {'extra': 'WORLDed.exe'}, {'extra': 'Slot0001.sav'}):
                with self.subTest(kwargs=kwargs):
                    self.make_archive(path, **kwargs)
                    with self.assertRaises(ValueError): package.verify_archive(path)
    def test_manual_links(self):
        parser = ManualParser()
        parser.feed((ROOT/'worlded-tutorial.html').read_text(encoding='utf-8'))
        self.assertEqual(parser.sections, 19)
        self.assertEqual(len(parser.ids), len(set(parser.ids)))
        for href in parser.links:
            if href.startswith('#'): self.assertIn(href[1:], parser.ids)

if __name__ == '__main__': unittest.main()
