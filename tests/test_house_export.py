"""Export independent houses without commercial resources."""
import json
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import unittest
from test_worlded import archive, sector, game_object, u32, prototype

EXE = Path(sys.argv.pop(1)).resolve()
MAP = 'arcanum1-024-fixed'

class Export(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='house-export-')
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.game = self.root/'game'; self.game.mkdir()

    def fixture(self, x, y, guard=False):
        sid = x | (y << 26)
        base = sector(version=0, objects=False)
        project = self.root/f'project-{x}-{y}'
        folder = project/'exports/maps'/MAP; folder.mkdir(parents=True)
        (project/'building.json').write_text(json.dumps(dict(format='worlded-scene', version=1, map=MAP, sector=sid, existing_map=True)))
        data = bytearray(game_object())
        location = (x*64+12) | ((y*64+7)<<32)
        data[75:83] = struct.pack('<Q', location)
        guid = (data[28:30]+data[36:52]).hex()
        (folder/f'{sid}.sec').write_bytes(base[:-4]+data+u32(1))
        guard_id = None
        if guard:
            identity = bytes(range(16,32))
            guard_id = '0200'+identity.hex()
            npc = u32(119)+struct.pack('<H6xI12x',1,17001)+struct.pack('<H6x16s',2,identity)+u32(16)
            npc += struct.pack('<H5I',3,1|2|(1<<18),0,0,0,0)+u32(0x28000000)+b'\1'+struct.pack('<Q',location)+u32(0)
            (folder/'guard.mob').write_bytes(npc)
        (self.game/'arcanum1.dat').write_bytes(archive([
            (f'maps/{MAP}/map.prp',struct.pack('<iiqq',0,0,128000,128000),False),
            (f'maps/{MAP}/{sid}.sec',base,False)]))
        return project, guid, guard_id

    def run_export(self, project, name, *options, success=True):
        output = self.root/name
        result = subprocess.run([str(EXE), str(self.game), str(project), str(output), *options],capture_output=True)
        self.assertEqual(result.returncode == 0, success, result.stderr)
        return output

    def test_two_sectors_roles_and_stable_ids(self):
        for x,y in [(31,27),(85,99)]:
            project, guid, guard = self.fixture(x,y,True)
            out = self.run_export(project,f'out-{x}','--guard',guard)
            text = (out/'scene.mes').read_text()
            self.assertIn(f'{{0}}{{1 {x} {y}}}',text)
            self.assertIn(f'guard {guard}',text)
            self.assertIn(f'object {guid}',text)
            other = self.run_export(project,f'again-{x}','--guard',guard)
            self.assertEqual({p.relative_to(out):p.read_bytes() for p in out.rglob('*') if p.is_file()},
                             {p.relative_to(other):p.read_bytes() for p in other.rglob('*') if p.is_file()})
            automatic = self.run_export(project,f'no-guard-{x}')
            self.assertIn(f'object {guard}',(automatic/'scene.mes').read_text())

    def test_resources_and_clearance(self):
        project,_,_ = self.fixture(7,9)
        (project/'exports/scr').mkdir(); (project/'exports/scr/49001_custom.scr').write_bytes(b'custom')
        (project/'vegetation.mes').write_bytes(b'{77}{62}\n')
        out = self.run_export(project,'resources')
        self.assertEqual((out/'resources/scr/49001_custom.scr').read_bytes(),b'custom')
        self.assertEqual((out/'vegetation.mes').read_bytes(),b'{77}{62}\n')

    def test_invalid_guard_and_existing_output(self):
        project,guid,_ = self.fixture(8,9)
        bad = self.run_export(project,'bad','--guard',guid,success=False)
        self.assertFalse(bad.exists())
        out = self.run_export(project,'good')
        before = (out/'scene.mes').read_bytes()
        self.run_export(project,'good',success=False)
        self.assertEqual((out/'scene.mes').read_bytes(),before)

    def test_tile_and_sector_script_headers(self):
        project,_,_ = self.fixture(8,9)
        file=next((project/'exports/maps'/MAP).glob('*.sec'))
        original=file.read_bytes();prefix=len(sector(version=0,objects=False))-8
        tile=struct.pack('<6I',0,1100,0x80000001,0xff030201,42,0)
        headers=u32(1)+tile+struct.pack('<3I',2,77,43)+bytes(24+512)
        file.write_bytes(original[:prefix]+u32(0xaa0004)+headers+original[prefix+4:])
        out=self.run_export(project,'scripts')
        self.assertEqual((out/'scripts.mes').read_text(),'{1100}{42 2147483649 4278387201}\n{4096}{43 2 77}\n')

    def test_sector_changes_export_as_additive_overlays(self):
        project,_,_ = self.fixture(8,9)
        file=next((project/'exports/maps'/MAP).glob('*.sec'))
        original=file.read_bytes()
        prefix=len(sector(version=0,objects=False))-8
        # Promote the independent v0 fixture with empty script/metadata/block headers.
        header=bytearray(4+12+24+512)
        for name,offset,value in [('blocking',40,1),('sound',32,100)]:
            changed=bytearray(header);struct.pack_into('<I',changed,offset,value)
            file.write_bytes(original[:prefix]+u32(0xaa0004)+changed+original[prefix+4:])
            out=self.run_export(project,name)
            target,expected=('blocking.mes','{0}{1}\n') if name=='blocking' else ('environment.mes','{2}{100}\n')
            self.assertEqual((out/target).read_text(),expected)

    def test_manual_inventory_preserves_ids_quantities_and_source_project(self):
        project, scenery_id, _ = self.fixture(8, 9)
        folder = project/'exports/maps'/MAP
        owner_raw = struct.pack('<H6x16s', 2, bytes(range(32,48)))
        item_raw = struct.pack('<H6x16s', 2, bytes(range(48,64)))
        owner_id = '0200'+bytes(range(32,48)).hex()
        item_id = '0200'+bytes(range(48,64)).hex()
        location = struct.pack('<Q', (8*64+12) | ((9*64+7)<<32))
        # Independent v119 bytes: BASE has two mask words; each child group one.
        owner = u32(119)+struct.pack('<H6xI12x',1,3044)+owner_raw+u32(2)
        owner += struct.pack('<H3I',5,3|(1<<18),0,(1<<3)|(1<<4))
        owner += u32(0x70000000)+b'\1'+location+u32(0)+u32(1)
        owner += b'\1'+struct.pack('<3I',24,1,0)+item_raw+u32(1)+u32(1)
        item = u32(119)+struct.pack('<H6xI12x',1,9076)+item_raw+u32(8)
        item += struct.pack('<H4I',6,3|(1<<18),0,(1<<1)|(1<<7),1<<1)
        item += u32(0x60000003)+b'\1'+location+u32(0x1000)+b'\1'+owner_raw+u32(0)+u32(54321)
        (folder/'chest.mob').write_bytes(owner)
        (folder/'coins.mob').write_bytes(item)
        before = {p.relative_to(project):p.read_bytes() for p in project.rglob('*') if p.is_file()}
        out = self.run_export(project,'inventory')
        self.assertEqual((out/'inventory.mes').read_text(),f'{{0}}{{{item_id} {owner_id}}}\n')
        self.assertIn(f'chest {owner_id}',(out/'scene.mes').read_text())
        self.assertIn(f'object {item_id}',(out/'scene.mes').read_text())
        # Housing adds OF_DYNAMIC only; the item keeps its owner and stack count.
        expected = bytearray(item)
        flags_offset = 58+4*4+4+1+8
        struct.pack_into('<I',expected,flags_offset,0x3000)
        self.assertEqual((out/'objects'/f'{item_id}.mob').read_bytes(),expected)
        again = self.run_export(project,'inventory-again')
        self.assertEqual((again/'inventory.mes').read_bytes(),(out/'inventory.mes').read_bytes())
        self.assertEqual({p.relative_to(project):p.read_bytes() for p in project.rglob('*') if p.is_file()},before)
        self.assertTrue((out/'objects'/f'{scenery_id}.mob').exists())

    def test_light_proxy_and_removal_preserve_native_sector(self):
        project, guid, _ = self.fixture(8, 9)
        sid = 8 | (9 << 26)
        file = next((project/'exports/maps'/MAP).glob('*.sec'))
        original = file.read_bytes()
        light = bytearray(48)
        struct.pack_into('<QiiII', light, 8, (8*64+12) | ((9*64+7)<<32), -6, 11, 4, 0x90000000)
        light[32:35] = bytes([210, 30, 90])
        # Replace the fixture's opaque light with a real independent light.
        base = sector(version=0, objects=False)
        old = bytearray(light); old[32] = 100
        base = base[:4] + old + base[52:]
        (self.game/'arcanum1.dat').write_bytes(archive([
            (f'maps/{MAP}/map.prp', struct.pack('<iiqq',0,0,128000,128000), False),
            (f'maps/{MAP}/{sid}.sec', base, False),
            ('proto/004001.pro', prototype(4001,3), False)]))
        file.write_bytes(original[:4] + light + original[52:])
        out = self.run_export(project, 'lights')
        self.assertEqual((out/'removed-lights.bin').read_bytes(), u32(1)+old)
        mobs = list((out/'objects').glob('*.mob'))
        self.assertEqual(len(mobs), 2)
        proxy = next(p for p in mobs if p.stem != guid)
        self.assertIn(f'object {proxy.stem}', (out/'scene.mes').read_text())
        again = self.run_export(project, 'lights-again')
        self.assertEqual(proxy.read_bytes(), (again/'objects'/proxy.name).read_bytes())
        self.assertEqual(file.read_bytes(), original[:4]+light+original[52:])

if __name__ == '__main__': unittest.main()
