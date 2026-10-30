"""Export independent houses without commercial resources."""
import json
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import unittest
from test_worlded import archive, sector, game_object, u32

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

if __name__ == '__main__': unittest.main()
