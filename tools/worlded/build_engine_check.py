#!/usr/bin/env python3
"""Link the Linux CE integration check against an existing Renaissance Ninja build.

Does not rebuild or install the game. All generated files stay in build/worlded.
"""
import argparse
from pathlib import Path
import shlex
import shutil
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build-dir', type=Path, default=Path('build'))
    parser.add_argument('--playtest', action='store_true')
    parser.add_argument('--scene', action='store_true', help='Validate an arbitrary exported playtest sector')
    parser.add_argument('--authoring', action='store_true', help='Execute the GUI authoring regression fixtures')
    parser.add_argument('--inventory', action='store_true', help='Build the native NPC inventory materializer')
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[2]
    build = args.build_dir.resolve()
    output = root / 'build/worlded'
    output.mkdir(parents=True, exist_ok=True)
    ninja = shutil.which('ninja') or '/tmp/arcanum-ce-sdk/usr/bin/ninja'
    commands = subprocess.check_output([ninja, '-C', str(build), '-t', 'commands', 'arcanum-ce'], text=True).splitlines()
    command = shlex.split(next(x for x in commands if ' -c ' in x and '/src/ui/wmap_ui.c' in x))
    for flag in ['-o', '-MF', '-MT']:
        if flag in command:
            command[command.index(flag) + 1] = str(output / ('check-engine.d' if flag == '-MF' else 'check-engine.o'))
    command[command.index('-c') + 1] = str(Path(__file__).with_name('prepare_inventory.c' if args.inventory else 'check_authoring.c' if args.authoring else 'check_scene.c' if args.scene else 'check_playtest.c' if args.playtest else 'check_engine.c'))
    subprocess.run(command, cwd=build, check=True)
    link_line = next(x for x in commands if ' -o arcanum-ce ' in x)
    link = shlex.split(next(part.strip() for part in link_line.split('&&') if ' -o arcanum-ce ' in part))
    link = [x for x in link if not x.endswith('/src/main.c.o')]
    link.insert(1, str(output / 'check-engine.o'))
    link[link.index('-o') + 1] = str(output / ('worlded-native-inventory' if args.inventory else 'check-worlded-authoring' if args.authoring else 'check-worlded-scene' if args.scene else 'check-worlded-playtest' if args.playtest else 'check-worlded-engine'))
    subprocess.run(link, cwd=build, check=True)
    print(output / ('worlded-native-inventory' if args.inventory else 'check-worlded-authoring' if args.authoring else 'check-worlded-scene' if args.scene else 'check-worlded-playtest' if args.playtest else 'check-worlded-engine'))


if __name__ == '__main__':
    main()
