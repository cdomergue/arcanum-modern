#!/usr/bin/env python3
"""Load a generated building with CE in a disposable installation, without saves."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import struct
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('game', type=Path)
    parser.add_argument('project', type=Path)
    parser.add_argument('--worlded', type=Path, default=Path('build/worlded/worlded'))
    parser.add_argument('--engine-check', type=Path, default=Path('build/worlded/check-worlded-engine'))
    args = parser.parse_args()
    game, project = args.game.resolve(), args.project.resolve()
    exports = project / 'exports'
    map_name=json.loads((project/'building.json').read_text())['map']
    sector = exports / f'maps/{map_name}/0.sec'
    data = sector.read_bytes()
    objects = json.loads(subprocess.check_output([str(args.worlded.resolve()), 'decode', str(sector)], text=True))['objects']
    static_count = len(objects)
    for mobile in sorted(sector.parent.glob('*.mob')):
        objects += json.loads(subprocess.check_output([str(args.worlded.resolve()), 'decode', str(mobile)], text=True))['objects']
    records = bytearray(struct.pack('<I',len(objects)))
    for obj in objects:
        values = {f['name']: f['value'] for f in obj['fields']}
        location = int(values['LOCATION'])
        records += bytes.fromhex(obj['id'][4:])
        records += struct.pack('<9I', obj['prototype_id'], obj['type'], location & 0xffffffff, location >> 32,
                               int(values['CURRENT_AID']) & 0xffffffff, int(values['FLAGS']) & 0xffffffff,
                               int(values.get('PORTAL_FLAGS', values.get('CONTAINER_FLAGS', '0'))),
                               int(values.get('CONTAINER_INVENTORY_NUM', '0')), int(values.get('CONTAINER_INVENTORY_SOURCE', '0')))
    archives = sorted(game.glob('*.[dD][aA][tT]')) + [game / 'modules/Arcanum.dat']
    before = {p: (p.stat().st_size, p.stat().st_mtime_ns) for p in archives}
    digest = hashlib.sha256(data).hexdigest()
    with tempfile.TemporaryDirectory(prefix='worlded-building-ce-') as temporary:
        root = Path(temporary)
        (root / '.worlded-engine-check').write_text('isolated test\n')
        (root / 'modules/Arcanum').mkdir(parents=True)
        for archive in archives:
            target = root / ('modules' if archive.parent.name.lower() == 'modules' else '') / archive.name
            target.symlink_to(archive)
        shutil.copytree(exports / 'maps', root / 'modules/Arcanum/maps')
        # Prototypes must be available during engine initialization, before mod_load.
        shutil.copytree(exports / 'proto', root / 'data/proto')
        tiles = root / 'tiles.bin'; tiles.write_bytes(data[4:4+4096*4])
        expected = root / 'objects.bin'; expected.write_bytes(records)
        command = [str(args.engine_check.resolve()), str(root), map_name, '0', str(tiles), str(static_count), '0', str(expected)]
        for attempt in range(2):
            result = subprocess.run(command, capture_output=True, text=True, timeout=90)
            print(f'Load {attempt+1}:', result.stdout.strip(), flush=True)
            if result.returncode:
                print(result.stderr[-4000:])
                for log in root.glob('*.log'):
                    print(log.read_text(errors='replace')[-2500:])
                raise RuntimeError(f'Building rejected by CE (exit {result.returncode})')
    assert hashlib.sha256(sector.read_bytes()).hexdigest() == digest
    assert all((p.stat().st_size, p.stat().st_mtime_ns) == state for p, state in before.items())
    print('PASS: repeated CE loads; exported sector and original archives unchanged; no saves accessed')


if __name__ == '__main__':
    main()
