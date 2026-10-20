#!/usr/bin/env python3
"""Validate a generated French UAP overlay against supplied game archives."""
import argparse
import gzip
import hashlib
import importlib.util
import json
from pathlib import Path
import re
from map_archive import Archive

ROOT = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--game', required=True, type=Path)
parser.add_argument('--uap', required=True, type=Path)
args = parser.parse_args()
active = args.game/'modern-ui/uap-fr/active'
assert (active/'ready').read_text() == '2.0.2-fr-1\n'
catalog = json.loads((ROOT/'tools/uap/data/catalog.json').read_text())
for name, expected in catalog['patch'].items():
    path = active/(('global/' if '/' not in name else '')+name)
    assert path.stat().st_size == expected['size'], name
    assert hashlib.sha256(path.read_bytes()).hexdigest() == expected['sha256'], name
plan = json.loads(gzip.decompress((ROOT/'tools/uap/data/plan.json.gz').read_bytes()))
archives = [Archive(args.uap/'Arcanum5.dat'), Archive(args.uap/'ArcanumXRaces.dat')]
module = Archive(args.uap/'modules/Arcanum.PATCH1')
french = {scope: Archive(active/f'french-{scope}.dat') for scope in ('global', 'module')}
count = 0
for file in plan['files']:
    repos = [module] if file['scope'] == 'module' else archives
    original = next(a.read(file['path']) for a in reversed(repos) if file['path'] in a.entries)
    localized = french[file['scope']].read(file['path'])
    if file['columns'] == 7:
        pattern = rb'(?m)^\s*' + rb'\s*'.join([rb'\{([^}]*)\}']*7)
        before, after = re.findall(pattern, original), re.findall(pattern, localized)
        assert len(before) == len(after), file['path']
        for a, b in zip(before, after):
            assert a[0] == b[0] and a[3:] == b[3:], (file['path'], a[0], 'script or condition changed')
            if a[3].strip() not in (b'', b'0'):
                assert a[2] == b[2], (file['path'], a[0], 'gender constraint changed')
            for col in (1, 2):
                if re.match(rb'^[A-Z]:', a[col]):
                    assert a[col] == b[col], (file['path'], a[0], 'directive changed')
        count += len(before)
assert set(french['global'].entries) == {f['path'] for f in plan['files'] if f['scope'] == 'global'}
assert set(french['module'].entries) == {f['path'] for f in plan['files'] if f['scope'] == 'module'}
print(f'PASS: {len(catalog["patch"])} unchanged UAP data files, {len(plan["files"])} localized resources, {count} dialogue records with original UAP logic')
