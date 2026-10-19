#!/usr/bin/env python3
import argparse
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

parser = argparse.ArgumentParser()
parser.add_argument('engine', type=Path)
parser.add_argument('game', type=Path)
args = parser.parse_args()
engine, game = args.engine.resolve(), args.game.resolve()
modules = sorted(p for p in (game/'modules').iterdir() if p.suffix.lower() == '.dat')
assert modules, 'No installed modules'


def copy_archive(source, target):
    try:
        os.link(source, target)
    except OSError:
        shutil.copyfile(source, target)


for module in modules:
    with tempfile.TemporaryDirectory(prefix='arcanum-first-launch-') as temporary:
        root = Path(temporary)
        (root/'modules').mkdir()
        for source in game.iterdir():
            if source.is_file() and source.suffix.lower() == '.dat':
                copy_archive(source, root/source.name)
        for source in (game/'modules').iterdir():
            if source.is_file() and source.stem.lower() == module.stem.lower():
                if source.suffix.lower() == '.dat' or source.suffix.lower().startswith('.patch'):
                    copy_archive(source, root/'modules'/source.name)
        if (game/'data').is_dir():
            shutil.copytree(game/'data', root/'data', ignore=lambda path, names: [
                name for name in names if name.lower() in ('save', 'saves', 'players', 'tigcache', 'maps')])
        cache = root/'modern-ui/local-maps'
        assert not cache.exists()
        for count in (1, 2):
            result = subprocess.run([str(engine), '--modern-generate-maps', str(root), '1'], timeout=300)
            log = (root/'modern-ui/.map-work/generation.log').read_text(errors='replace')
            assert result.returncode == 75, log[-4000:]
            assert len(list(cache.rglob('*.png'))) == count * 4, 'Incomplete tile or resume skipped completed work'
            assert 'Cannot open map' not in log and 'Error reading mobile objects' not in log, log[-4000:]
        assert not (root/'modules'/module.stem/'maps').exists(), 'Generated objects escaped the work directory'
        print(f'PASS: {module.stem}: first launch without prepared objects and resume', flush=True)
