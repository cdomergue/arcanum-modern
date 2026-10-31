"""Package the metadata emitted by check-world-updates --prepare."""
from pathlib import Path
import hashlib
import json
import shutil
import sys
import package


def build(game, destination):
    game, destination = Path(game).resolve(), Path(destination).resolve()
    if not (game / '.renaissance-house-test').is_file() or destination.exists():
        raise ValueError('Choose a marked test installation and a new package folder')
    metadata = json.loads((game / 'world-update-fixture.json').read_text())
    destination.mkdir(parents=True)
    (destination / 'objects').mkdir()
    payload = destination / 'objects/decor.mob'
    shutil.copy2(game / 'modules/Arcanum/world-update-decor.mob', payload)
    common = dict(revision=1, map=metadata['map'], sector=str(metadata['sector']))
    def guid(index):
        return '0200' + bytes(range(0x80 + index * 16, 0x90 + index * 16)).hex()
    operations = [
        dict(common, id='move-chest', kind='move_object',
             selector=dict(kind='guid', guid=guid(0), type=2, prototype=metadata.get('chest_prototype',3064)),
             before=[str(metadata['before_location']), '0', '0'],
             after=[str(metadata['after_location']), '0', '0']),
        dict(common, id='add-decor', kind='add_object', payload='objects/decor.mob',
             selector=dict(kind='guid', guid=guid(2), type=3,
                           prototype=metadata['decor_prototype'])),
        dict(common, id='change-block', kind='set_block', index=0,
             selector=dict(kind='none', base_path=metadata['base_path'],
                           base_sha256=metadata['base_sha256']),
             before=[str(metadata['block_before']), '0', '0'],
             after=[str(1-metadata['block_before']), '0', '0']),
    ]
    manifest = dict(format='renaissance-world-update', format_version=1,
                    package_id='native-regression', module='Arcanum', version='1', revision=1,
                    resources=[dict(path='objects/decor.mob', sha256=hashlib.sha256(payload.read_bytes()).hexdigest())],
                    operations=operations)
    (destination / 'manifest.json').write_text(json.dumps(manifest, indent=2)+'\n')
    (destination / 'runtime.rwp').write_bytes(package.compile_manifest(manifest, destination))
    package.install_package(destination, game)
    print(destination)


if __name__ == '__main__':
    if len(sys.argv) != 3:
        raise SystemExit('Usage: package-native-test.py MARKED_TEST_GAME NEW_PACKAGE_DIRECTORY')
    build(*sys.argv[1:])
