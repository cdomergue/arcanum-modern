import importlib.util
import struct
import tempfile
import unittest
import zlib
from pathlib import Path

spec = importlib.util.spec_from_file_location('map_archive', Path(__file__).resolve().parents[1] / 'tools/map_archive.py')
archive = importlib.util.module_from_spec(spec)
spec.loader.exec_module(archive)


class MapArchiveTests(unittest.TestCase):
    def test_compressed_dat_is_read_only_and_preserves_case_insensitive_paths(self):
        content = b'{1}{Test map}'
        packed = zlib.compress(content)
        name = b'Rules\\TownMap.mes\0'
        entries = struct.pack('<I', 1) + struct.pack('<I', len(name)) + name
        entries += struct.pack('<IIIII', 0, 2, len(content), len(packed), 0)
        table = struct.pack('<I', len(packed) + 4) + entries
        raw = packed + table + b'DAT ' + struct.pack('<II', len(name), len(table) + 8)
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp) / 'test.dat'
            path.write_bytes(raw)
            reader = archive.Archive(path)
            self.assertEqual(reader.read('rules/townmap.mes'), content)
            self.assertEqual(path.read_bytes(), raw)

    def test_sector_with_lights_roofs_and_scripts_finds_townmap(self):
        raw = struct.pack('<I', 2) + bytes(2 * 48 + 4096 * 4)
        raw += struct.pack('<I', 0) + bytes(256 * 4)
        raw += struct.pack('<II', 0xAA0004, 3) + bytes(3 * 24) + bytes(12)
        raw += struct.pack('<I', 125)
        self.assertEqual(archive.sector_townmap(raw), 125)

    def test_old_sector_has_no_townmap(self):
        raw = bytes(4 + 4096 * 4) + struct.pack('<II', 1, 0xAA0000)
        self.assertEqual(archive.sector_townmap(raw), 0)


if __name__ == '__main__':
    unittest.main()
