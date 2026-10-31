"""Make an isolated native housing inventory fixture from an existing test copy.

Usage: python prepare-inventory-test.py MARKED_TEST_GAME NEW_COPY FIXTURE_OUTPUT
FIXTURE_OUTPUT is produced by worlded-inventory-check --housing-fixture GAME OUT.
Only copied saves are used. The source test copy is never changed.
"""
from pathlib import Path
import shutil
import sys


def prepare(source, destination, fixture):
    source, destination, fixture = (Path(p).resolve() for p in (source, destination, fixture))
    if not (source / '.renaissance-house-test').is_file():
        raise ValueError('The source must be an existing marked housing test copy')
    if destination.exists() or source == destination or source in destination.parents:
        raise ValueError('Choose a new directory outside the source copy')
    for name in ('scene-append.mes', 'inventory.mes'):
        if not (fixture / name).is_file():
            raise ValueError(f'Missing native fixture {name}')

    def ignore(directory, names):
        if Path(directory).name.casefold() == 'save':
            return [name for name in names if not name.startswith(('Slot0002', 'Slot0088'))]
        return [name for name in names if name.casefold() in ('tigcache', 'current')]

    shutil.copytree(source, destination, ignore=ignore)
    scene = destination / 'ArcanumModern/house/base/house/scene'
    manifest = scene / 'scene.mes'
    if not manifest.is_file():
        raise ValueError('Test copy has no loose housing scene fixture')
    manifest.write_bytes(manifest.read_bytes() + b'\n' + (fixture / 'scene-append.mes').read_bytes())
    shutil.copy2(fixture / 'inventory.mes', scene / 'inventory.mes')
    for name in ('removed-lights.bin', 'test-native-lights.bin'):
        if (scene / name).exists():
            raise ValueError(f'Source test copy already contains {name}')
        shutil.copy2(fixture / name, scene / name)
    for item in (fixture / 'objects').glob('*.mob'):
        if (scene / 'objects' / item.name).exists():
            raise ValueError(f'Fixture identity already present: {item.stem}')
        shutil.copy2(item, scene / 'objects' / item.name)
    print(destination)


if __name__ == '__main__':
    if len(sys.argv) != 4:
        raise SystemExit(__doc__)
    prepare(*sys.argv[1:])
