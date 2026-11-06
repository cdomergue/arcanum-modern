#!/usr/bin/env python3
"""Validate the interactive tutorial content pack and zero-base-file invariants."""
import importlib.util
from pathlib import Path
import shutil
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class TutorialContentTests(unittest.TestCase):
    def setUp(self):
        spec = importlib.util.spec_from_file_location('tutorial_builder', ROOT / 'tools/tutorial/build-content.py')
        self.builder = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(self.builder)
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.work = Path(self.temp.name)

    def test_builds_complete_self_contained_pack(self):
        output = self.work / 'out'
        self.builder.build(output)
        base = output / 'base'
        self.assertEqual((base / 'tutorial/session-v1').read_bytes(), b'1')
        self.assertTrue((base / 'maps/Arcanum1-024-fixed/52747567503.sec').is_file())
        self.assertTrue((base / 'maps/Arcanum1-024-fixed/52747567504.sec').is_file())
        self.assertTrue((base / 'scr/48874_guide.scr').is_file())
        self.assertEqual(len(list((base / 'tutorial/mobs').glob('*.mob'))), 7)
        self.assertEqual(len(list((base / 'townmap/tutorial-island').glob('*.bmp'))), 32)
        self.assertEqual(len(list((base / 'sound/tutorial').glob('TUTORIAL_*.mp3'))), 36)
        self.assertEqual((base / 'sound/tutorial/TUTORIAL_PORTRAIT.mp3').read_bytes(),
                         (ROOT / 'resources/tutorial/sound/TUTORIAL_PORTRAIT.mp3').read_bytes())
        for line in ('1', '10'):
            voice = Path(f'speech/16106/v{line}_m.mp3')
            self.assertEqual((base / 'sound' / voice).read_bytes(),
                             (ROOT / 'resources/tutorial' / voice).read_bytes())
        for lang in ('fr', 'en'):
            dlg = (base / f'tutorial/{lang}/48874_guide.dlg').read_bytes().decode('cp1252')
            mes = (base / f'tutorial/{lang}/journal.mes').read_bytes().decode('cp1252')
            self.assertIn('{1}{', dlg)
            for line in ('1', '10'):
                self.assertIn(f'}}{{}}{{{line}}}{{0}}{{}}',
                              next(row for row in dlg.splitlines() if row.startswith(f'{{{line}}}')))
            for num in (1980, 1981, 1982, 1983, 2980, 2981, 2982, 2983):
                self.assertIn(f'{{{num}}}{{', mes)
        # Ensure zero extracted base-game MES files are shipped.
        for forbidden in ('gamequest.mes', 'townmap.mes', 'gamequestlog.mes', 'game_rd_npc_m2m.mes'):
            self.assertEqual(list(output.rglob(forbidden)), [])

    def test_rejects_missing_voice_clip_or_mob(self):
        res = self.work / 'res'
        shutil.copytree(ROOT / 'resources/tutorial', res)
        (res / 'sound/TUTORIAL_HELLO.mp3').unlink()
        with self.assertRaisesRegex(ValueError, 'TUTORIAL_HELLO'):
            self.builder.build(self.work / 'bad-voice', resources=res)

        shutil.copy2(ROOT / 'resources/tutorial/sound/TUTORIAL_HELLO.mp3', res / 'sound/TUTORIAL_HELLO.mp3')
        (res / 'mobs/g_9446d492810bd640a40425cbfac90f01.mob').unlink()
        with self.assertRaisesRegex(ValueError, 'mobs'):
            self.builder.build(self.work / 'bad-mob', resources=res)


if __name__ == '__main__':
    unittest.main()
