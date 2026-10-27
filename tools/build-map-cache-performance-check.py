#!/usr/bin/env python3
"""Build the real map-cache-performance regression harness against an existing Ninja build."""
from pathlib import Path
import sys
import shlex
import shutil
import subprocess

root = Path(__file__).resolve().parents[1]
build = root / 'build'
ninja = shutil.which('ninja') or '/tmp/arcanum-ce-sdk/usr/bin/ninja'
commands = subprocess.check_output([ninja, '-C', str(build), '-t', 'commands', 'arcanum-ce'], text=True).splitlines()
compile_command = shlex.split(next(x for x in commands if ' -c ' in x and '/src/ui/modern_ui.c' in x))
for flag in ['-o', '-MF', '-MT']:
    if flag in compile_command:
        compile_command[compile_command.index(flag) + 1] = str(build / ('check-map-cache-performance' + ('.d' if flag == '-MF' else '.o')))
compile_command[compile_command.index('-c') + 1] = str(root / 'tools/check-map-cache-performance.c')
if '--baseline' in sys.argv:
    old_cache=build/'map-cache-before.inc'
    assert old_cache.exists(), 'Save the original map cache before building the baseline'
    ui=(root/'source/src/ui/modern_ui.c').read_text().replace('"modern_map_cache.inc"', '"'+str(old_cache)+'"')
    (build/'modern-ui-perf-before.c').write_text(ui)
    harness=(root/'tools/check-map-cache-performance.c').read_text().replace('"../source/src/ui/modern_ui.c"', '"'+str(build/'modern-ui-perf-before.c')+'"')
    (build/'map-cache-perf-before.c').write_text(harness)
    compile_command[compile_command.index('-c')+1]=str(build/'map-cache-perf-before.c')
    compile_command+=['-DMAP_CACHE_BASELINE','-iquote',str(root/'source/src/ui')]
subprocess.run(compile_command, cwd=build, check=True)
link = shlex.split(next(x for x in commands if ' -o arcanum-ce ' in x).split('&&')[1].strip())
link = [x for x in link if not any(x.endswith('/src/' + f + '.c.o') for f in ['main', 'ui/modern_ui'])]
link.insert(1, str(build / 'check-map-cache-performance.o'))
link[link.index('-o') + 1] = str(build / ('check-map-cache-performance-before' if '--baseline' in sys.argv else 'check-map-cache-performance'))
subprocess.run(link, cwd=build, check=True)
