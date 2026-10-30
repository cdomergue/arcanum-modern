#!/usr/bin/env python3
"""Independent binary fixtures for the WorldED CLI; no commercial data required."""
import json
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import unittest
import zlib

EXECUTABLE = Path(sys.argv.pop(1)).resolve() if __name__ == '__main__' and len(sys.argv) > 1 else None


def u32(value):
    return struct.pack('<I', value)


def sector(version=4, roofs=True, objects=True):
    # Independent fixture writer, including nonzero data outside the tile layer.
    lights = u32(1) + bytes(range(48))
    tiles = b''.join(u32(0x10000000 + i) for i in range(4096))
    data = lights + tiles + u32(0 if roofs else 1)
    if roofs:
        data += b''.join(u32(0x50000000 + i) for i in range(256))
    data += u32(0xaa0000 + version)
    if version >= 1:
        data += u32(1) + bytes(range(24))
    if version >= 2:
        data += bytes(range(12))
    if version >= 3:
        data += struct.pack('<6I', 21, 2, 3, 4, 5, 6)
    if version >= 4:
        data += bytes(range(256)) * 2
    # Deliberately opaque, not a valid game object: the inspector must say so.
    data += (u32(119) + bytes(range(96)) + u32(1)) if objects else u32(0)
    return data


def script(lines=2, capacity=3):
    return (u32(0x10) + u32(2) + b'Maison achet\xe9e\0'.ljust(40, b'\0')
            + struct.pack('<4I', 7, lines, capacity, 0xdeadbeef)
            + bytes(range(132)) * lines)


def game_object(array=False):
    # v119 scenery instance: BASE uses two bitmap words, SCENERY one.
    proto = struct.pack('<H6xI12x', 1, 123)
    oid = struct.pack('<H6x16s', 2, bytes(range(16)))
    mask = 7 | (1 << 18) | ((1 << 31) if array else 0)
    data = u32(119) + proto + oid + u32(3) + struct.pack('<H3I', 5 if array else 4, mask, 0, 0)
    data += u32(0x40080000) + b'\1' + struct.pack('<Q', (7 << 32) | 12) + u32(3) + u32(0)
    if array:
        # Scripts at sparse attachment points 0 and 9, two 12-byte records.
        data += b'\1' + struct.pack('<3I', 12, 2, 0) + bytes(range(24)) + u32(1) + u32(1 | (1 << 9))
    return data


def art_file(rotations=1, frames=2, rle=True):
    header = struct.pack('<9I', 1 if rotations == 1 else 0, 12, 8, 1, 1, 0, 0, 0, frames) + bytes(96)
    palette = b''.join(u32(i * 0x010203) for i in range(256))
    palette2 = b''.join(u32(0xff0000 if i == 1 else 0x0000ff) for i in range(256))
    metadata, pixels = bytearray(), bytearray()
    for _ in range(rotations):
        for frame_index in range(frames):
            encoded = bytes([0x83, 1, 0, 2, 5, 3]) if rle else bytes([1, 0, 2, 3, 3, 3, 3, 3])
            metadata += struct.pack('<3I4i', 4, 2, len(encoded), -2, 7, frame_index, 0)
            pixels += encoded
    return header + palette + palette2 + metadata + pixels


def prototype(number, kind):
    # Independent v119 base schema: non-scalar fields have a one-byte null marker.
    nullable = {1, 5, 6, 7, 15, 16, 17, 30, 31, 34, 35}
    data = u32(119) + struct.pack('<H22x', 65535) + struct.pack('<H6xI12x', 1, number)
    data += u32(kind) + bytes(12)
    for index in range(36):
        value = 0x4000 if index == 18 else 0
        data += b'\0' if index in nullable else u32(value)
    tail = {0: [4,4,4,1,1], 1: [4,4,4,4,4,4,1,1], 2: [4,4,4,4,1,4,4,4,4,1,1], 3: [4,1,4,4,1,1]}[kind]
    return data + bytes(sum(tail))


def building_prototypes():
    return [(f'proto/{number:06d}.pro', prototype(number, kind), False)
            for number, kind in [(1020,0),(2028,1),(3044,2)]]


def archive(entries, dat1=True, prefix=b'', name_padding=b''):
    """entries = [(legacy_name, bytes, compressed)]; support prefixed DATs."""
    payload = bytearray()
    records = bytearray()
    names_size = 0
    for name, data, compressed in entries:
        name = name.encode('cp1252') + b'\0' + name_padding
        names_size += len(name)
        stored = zlib.compress(data) if compressed else data
        records += u32(len(name)) + name
        records += struct.pack('<5I', 0, 2 if compressed else 1, len(data), len(stored), len(payload))
        payload += stored
    footer = (bytes(range(16)) if dat1 else b'') + (b'1TAD' if dat1 else b' TAD') + u32(names_size)
    table = u32(len(payload) + 4) + u32(len(entries)) + records
    return prefix + payload + table + footer + u32(len(table) + len(footer))


class WorldEdTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='worlded-tests-')
        self.root = Path(self.temp.name)

    def tearDown(self):
        self.temp.cleanup()

    def run_cli(self, *args, ok=True):
        result = subprocess.run([str(EXECUTABLE), *map(str, args)], capture_output=True,
                                encoding='utf-8', errors='strict')
        if ok:
            self.assertEqual(result.returncode, 0, result.stderr)
        else:
            self.assertNotEqual(result.returncode, 0)
        return result

    def file(self, name, data):
        path = self.root / name
        path.write_bytes(data)
        return path

    def project(self, name='projet épreuve'):
        path = self.root / name
        self.run_cli('init', path)
        return path

    def test_sector_versions_roundtrip_and_single_tile_edit(self):
        for version in range(5):
            for roofs in (True, False):
                with self.subTest(version=version, roofs=roofs):
                    original = sector(version, roofs)
                    source = self.file(f'{version}-{roofs}.sec', original)
                    info = json.loads(self.run_cli('inspect', source).stdout)
                    self.assertTrue(info['lossless'])
                    self.assertEqual(info['roofs'], roofs)
                    self.assertEqual(info['townmap'], 21 if version >= 3 else 0)
                    self.assertEqual(info['objects'], 1)
                    self.assertFalse(info['object_payload_validated'])
                    project = self.project(f'roundtrip-{version}-{roofs}')
                    output = json.loads(self.run_cli('roundtrip', source, project).stdout)['output']
                    self.assertEqual(Path(output).read_bytes(), original)
                    edited = self.project(f'edit-{version}-{roofs}')
                    output = json.loads(self.run_cli('tile', source, edited, 63, 34, '0xaabbccdd').stdout)['output']
                    expected = bytearray(original)
                    struct.pack_into('<I', expected, 52 + 4 * (63 + 34 * 64), 0xaabbccdd)
                    self.assertEqual(Path(output).read_bytes(), expected)
                    self.assertEqual(source.read_bytes(), original)

    def test_bad_sectors_and_coordinates(self):
        good = sector()
        cases = [b'', good[:4], good[:16436], good[:17500], good[:-4]]
        bad_version = bytearray(good)
        struct.pack_into('<I', bad_version, 52 + 16384 + 4 + 1024, 0xaa0005)
        cases += [bad_version, u32(0xffffffff) + good[4:], good[:-4] + u32(0xffffffff)]
        for i, data in enumerate(cases):
            self.run_cli('inspect', self.file(f'bad{i}.sec', data), ok=False)
        source = self.file('good.sec', good)
        project = self.project()
        for x, y, art in [('64', '0', '1'), ('0', '64', '1'), ('-1', '0', '1'), ('0', '0', '4294967296')]:
            self.run_cli('tile', source, project, x, y, art, ok=False)
        self.assertFalse((project / 'exports').exists())

    def test_dat_plain_compressed_and_prefixed(self):
        for dat1 in (True, False):
            for prefix in (b'', b'embedded-database-prefix'):
                entries = [('Maps\\Test\\42.sec', sector(), True),
                           ('mes/épreuve.mes', b'// notes\r\n{1}{Propri\xe9t\xe9}\r\n{1}{Deux}\r\n', False),
                           ('empty.bin', b'', True)]
                path = self.file('fixture.dat', archive(entries, dat1, prefix))
                listing = [json.loads(line) for line in self.run_cli('list', path).stdout.splitlines()]
                self.assertEqual(len(listing), 3)
                info = json.loads(self.run_cli('inspect', path, 'MAPS/TEST/42.SEC').stdout)
                self.assertTrue(info['lossless'])
                messages = json.loads(self.run_cli('inspect', path, 'mes/épreuve.mes').stdout)['entries']
                self.assertEqual(messages, [{'id': 1, 'text': 'Propriété'}, {'id': 1, 'text': 'Deux'}])
                project = self.project(f'project-{dat1}-{len(prefix)}')
                for name, original, _ in entries:
                    result = json.loads(self.run_cli('import', path, name, project).stdout)
                    self.assertEqual(Path(result['output']).read_bytes(), original)

    def test_padded_archive_names(self):
        path = self.file('padded.dat', archive([('mes/example.mes', b'{1}{ok}', False)], name_padding=b'\x18\0\xffgarbage'))
        result = json.loads(self.run_cli('inspect', path, 'mes/example.mes').stdout)
        self.assertEqual(result['entries'], [{'id': 1, 'text': 'ok'}])

    def test_managed_overlays_require_activation_and_follow_runtime_order(self):
        game = self.root / 'managed'; game.mkdir()
        def dat(path, value):
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(archive([('mes/x.mes', value, False)]))
        dat(game/'arcanum1.dat', b'base')
        dat(game/'modules/Arcanum.dat', b'module')
        active = game/'modern-ui/uap-fr/active'
        for name in ['arcanum5.dat','arcanumxambient.dat','arcanumxanims1base.dat','arcanumxanims2dupes.dat','arcanumxanims3uap.dat','arcanumxraces.dat']:
            dat(active/'global'/name, b'global')
        for name in ['french-global.dat','modules/arcanum.patch1','modules/arcanum.patch9','french-module.dat']:
            dat(active/name, name.encode())
        (active/'data').mkdir()
        command = [str(EXECUTABLE), 'read', str(game), 'mes/x.mes']
        self.assertEqual(subprocess.check_output(command), b'module')
        (active/'ready').write_bytes(b'2.0.2-fr-1\n')
        self.assertEqual(subprocess.check_output(command), b'french-module.dat')
        for name in ['Arcanum8.dat','Arcanum.PATCH8','french-module.dat']:
            dat(active/'hotfix'/name, ('hotfix-'+name).encode())
        (active/'hotfix/ready').write_bytes(b'wrong-version\n')
        self.assertEqual(subprocess.check_output(command), b'french-module.dat')
        (active/'hotfix/ready').write_bytes(b'1.1.0-fr-1\n')
        self.assertEqual(subprocess.check_output(command), b'hotfix-french-module.dat')
        loose = game/'modules/Arcanum/mes/x.mes';loose.parent.mkdir(parents=True);loose.write_bytes(b'project')
        self.assertEqual(subprocess.check_output(command), b'project')

    def test_bad_archives(self):
        good = archive([('test.sec', sector(), True)])
        cases = [b'', good[:-1], good[:-4] + u32(0xffffffff), good[:-12] + b'NOPE' + good[-8:]]
        corrupt = bytearray(good)
        corrupt[3] ^= 0xff
        cases.append(corrupt)
        for i, data in enumerate(cases):
            path = self.file(f'bad{i}.dat', data)
            self.run_cli('inspect', path, 'test.sec', ok=False)
        duplicate = self.file('duplicate.dat', archive([('x', b'x', False), ('X', b'y', False)]))
        self.run_cli('list', duplicate, ok=False)

    def test_archive_out_of_bounds_and_expansion_limit(self):
        data = archive([('x', b'abc', False)])
        index = len(data) - 4 - struct.unpack_from('<I', data, len(data) - 4)[0]
        # Index 8-byte header, name length + "x\0", pointer then flags/size/packed/offset.
        for field_offset, value in [(index + 30, 0xffffffff), (index + 22, 0xffffffff)]:
            bad = bytearray(data)
            struct.pack_into('<I', bad, field_offset, value)
            self.run_cli('list', self.file('bad.dat', bad), ok=False)
        data = bytearray(archive([('x', b'abc', True)]))
        index = len(data) - 4 - struct.unpack_from('<I', data, len(data) - 4)[0]
        struct.pack_into('<I', data, index + 22, 64 * 1024 * 1024 + 1)
        self.run_cli('import', self.file('large.dat', data), 'x', self.project(), ok=False)

    def test_safe_exports_and_no_overwrite(self):
        for name in ('../escape', '/absolute', 'a/../../escape', 'C:\\escape', 'a//b', 'a/CON.txt', 'a/nul', 'a/x.', 'a/x '):
            path = self.file('unsafe.dat', archive([(name, b'x', False)]))
            self.run_cli('list', path, ok=False)
        source = self.file('good.sec', sector())
        project = self.project()
        output = json.loads(self.run_cli('roundtrip', source, project).stdout)['output']
        self.run_cli('roundtrip', source, project, ok=False)
        self.assertEqual(Path(output).read_bytes(), source.read_bytes())
        self.run_cli('init', project, ok=False)
        self.run_cli('roundtrip', source, self.root, ok=False)
        game = self.root / 'fake-game'
        game.mkdir()
        (game / 'arcanum1.dat').write_bytes(b'')
        self.run_cli('init', game / 'project', ok=False)
        self.assertFalse((game / 'project').exists())

    def test_symlink_export_refused(self):
        project = self.project()
        outside = self.root / 'outside'
        outside.mkdir()
        try:
            (project / 'exports').symlink_to(outside, target_is_directory=True)
        except OSError:
            self.skipTest('Symlink creation not available for this user')
        source = self.file('good.sec', sector())
        self.run_cli('roundtrip', source, project, ok=False)
        self.assertEqual(list(outside.iterdir()), [])

    def test_script_layout(self):
        info = json.loads(self.run_cli('inspect', self.file('good.scr', script())).stdout)
        self.assertEqual(info['description'], 'Maison achetée')
        self.assertEqual(info['lines'], 2)
        self.assertFalse(info['instructions_validated'])
        for i, data in enumerate([script()[:-1], script(2, 1), script() + b'x', b'']):
            self.run_cli('inspect', self.file(f'bad{i}.scr', data), ok=False)

    def test_mes_brace_scanning_and_invalid_fields(self):
        info = json.loads(self.run_cli('inspect', self.file('test.mes', b'// {1}{a}\r\n{2}{b\nc}')).stdout)
        self.assertEqual(info['entries'], [{'id': 1, 'text': 'a'}, {'id': 2, 'text': 'b\nc'}])
        for data in (b'{1}', b'{1}{x', b'{x}{text}', b'{4294967296}{x}', b'{1}{x\0}'):
            self.run_cli('inspect', self.file('bad.mes', data), ok=False)

    def test_audit_summary_and_failures(self):
        path = self.file('good.dat', archive([('a.sec', sector(), True), ('b.scr', script(), True)]))
        info = json.loads(self.run_cli('audit', path).stdout)
        self.assertEqual((info['sectors'], info['scripts'], info['objects_preserved'], info['failures']), (1, 1, 1, 0))
        self.run_cli('audit', path, 'missing', ok=False)
        path = self.file('bad.dat', archive([('a.sec', sector(), False), ('b.sec', b'bad', False)]))
        info = json.loads(self.run_cli('audit', path, ok=False).stdout)
        self.assertEqual((info['sectors'], info['failures']), (1, 1))

    def test_asset_audit_including_empty_sector(self):
        path = self.file('empty.dat', archive([('maps/test/0.sec', sector(objects=False), False)]))
        info = json.loads(self.run_cli('audit-assets', path).stdout)
        self.assertEqual(info['objects'], 0)
        self.assertEqual(info['failures'], 0)
        self.run_cli('audit-assets', path, 'missing', ok=False)
        bad = self.file('broken.dat', archive([('art/broken.art', b'bad', False)]))
        self.assertEqual(json.loads(self.run_cli('audit-assets', bad, ok=False).stdout)['failures'], 1)

    def test_object_fields_and_sparse_arrays(self):
        path = self.file('item.mob', game_object(True))
        result = json.loads(self.run_cli('decode', path).stdout)['objects']
        self.assertEqual(len(result), 1)
        obj = result[0]
        self.assertEqual((obj['type'], obj['prototype_id'], obj['size']), (3, 123, len(path.read_bytes())))
        fields = {field['name']: field for field in obj['fields']}
        self.assertEqual(fields['LOCATION']['value'], str((7 << 32) | 12))
        self.assertEqual(fields['SCRIPTS_IDX']['value'], '2 elements')
        payload = sector(objects=False)[:-4] + game_object() + game_object(True) + u32(2)
        result = json.loads(self.run_cli('decode', self.file('valid.sec', payload)).stdout)
        self.assertEqual(len(result['objects']), 2)

    def test_bad_object_streams(self):
        original = game_object(True)
        wrong_count = bytearray(original); struct.pack_into('<H', wrong_count, 56, 3)
        wrong_mask = bytearray(original); struct.pack_into('<I', wrong_mask, 66, 0x80000000)
        bad_sparse = original[:-4] + u32(1)
        cases = [original[:-1], original[:56], u32(120)+original[4:], wrong_count, wrong_mask, bad_sparse]
        for i, data in enumerate(cases):
            self.run_cli('decode', self.file(f'bad{i}.mob', data), ok=False)

    def test_art_pixels_rotations_palettes_and_rle(self):
        indices = [1, 0, 2, 3, 3, 3, 3, 3]
        expected = bytes(channel for i in indices for channel in (i, 2*i, 3*i, 255 if i else 0))
        for rotations in (1, 8):
            for compressed in (True, False):
                path = self.file('sprite.art', art_file(rotations, rle=compressed))
                info = json.loads(self.run_cli('decode', path).stdout)
                self.assertEqual((info['rotations'], info['frames'], info['fps']), (rotations, 2, 12))
                self.assertEqual(info['frame_data'][0]['hot_x'], -2)
                pixels = subprocess.check_output([str(EXECUTABLE), 'pixels', str(path), str(rotations-1), '1', '0'])
                self.assertEqual(pixels, expected)
                second = subprocess.check_output([str(EXECUTABLE), 'pixels', str(path), '0', '0', '1'])
                self.assertEqual(second[:8], bytes([255, 0, 0, 255, 0, 0, 255, 0]))
                self.run_cli('pixels', path, 8, 0, 0, ok=False)
                self.run_cli('pixels', path, 0, 0, 2, ok=False)

    def test_bad_art_frames(self):
        original = art_file(frames=1)
        bad_bpp = bytearray(original); struct.pack_into('<I', bad_bpp, 8, 32)
        huge = bytearray(original); struct.pack_into('<I', huge, 132+2048, 0xffffffff)
        cases = [original[:100], original[:-1], bad_bpp, huge]
        for i, data in enumerate(cases):
            self.run_cli('decode', self.file(f'bad{i}.art', data), ok=False)
        overflow = original[:-2] + bytes([127, 1])
        self.run_cli('pixels', self.file('bad-rle.art', overflow), 0, 0, 0, ok=False)
        # Real files contain unused suffixes, also ignored by the CE loader.
        self.run_cli('decode', self.file('trailing.art', original + b'old unused data'))

    def test_building_export_and_validation(self):
        game = self.root / 'game'; game.mkdir()
        (game / 'arcanum2.dat').write_bytes(archive(building_prototypes()))
        project = self.root / 'project'
        output = json.loads(self.run_cli('house', game, project, 12, 10, 4).stdout)
        path = Path(output['output']); data = path.read_bytes()
        objects = json.loads(self.run_cli('decode', path).stdout)['objects']
        self.assertEqual(len(objects),45)
        mobiles=list(path.parent.glob('*.mob')); self.assertEqual(len(mobiles),4)
        for mobile in mobiles:
            objects += json.loads(self.run_cli('decode',mobile).stdout)['objects']
        self.assertEqual(len(objects), 49)
        self.assertEqual(len({o['id'] for o in objects}), 49)
        self.assertEqual([o['type'] for o in objects].count(0), 44)
        self.assertEqual([o['type'] for o in objects].count(1), 1)
        self.assertEqual([o['type'] for o in objects].count(2), 4)
        for o in objects:
            values = {f['name']: f['value'] for f in o['fields']}
            loc = int(values['LOCATION'])
            self.assertLess(loc & 0xffffffff, 64); self.assertLess(loc >> 32, 64)
            self.assertEqual(values['SCRIPTS_IDX'], 'null')
            if o['type'] == 2:
                self.assertEqual(values['CONTAINER_INVENTORY_NUM'], '0')
                self.assertEqual(values['CONTAINER_INVENTORY_SOURCE'], '0')
                self.assertEqual(values['CONTAINER_INVENTORY_LIST_IDX'], 'null')
        self.assertEqual(struct.unpack_from('<I', data, 4+(24+24*64)*4)[0], (4<<22)|(4<<16))
        self.assertEqual((path.parent/'map.prp').read_bytes(), struct.pack('<iiqq',0,0,64,64))
        terrain=(path.parent/'terrain.tdf').read_bytes()
        self.assertEqual(len(terrain),34)
        self.assertEqual(struct.unpack_from('<IqqiiH',terrain,4),(0,1,1,0,0,60))
        self.assertEqual((path.parent/'map.jmp').read_bytes(), bytes(4))
        self.run_cli('house', game, project, ok=False)
        self.assertEqual(path.read_bytes(),data)
        self.run_cli('house',game,self.root/'bad-size',7,10,4,ok=False)
        self.assertFalse((self.root/'bad-size').exists())
        (game/'arcanum1.dat').write_bytes(archive([]))
        self.run_cli('house',game,game/'unsafe',ok=False)
        self.assertFalse((game/'unsafe').exists())
        missing=self.root/'missing'; missing.mkdir()
        (missing/'arcanum2.dat').write_bytes(archive(building_prototypes()[:2]))
        self.run_cli('house',missing,self.root/'missing-proto',ok=False)
        self.assertFalse((self.root/'missing-proto').exists())

    def test_installation_precedence_and_save_exclusion(self):
        game = self.root / 'installation'; game.mkdir()
        (game / 'arcanum1.dat').write_bytes(archive([('mes/x.mes', b'base', False)]))
        (game / 'data/mes').mkdir(parents=True); (game / 'data/mes/x.mes').write_bytes(b'global loose')
        (game / 'Modules').mkdir()
        (game / 'Modules/Arcanum.dat').write_bytes(archive([('mes/x.mes', b'module', False)]))
        (game / 'Modules/Arcanum.patch2').write_bytes(archive([('mes/x.mes', b'patch2', False)]))
        (game / 'Modules/Arcanum.patch10').write_bytes(archive([('mes/x.mes', b'patch10', False)]))
        command = [str(EXECUTABLE), 'read', str(game), 'mes/x.mes']
        self.assertEqual(subprocess.check_output(command), b'patch10')
        (game / 'Modules/Arcanum/mes').mkdir(parents=True)
        (game / 'Modules/Arcanum/mes/x.mes').write_bytes(b'loose module')
        (game / 'Modules/Arcanum/Save').mkdir()
        (game / 'Modules/Arcanum/Save/private.mes').write_bytes(b'private')
        self.assertEqual(subprocess.check_output(command), b'loose module')
        catalog = [json.loads(line) for line in self.run_cli('catalog', game).stdout.splitlines()]
        self.assertEqual(len(catalog), 1)
        self.assertEqual(catalog[0]['sources'], 6)


if __name__ == '__main__':
    if EXECUTABLE is None:
        raise SystemExit('Usage: python3 tests/test_worlded.py PATH_TO_WORLDED')
    unittest.main()
