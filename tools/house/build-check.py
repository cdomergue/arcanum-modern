#!/usr/bin/env python3
"""Link the Linux CE integration check against an existing Renaissance Ninja build.

Does not rebuild or install the game. All generated files stay in build/house.
"""
import argparse
from pathlib import Path
import shlex
import shutil
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build-dir', type=Path, default=Path('build'))
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[2]
    build = args.build_dir.resolve()
    output = root / 'build/house'
    output.mkdir(parents=True, exist_ok=True)
    ninja = shutil.which('ninja') or '/tmp/arcanum-ce-sdk/usr/bin/ninja'
    commands = subprocess.check_output([ninja, '-C', str(build), '-t', 'commands', 'arcanum-ce'], text=True).splitlines()
    command = shlex.split(next(x for x in commands if ' -c ' in x and '/src/ui/wmap_ui.c' in x))
    for flag in ['-o', '-MF', '-MT']:
        if flag in command:
            command[command.index(flag) + 1] = str(output / ('check-house.d' if flag == '-MF' else 'check-house.o'))
    command[command.index('-c') + 1] = str(root / 'tools/house/check-house.c')
    subprocess.run(command, cwd=build, check=True)
    link_line = next(x for x in commands if ' -o arcanum-ce ' in x)
    link = shlex.split(next(part.strip() for part in link_line.split('&&') if ' -o arcanum-ce ' in part))
    link = [x for x in link if not x.endswith('/src/main.c.o')]
    link.insert(1, str(output / 'check-house.o'))
    link[link.index('-o') + 1] = str(output / 'check-house')
    subprocess.run(link, cwd=build, check=True)
    print(output / 'check-house')


if __name__ == '__main__':
    main()
