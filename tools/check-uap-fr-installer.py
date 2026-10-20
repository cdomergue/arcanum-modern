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

ROOT = Path(__file__).resolve().parents[1]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--game', type=Path, required=True)
p.add_argument('--helper', type=Path, required=True)
p.add_argument('--installer', type=Path, required=True)
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
    save = game/'modules/arcanum/Save/Slot0001.tfaf'
    save.parent.mkdir(parents=True)
    save.write_bytes(b'Existing saved game fixture')
    def run(mode, expected, installer=None):
        args = [str(a.helper.resolve()), '--game', str(game), '--mode', mode]
        if installer: args += ['--installer', str(installer.resolve())]
        result = subprocess.run(args, capture_output=True, text=True)
        assert result.returncode == expected, (mode, result.returncode, result.stdout[-300:], result.stderr[-500:])
    run('probe', 20)
    run('decline', 0)
    run('probe', 0)
    assert not (game/'save-backups').exists()
    (game/'modern-ui/uap-fr/choice').unlink()
    run('install', 1, game/'missing-installer.exe')
    assert not (game/'modern-ui/uap-fr/active').exists()
    (game/'save-backups').write_text('Prevent backup directory creation')
    run('install', 1, a.installer)
    assert not (game/'modern-ui/uap-fr/active').exists()
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
        assert hashlib.sha256((game/name).read_bytes()).hexdigest() == expected['sha256']
    assert (game/'modern-ui/uap-fr/active/ready').read_text() == '2.0.2-fr-1\n'
print('PASS: detection, remembered refusal, failed installer, failed backup, successful activation, preserved originals and idempotence')
