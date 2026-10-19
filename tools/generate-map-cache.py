#!/usr/bin/env python3
import argparse
import json
import hashlib
import shutil
import subprocess
from pathlib import Path
from map_archive import Module

parser = argparse.ArgumentParser()
parser.add_argument('--game', type=Path, default=Path('game'))
parser.add_argument('--output', type=Path, default=Path('dist/local-map-cache'))
parser.add_argument('--work', type=Path, default=Path('build/map-export'))
parser.add_argument('--exporter', type=Path, default=Path('build/macos-arm64/export_maps'))
parser.add_argument('--module')
parser.add_argument('--map', type=int)
parser.add_argument('--tile-limit', type=int, default=0)
parser.add_argument('--inventory-only', action='store_true')
args = parser.parse_args()
game, output, work = args.game.resolve(), args.output.resolve(), args.work.resolve()
if work == game or work.is_relative_to(game) or output == game or output.is_relative_to(game):
    parser.error('Export and scratch directories must be outside the game installation')
output.mkdir(parents=True, exist_ok=True)
work.mkdir(parents=True, exist_ok=True)
modules = [p.stem for p in (game / 'modules').glob('*.dat') if not args.module or p.stem == args.module]
if not modules:
    parser.error('No matching installed module')
for name in modules:
    module = Module(game, name)
    snapshot = module.source_snapshot()
    signature = hashlib.sha256(json.dumps(snapshot, sort_keys=True).encode()).hexdigest()
    catalog = module.catalog()
    for info in catalog:
        if not info['source_map'] and name == 'Arcanum' and info['id'] == 18:
            info['source_map'] = catalog[0]['source_map']
            info['association'] = 'Overworld location; unused townmap identifier in installed sectors'
    missing = [m['name'] for m in catalog if not m['source_map']]
    if missing:
        raise RuntimeError(f'Cannot associate maps: {missing}')
    destination = output / name
    destination.mkdir(exist_ok=True)
    manifest_path = destination / 'manifest.json'
    if manifest_path.exists():
        previous = json.loads(manifest_path.read_text())
        if previous.get('roofs') is not True:
            raise RuntimeError(f'Roof policy changed: generate a fresh cache in a different output directory: {name}')
        if previous.get('source_signature', signature) != signature:
            raise RuntimeError(f'Source files changed: generate a fresh cache in a different output directory: {name}')
    manifest = dict(format=1, module=name, tile_size=1024, pixels_per_world_pixel=1,
                    levels=[1, 2, 4, 8], source_signature=signature,
                    text_baked=False, roofs=True, dynamic_objects=False, maps=catalog)
    manifest_path.write_text(json.dumps(manifest, ensure_ascii=False, indent=2))
    if args.inventory_only:
        continue
    sandbox = work / name
    sandbox.mkdir(exist_ok=True)
    (sandbox / '.map-export-sandbox').touch()
    for archive in game.glob('*.[dD][aA][tT]'):
        link = sandbox / archive.name
        if not link.exists():
            link.symlink_to(archive)
    (sandbox / 'modules').mkdir(exist_ok=True)
    for archive in (game / 'modules').glob(name + '.*'):
        if archive.is_file():
            link = sandbox / 'modules' / archive.name
            if not link.exists():
                link.symlink_to(archive)
    for source in (game / 'data', game / 'modules' / name):
        if source.exists():
            target = sandbox / source.relative_to(game)
            shutil.copytree(source, target, dirs_exist_ok=True,
                            ignore=shutil.ignore_patterns('Save', 'save', 'TIGCache', 'screenshots', 'movies', 'sound'))
    jobs = sandbox / 'jobs.tsv'
    selected = [m for m in catalog if not args.map or m['id'] == args.map]
    if not selected:
        raise RuntimeError(f'No matching local map in {name}')
    selected.sort(key=lambda m: (m['source_map'], m['id']))
    jobs.write_text(''.join(f"{m['id']}\t{m['source_map']}\n" for m in selected))
    subprocess.run([str(args.exporter.resolve()), str(sandbox), name, str(jobs),
                    str(destination), str(args.tile_limit)], check=True)
    if module.source_snapshot() != snapshot:
        raise RuntimeError(f'Source files changed during export: {name}')
