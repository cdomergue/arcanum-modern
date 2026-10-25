#!/usr/bin/env python3
"""Exercise UAP installation decisions and failures on an isolated game fixture."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import tempfile
import zipfile
import struct
import zlib

ROOT = Path(__file__).resolve().parents[1]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--game', type=Path, required=True)
p.add_argument('--helper', type=Path, required=True)
p.add_argument('--installer', type=Path, required=True)
p.add_argument('--english-fixture', action='store_true', help='Exercise the English path with synthetic English UI text over shared original art')
a = p.parse_args()
catalog = json.loads((ROOT/'tools/uap/data/catalog.json').read_text())
def find(root, name):
    for part in name.split('/'):
        root = next(p for p in root.iterdir() if p.name.lower() == part.lower())
    return root
with tempfile.TemporaryDirectory(prefix='uap-installer-', dir=ROOT/'build') as directory:
    game = Path(directory)
    for name in catalog['original']:
        target = game/name
        target.parent.mkdir(parents=True, exist_ok=True)
        os.link(find(a.game, name), target)
    language = 'en' if a.english_fixture else 'fr'
    overlay = game/f'modern-ui/uap-{language}/active'
    if a.english_fixture:
        # The proprietary English game is not distributed with this test. Replace
        # one archive in this isolated fixture with a minimal English MES record.
        name = b'mes\\intgame.mes\0'
        raw = b'{0}{Level}\n{1}{Reaction}\n'
        packed = zlib.compress(raw)
        table = struct.pack('<I',len(name))+name+struct.pack('<IIIII',0,2,len(raw),len(packed),0)
        archive = packed+struct.pack('<II',len(packed)+4,1)+table+b' TAD'+struct.pack('<II',len(name),len(table)+16)
        (game/'arcanum4.dat').unlink() # Never write through the original hard link.
        (game/'arcanum4.dat').write_bytes(archive)
    originals = {name: hashlib.sha256((game/name).read_bytes()).hexdigest() for name in catalog['original']}
    save = game/'modules/arcanum/Save/Slot0001.tfaf'
    save.parent.mkdir(parents=True)
    save.write_bytes(b'Existing saved game fixture')
    def run(mode, expected, installer=None):
        args = [str(a.helper.resolve()), '--game', str(game), '--mode', mode]
        if installer: args += ['--installer', str(installer.resolve())]
        result = subprocess.run(args, capture_output=True, text=True)
        assert result.returncode == expected, (mode, result.returncode, result.stdout[-300:], result.stderr[-500:])
    run('probe', 21 if a.english_fixture else 20)
    run('decline', 0)
    run('probe', 0)
    assert not (game/'save-backups').exists()
    (game/f'modern-ui/uap-{language}/choice').unlink()
    run('install', 1, game/'missing-installer.exe')
    assert not overlay.exists()
    (game/'save-backups').write_text('Prevent backup directory creation')
    run('install', 1, a.installer)
    assert not overlay.exists()
    assert save.read_bytes() == b'Existing saved game fixture'
    (game/'save-backups').unlink()
    run('install', 0, a.installer)
    run('probe', 0)
    backups = list((game/'save-backups').glob('*/saves.zip'))
    assert len(backups) == 1
    with zipfile.ZipFile(backups[0]) as z:
        assert z.read('modules/arcanum/Save/Slot0001.tfaf') == save.read_bytes()
        assert 'RESTORE.txt' in z.namelist()
    run('install', 1, a.installer)
    assert len(list((game/'save-backups').glob('*/saves.zip'))) == 1
    for name, expected in catalog['original'].items():
        assert hashlib.sha256((game/name).read_bytes()).hexdigest() == originals[name]
    assert (overlay/'ready').read_text() == f'2.0.2-{language}-1\n'
    assert (overlay/'french-global.dat').exists() == (language == 'fr')
    assert (overlay/'french-module.dat').exists() == (language == 'fr')
    for name, expected in catalog['patch'].items():
        target = overlay/('global/'+name if '/' not in name else name)
        assert hashlib.sha256(target.read_bytes()).hexdigest() == expected['sha256']
    # Both manual UAP installations and prepared overlays suppress reinstallation.
    (overlay/'ready').unlink()
    (game/'Arcanum5.dat').write_bytes(b'existing UAP installation')
    run('probe', 0)
print('PASS: detection, remembered refusal, failed installer, failed backup, successful activation, preserved originals and idempotence')
