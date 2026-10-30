#!/usr/bin/env python3
"""Check real game resources without changing the installation or any save.

Uses a temporary workspace; optional CE checks use a disposable installation with
read-only archive links and no links to writable game directories.
"""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import struct
import subprocess
import sys
import tempfile

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from map_archive import Archive  # Independent existing Python reader.


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('game', type=Path)
    parser.add_argument('worlded', type=Path)
    parser.add_argument('--engine-check', type=Path)
    args = parser.parse_args()
    game, worlded = args.game.resolve(), args.worlded.resolve()
    module = game / 'modules/Arcanum.dat'
    resource = 'maps/pschuylers lev 1-fixed/67108865.sec'
    original = Archive(module).read(resource)
    before = hashlib.sha256(original).hexdigest()
    archives = sorted(game.glob('*.[dD][aA][tT]')) + [module]
    snapshots = {path: (path.stat().st_size, path.stat().st_mtime_ns) for path in archives}

    def cli(*values):
        return subprocess.check_output([str(worlded), *map(str, values)], text=True, encoding='utf-8')

    with tempfile.TemporaryDirectory(prefix='worlded-real-check-') as temporary:
        work = Path(temporary)
        for path in archives:
            # Audit only archives containing supported resources.
            if any(name.endswith(('.sec', '.scr')) for name in Archive(path).entries):
                print(path.name, cli('audit', path).strip(), flush=True)
        project = work / 'roundtrip'
        cli('init', project)
        imported = Path(json.loads(cli('import', module, resource, project))['output'])
        if imported.read_bytes() != original:
            raise RuntimeError('C++ DAT reader differs from independent Python reader')
        exported = Path(json.loads(cli('roundtrip', imported, project))['output'])
        if exported.read_bytes() != original:
            raise RuntimeError('Sector roundtrip differs from original')
        info = json.loads(cli('inspect', imported))
        tile_offset = 4 + struct.unpack_from('<I', original)[0] * 48
        tiles = struct.unpack_from('<4096I', original, tile_offset)
        replacement = next(art for art in tiles if art != tiles[0])
        edit_project = work / 'edited'
        cli('init', edit_project)
        edited = Path(json.loads(cli('tile', imported, edit_project, 0, 0, replacement))['output'])
        expected = bytearray(original)
        struct.pack_into('<I', expected, tile_offset, replacement)
        if edited.read_bytes() != expected:
            raise RuntimeError('Tile edit changed unrelated data')
        print(f'PASS: independent DAT extraction, exact roundtrip and isolated tile edit; SHA256 {before}', flush=True)

        if args.engine_check:
            checker = args.engine_check.resolve()
            for variant, sector in [('roundtrip', exported), ('edited', edited)]:
                sandbox = work / f'engine-{variant}'
                (sandbox / 'modules/Arcanum').mkdir(parents=True)
                (sandbox / '.worlded-engine-check').write_text('isolated test\n')
                for archive in archives:
                    target = sandbox / ('modules' if archive == module else '') / archive.name
                    target.symlink_to(archive)
                destination = sandbox / 'modules/Arcanum' / resource
                destination.parent.mkdir(parents=True)
                shutil.copyfile(sector, destination)
                expected_tiles = sandbox / 'expected-tiles.bin'
                expected_tiles.write_bytes(sector.read_bytes()[tile_offset:tile_offset + 4096 * 4])
                result = subprocess.run([str(checker), str(sandbox), 'pschuylers lev 1-fixed',
                    '67108865', str(expected_tiles), str(info['objects']), str(info['townmap'])],
                    capture_output=True, text=True, timeout=120)
                print(variant, result.stdout.strip(), flush=True)
                if result.returncode:
                    print(result.stderr, file=sys.stderr)
                    for log in sandbox.rglob('*.log'):
                        print(log.read_text(errors='replace')[-4000:], file=sys.stderr)
                    raise RuntimeError(f'CE check failed: {variant}, exit {result.returncode}')

            sprite_name = 'art/container/chest_wood1.art'
            sprite = work / 'chest.art'
            sprite.write_bytes(Archive(game / 'arcanum2.dat').read(sprite_name))
            metadata = json.loads(cli('decode', sprite))
            for rotation, frame in [(0, 0), (0, min(2, metadata['frames']-1))]:
                item = metadata['frame_data'][rotation*metadata['frames']+frame]
                pixels = subprocess.check_output([str(worlded), 'pixels', str(sprite), str(rotation), str(frame), '0'])
                expected_art = work / 'expected-art.bin'
                expected_art.write_bytes(struct.pack('<4i', item['width'], item['height'], item['hot_x'], item['hot_y']) + pixels)
                subprocess.run([str(checker), str(sandbox), '--art', sprite_name, str(expected_art), str(rotation), str(frame), '0'], check=True, timeout=30)

    if hashlib.sha256(Archive(module).read(resource)).hexdigest() != before:
        raise RuntimeError('Source sector changed')
    if any((path.stat().st_size, path.stat().st_mtime_ns) != state for path, state in snapshots.items()):
        raise RuntimeError('Source archive metadata changed')
    print('PASS: source archives unchanged; no saves read or written')


if __name__ == '__main__':
    main()
