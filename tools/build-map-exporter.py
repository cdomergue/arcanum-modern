#!/usr/bin/env python3
import argparse
import shlex
import subprocess
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument('--build-dir', type=Path, default=Path('build/macos-arm64'))
parser.add_argument('--source', type=Path, default=Path('tools/export_maps.c'))
parser.add_argument('--name', default='export_maps')
parser.add_argument('--exclude-object', action='append', default=[])
args = parser.parse_args()
root = Path(__file__).resolve().parent.parent
build = args.build_dir.resolve()
target = build / 'CMakeFiles/arcanum-ce.dir'
subprocess.run(['cmake', '--build', str(build), '--parallel', '8'], check=True)
flags = dict(line.split(' = ', 1) for line in (target / 'flags.make').read_text().splitlines() if ' = ' in line)
link = shlex.split((target / 'link.txt').read_text())
obj = build / (args.name + '.o')
subprocess.run([link[0], *shlex.split(flags['C_FLAGS']), *shlex.split(flags['C_INCLUDES']),
                '-c', str(args.source.resolve()), '-o', str(obj)], check=True)
link = [str(obj) if part.endswith('/src/main.c.o') else part for part in link]
link = [part for part in link if not any(part.endswith('/' + name) for name in args.exclude_object)]
link[link.index('-o') + 1] = str(build / args.name)
subprocess.run(link, cwd=build, check=True)
print(build / args.name)
