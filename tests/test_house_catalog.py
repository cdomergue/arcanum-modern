"""Catalogue protocol checks, including future houses and native five-choice limit."""
import copy
import importlib.util
import json
from pathlib import Path
import re
import shutil
import struct
import tempfile
import unittest
ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('house',ROOT/'tools/house/build-content.py')
house=importlib.util.module_from_spec(spec);spec.loader.exec_module(house)

class Catalogue(unittest.TestCase):
    def setUp(self):self.catalog=json.loads((ROOT/'resources/house/catalog.json').read_text())
    def test_current_property(self):
        p=self.catalog['properties'][0]
        self.assertEqual(p['price'],10000);self.assertEqual(p['text']['fr']['standing'],'Bon standing')
        for lang in house.LANGUAGES:
            text=house.dialogue(self.catalog,lang)
            self.assertIn('10 000' if lang=='fr' else '10,000',text)
            self.assertIn('pièces' if lang=='fr' else 'coins',text)
            self.assertNotIn('\u00c3',text)
            self.assertIn('loup apprivoisé' if lang=='fr' else 'tame wolf',text)
    def test_future_catalogue_navigation(self):
        first=self.catalog['properties'][0];self.catalog['properties']=[]
        for n in range(8):
            p=copy.deepcopy(first);p.update(id=f'house{n}',slot=n,price=(n+1)*10000,ownership_global=1800+n*2,discovery_global=1801+n*2,key_id=62000+n,sector=[1200+n,1100])
            p['text']['en']['standing']=['Modest','Good standing','Luxury'][n%3];self.catalog['properties'].append(p)
        house.validate(self.catalog)
        for lang in house.LANGUAGES:
            text=house.dialogue(self.catalog,lang)
            rows=[re.findall(r'\{([^}]*)\}',l) for l in text.splitlines()]
            ids={int(r[0]):r for r in rows};self.assertEqual(len(ids),len(rows));choices=0
            for r in rows:
                self.assertEqual(len(r),7)
                if r[3]:
                    choices+=1;self.assertLessEqual(choices,5)
                    target=int(r[5])
                    if target>0:self.assertIn(target,ids);self.assertFalse(ids[target][3])
                    if target<0:self.assertIn((-target-100)//10,range(8));self.assertIn((-target-100)%10,(0,1,2))
                else:
                    choices=0;self.assertEqual(r[1],r[2]) # Female PCs must get the same information.
            self.catalog['properties'].reverse()
            self.assertEqual(text,house.dialogue(self.catalog,lang))
    def test_duplicate_save_ids_rejected(self):
        self.catalog['properties'].append(copy.deepcopy(self.catalog['properties'][0]))
        with self.assertRaises(ValueError):house.validate(self.catalog)
    def test_overlapping_globals_rejected(self):
        self.catalog['properties'][0]['discovery_global']=1989
        with self.assertRaises(ValueError):house.validate(self.catalog)
    def test_seller_name_is_content(self):
        self.catalog['seller']['name'] = 'Morgan Reed'
        for lang in house.LANGUAGES:
            self.assertIn('Morgan Reed', house.dialogue(self.catalog, lang))
            self.assertNotIn('Edwin Harrow', house.dialogue(self.catalog, lang))

    def test_scene_validation(self):
        with tempfile.TemporaryDirectory() as d:
            scenes = Path(d)
            scene = scenes / self.catalog['properties'][0]['scene']
            shutil.copytree(ROOT/'resources/house'/scene.name, scene)
            house.check_scenes(self.catalog, scenes)
            manifest = scene/'scene.mes'
            original = manifest.read_text()
            manifest.write_text(original + '\n{0}{1 1 1}\n')
            with self.assertRaisesRegex(ValueError, 'duplicate manifest'):
                house.check_scenes(self.catalog, scenes)
            manifest.write_text(original)
            self.catalog['properties'][0]['sector'] = [12, 34]
            with self.assertRaisesRegex(ValueError, 'sector does not match'):
                house.check_scenes(self.catalog, scenes)
            (scene/'roofs.mes').unlink()
            with self.assertRaisesRegex(ValueError, 'missing roofs'):
                house.check_scenes(self.catalog, scenes)

    def test_shared_project_resources(self):
        with tempfile.TemporaryDirectory() as d:
            root = Path(d)
            scene = root/'scenes'/self.catalog['properties'][0]['scene']
            shutil.copytree(ROOT/'resources/house'/scene.name, scene)
            scripts = scene/'resources/scr'
            scripts.mkdir(parents=True)
            data = house.placeholder_script()
            (scripts/'49000_custom.scr').write_bytes(data)
            house.build(self.catalog, root/'pack', root/'scenes')
            self.assertEqual((root/'pack/base/scr/49000_custom.scr').read_bytes(), data)
            (scripts/'48301_conflict.scr').write_bytes(data)
            with self.assertRaisesRegex(ValueError, 'estate service'):
                house.build(self.catalog, root/'invalid', root/'scenes')
            self.assertFalse((root/'invalid').exists())

    def test_duplicate_scene_guids_rejected(self):
        with tempfile.TemporaryDirectory() as d:
            scenes = Path(d)
            first = self.catalog['properties'][0]
            shutil.copytree(ROOT/'resources/house'/first['scene'], scenes/first['scene'])
            second = copy.deepcopy(first)
            second.update(id='another', slot=1, ownership_global=1800,
                          discovery_global=1801, key_id=62000, scene='another', sector=[12,34])
            self.catalog['properties'].append(second)
            shutil.copytree(scenes/first['scene'], scenes/'another')
            manifest = scenes/'another/scene.mes'
            manifest.write_text(manifest.read_text().replace('{0}{1 1346 1024}', '{0}{1 12 34}'))
            with self.assertRaisesRegex(ValueError, 'Duplicate scene object GUID'):
                house.build(self.catalog, scenes/'invalid', scenes)

    def test_native_headers_and_both_languages(self):
        with tempfile.TemporaryDirectory() as d:
            root=Path(d);house.build(self.catalog,root)
            for script in (root/'base/scr').glob('*.scr'):
                b=script.read_bytes();self.assertEqual(struct.unpack_from('<ii',b,52),(1,1));self.assertEqual(len(b),196)
            for lang in house.LANGUAGES:
                self.assertTrue((root/lang/'dlg/48301_estate_agent.dlg').read_bytes().decode('cp1252'))
                self.assertIn('{61003}',(root/lang/'house/descriptions.mes').read_bytes().decode('cp1252'))

if __name__=='__main__':unittest.main()
