"""Create cumulative revision 3 from the native revision-1 fixture."""
from pathlib import Path
import json
import hashlib
import shutil
import sys
import package


def upgrade(game, previous, destination, actors=False):
    game, previous, destination = map(lambda value: Path(value).resolve(), (game, previous, destination))
    if not (game / '.renaissance-house-test').is_file() or destination.exists():
        raise ValueError('Choose a marked test installation and a new output folder')
    manifest, _ = package.load_package(previous)
    ids = json.loads((game / 'world-update-globals.json').read_text())['quests']
    manifest['revision'] = 4 if actors else 3
    manifest['version'] = str(manifest['revision'])
    conversions = [('var', 'global_var', 1900, 321, 654),
                   ('flag', 'global_flag', 3100, 0, 1),
                   ('quest-start', 'quest_state', ids[0], 0, 2),
                   ('quest-completed-conflict', 'quest_state', ids[1], 0, 2),
                   ('quest-botched-conflict', 'quest_state', ids[2], 0, 2)]
    for identity, field, index, before, after in ([] if actors else conversions):
        manifest['operations'].append(dict(id=identity, revision=3, kind='global',
                                           field=field, index=index,
                                           before=[str(before), '0', '0'], after=[str(after), '0', '0']))
    destination.mkdir(parents=True)
    for resource in manifest['resources']:
        target = destination / resource['path']
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(previous / resource['path'], target)
    if actors:
        metadata = json.loads((game / 'world-update-fixture.json').read_text())
        for index in range(3, 6):
            relative = f'objects/actor-{index}.mob'
            target = destination / relative
            shutil.copy2(game / f'modules/Arcanum/world-update-actor-{index}.mob', target)
            manifest['resources'].append(dict(path=relative, sha256=hashlib.sha256(target.read_bytes()).hexdigest()))
            manifest['operations'].append(dict(id=f'adopt-actor-{index}', revision=4, kind='add_object',
                                               map=metadata['map'], sector=str(metadata['sector']), payload=relative,
                                               selector=dict(kind='guid', guid='0200'+bytes(range(0x80+index*16,0x90+index*16)).hex(), type=16, prototype=17325)))
    (destination / 'manifest.json').write_text(json.dumps(manifest, indent=2)+'\n')
    (destination / 'runtime.rwp').write_bytes(package.compile_manifest(manifest, destination))
    package.install_package(destination, game)
    print(destination)


if __name__ == '__main__':
    if len(sys.argv) not in (4,5) or (len(sys.argv)==5 and sys.argv[4]!='--actors'):
        raise SystemExit('Usage: upgrade-native-test.py MARKED_GAME PREVIOUS_PACKAGE NEW_PACKAGE [--actors]')
    upgrade(*sys.argv[1:4], actors=len(sys.argv)==5)
