#!/usr/bin/env python3
"""Build the real UAP hotfix regression harness against an existing Ninja build."""
from pathlib import Path
import shlex
import shutil
import subprocess

root = Path(__file__).resolve().parents[1]
build = root / 'build'
ninja = shutil.which('ninja') or '/tmp/arcanum-ce-sdk/usr/bin/ninja'
commands = subprocess.check_output([ninja, '-C', str(build), '-t', 'commands', 'arcanum-ce'], text=True).splitlines()
compile_command = shlex.split(next(x for x in commands if ' -c ' in x and '/src/ui/wmap_ui.c' in x))
for flag in ['-o', '-MF', '-MT']:
    if flag in compile_command:
        compile_command[compile_command.index(flag) + 1] = str(build / ('check-uap-hotfix' + ('.d' if flag == '-MF' else '.o')))
compile_command[compile_command.index('-c') + 1] = str(root / 'tools/check-uap-hotfix.c')
subprocess.run(compile_command, cwd=build, check=True)
link = shlex.split(next(x for x in commands if ' -o arcanum-ce ' in x).split('&&')[1].strip())
link = [x for x in link if not any(x.endswith('/src/' + f + '.c.o') for f in ['main'])]
link.insert(1, str(build / 'check-uap-hotfix.o'))
link[link.index('-o') + 1] = str(build / 'check-uap-hotfix')
subprocess.run(link, cwd=build, check=True)
