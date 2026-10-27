#!/usr/bin/env python3
"""Check hotfix upgrades using private copies of legally installed game data."""
import argparse
import errno
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import shutil
import subprocess
import zipfile
import struct
import zlib
from map_archive import Archive

ROOT = Path(__file__).resolve().parents[1]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--game', type=Path, required=True)
p.add_argument('--helper', type=Path, required=True)
p.add_argument('--downloads', type=Path, required=True)
p.add_argument('--output', type=Path, required=True, help='New isolated fixture directory')
p.add_argument('--variant', nargs='+', choices=['fr','en','manual-fr','manual-en'], default=['fr','en','manual-fr','manual-en'])
a = p.parse_args()
a.output.mkdir(parents=True, exist_ok=False)
cat = json.loads((ROOT/'tools/uap/data/catalog.json').read_text())
hotfix = json.loads((ROOT/'tools/uap/data/hotfix-catalog.json').read_text())
spec = importlib.util.spec_from_file_location('plan', ROOT/'tools/uap/make-plan.py')
plan = importlib.util.module_from_spec(spec); spec.loader.exec_module(plan)

def find(root, name):
    for part in name.split('/'):
        root = next(x for x in root.iterdir() if x.name.lower() == part.lower())
    return root

def copy(source, target):
    target.parent.mkdir(parents=True, exist_ok=True)
    try: os.link(source, target)
    except OSError as e:
        if e.errno != errno.EXDEV: raise
        shutil.copy2(source, target)

def run(game, mode, expected):
    result = subprocess.run([str(a.helper.resolve()), '--game', str(game.resolve()), '--mode', mode,
        '--hotfix-downloads', str(a.downloads.resolve())], capture_output=True, text=True)
    assert result.returncode == expected, (mode, result.stdout[-1000:], result.stderr[-1000:])
    return result

for lang in a.variant:
    game = a.output/lang
    game.mkdir(); (game/'.hotfix-sandbox').touch()
    (game/'.hotfix-resources').write_text('\n'.join(hotfix['inputs'])+'\n')
    for name in cat['original']: copy(find(a.game, name), game/name)
    copy(find(a.game,'tig.dat'),game/'tig.dat')
    prepared = a.game/'modern-ui/uap-fr/active'
    manual = lang.startswith('manual')
    base = game if manual else game/f'modern-ui/uap-{lang}/active'
    base.mkdir(parents=True, exist_ok=True)
    for name in cat['patch']:
        source = prepared/('global/'+name if '/' not in name else name)
        target = base/(name if manual or '/' in name else 'global/'+name)
        copy(source,target)
    if not manual:
        (base/'data').mkdir(exist_ok=True)
        (base/'ready').write_text(f'2.0.2-{lang}-1\n')
        if lang == 'fr':
            for name in ['french-global.dat','french-module.dat']: copy(prepared/name,base/name)
    # Two case variants created by older engine versions, with different padding.
    for folder in ['Proto','proto']:
        source = find(a.game, 'data/'+folder+'/010114 - Food.pro')
        copy(source,game/'data'/folder/'010114 - Food.pro')
    language = lang.removeprefix('manual-')
    if lang == 'manual-en':
        # Language-detection fixture, not a redistributed English game archive.
        name = b'mes\\intgame.mes\0'; raw = b'{0}{Level}\n{1}{Reaction}\n'; packed = zlib.compress(raw)
        table = struct.pack('<I',len(name))+name+struct.pack('<IIIII',0,2,len(raw),len(packed),0)
        archive = packed+struct.pack('<II',len(packed)+4,1)+table+b' TAD'+struct.pack('<II',len(name),len(table)+16)
        (game/'arcanum4.dat').unlink() # Preserve the original hard link's target.
        (game/'arcanum4.dat').write_bytes(archive)
    originals = {name:hashlib.sha256((game/name).read_bytes()).hexdigest() for name in cat['original']}
    active = game/f'modern-ui/uap-hotfix/{language}/active' if manual else base/'hotfix'
    work = game/f'modern-ui/uap-hotfix/{language}'
    save = game/'modules/arcanum/Save/Slot0001.tfaf'
    save.parent.mkdir(parents=True); save.write_bytes(b'Existing campaign fixture')
    expected = 22 if language == 'fr' else 23
    run(game,'probe',expected)
    run(game,'decline-hotfix',0);run(game,'probe',0)
    (work/'choice').unlink()
    # A changed dialogue must block installation, without replacing base or saves.
    conflict = save.parents[1]/'dlg/02635sarahtoone.dlg'
    conflict.parent.mkdir();conflict.write_text('A third party dialogue mod')
    assert 'conflict' in run(game,'probe',0).stdout
    run(game,'install-hotfix',1);assert not active.exists()
    conflict.unlink()
    (game/'save-backups').write_text('Simulated backup failure')
    run(game,'install-hotfix',1);assert not active.exists()
    assert save.read_bytes() == b'Existing campaign fixture'
    (game/'save-backups').unlink()
    run(game,'install-hotfix',0);run(game,'probe',0);run(game,'install-hotfix',0)
    assert active.joinpath('ready').read_text() == f'1.1.0-{language}-1\n'
    backups = list((game/'save-backups').glob('*/saves.zip'));assert len(backups)==1
    with zipfile.ZipFile(backups[0]) as z:
        assert z.read('modules/arcanum/Save/Slot0001.tfaf') == save.read_bytes()
        assert str(active.relative_to(game)) in z.read('RESTORE.txt').decode()
    for name, want in hotfix['assets'].items():
        assert hashlib.sha256((active/name).read_bytes()).hexdigest()==want['sha256']
    if language == 'fr':
        original = Archive(active/'Arcanum.PATCH8'); translated = Archive(active/'french-module.dat')
        assert len(translated.entries)==7
        for name in translated.entries:
            cols = 7 if name.endswith('.dlg') else 2
            before = plan.records(original.read(name),cols);after = plan.records(translated.read(name),cols)
            assert before.keys()==after.keys(),name
            for key, row in before.items():
                if cols == 7:
                    assert row[3:]==after[key][3:],(name,key,'dialogue logic')
                    if row[3].strip() not in ('','0'): assert row[2]==after[key][2],(name,key,'voice reference')
                else: assert row[1].split('/')[0]==after[key][1].split('/')[0],(name,key,'coordinates')
        magnus=plan.records(translated.read('dlg/01046magnus_shale_fist.dlg'),7)
        assert 'Qintarra' in magnus[449][1] and 'Roue' in magnus[448][1]
    else: assert not (active/'french-module.dat').exists()
    # Unknown existing hotfix is never overwritten or automatically re-offered.
    ready = (active/'ready').read_bytes();(active/'ready').write_text('unknown')
    run(game,'probe',0);run(game,'install-hotfix',1);(active/'ready').write_bytes(ready)
    for name,want in cat['original'].items():
        assert hashlib.sha256((game/name).read_bytes()).hexdigest()==originals[name]
    print('PASS:',lang,'upgrade, refusal, conflicts, backup failure, activation, idempotence, original data and translation logic',flush=True)
print('English uses shared original French art with the English UAP overlay; a proprietary English distribution is not included.')
