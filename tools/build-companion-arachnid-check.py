"""Link the Linux arachnid regression against an existing Renaissance Ninja build."""
from pathlib import Path
import os
import shlex
import shutil
import subprocess

root = Path(__file__).resolve().parents[1]
build = Path(os.environ.get('ARCANUM_BUILD_DIR', root / 'build')).resolve()
ninja = shutil.which('ninja') or '/tmp/arcanum-ce-sdk/usr/bin/ninja'
commands = subprocess.check_output([ninja, '-C', str(build), '-t', 'commands', 'arcanum-ce'], text=True).splitlines()
command = shlex.split(next(x for x in commands if ' -c ' in x and '/src/ui/wmap_ui.c' in x))
for flag in ['-o', '-MF', '-MT']:
    if flag in command:
        command[command.index(flag) + 1] = str(build / ('check-companion-arachnid.d' if flag == '-MF' else 'check-companion-arachnid.o'))
command[command.index('-c') + 1] = str(root / 'tools/check-companion-arachnid.c')
subprocess.run(command, cwd=build, check=True)
link = shlex.split(next(x for x in commands if ' -o arcanum-ce ' in x).split('&&')[1].strip())
link = [x for x in link if not x.endswith('/src/main.c.o')]
link.insert(1, str(build / 'check-companion-arachnid.o'))
link[link.index('-o') + 1] = str(build / 'check-companion-arachnid')
subprocess.run(link, cwd=build, check=True)
