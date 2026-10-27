#!/usr/bin/env python3
"""Build the live map regression harness from an existing Ninja game build.

Run build/check-map-zoom GAME SAVE_SLOT OUTPUT_PREFIX on an isolated marked copy.
"""
from pathlib import Path
import shlex,subprocess
root=Path(__file__).resolve().parents[1];build=root/'build'
import shutil
ninja=shutil.which('ninja') or '/tmp/arcanum-ce-sdk/usr/bin/ninja'
commands=subprocess.check_output([ninja,'-C',str(build),'-t','commands','arcanum-ce'],text=True).splitlines()
for name,replace in [('check-map-zoom','modern_ui'),('check-travel-map','wmap_ui')]:
 compile=shlex.split(next(x for x in commands if ' -c ' in x and '/src/ui/'+replace+'.c' in x))
 for flag in ['-o','-MF','-MT']:
  if flag in compile:compile[compile.index(flag)+1]=str(build/(name+('.d' if flag=='-MF' else '.o')))
 compile[compile.index('-c')+1]=str(root/'tools'/f'{name}.c');subprocess.run(compile,cwd=build,check=True)
link=shlex.split(next(x for x in commands if ' -o arcanum-ce ' in x).split('&&')[1].strip())
link=[x for x in link if not any(x.endswith('/src/'+f+'.c.o') for f in ['main','ui/modern_ui','ui/wmap_ui'])]
link[1:1]=[str(build/'check-map-zoom.o'),str(build/'check-travel-map.o')]
link[link.index('-o')+1]=str(build/'check-map-zoom');subprocess.run(link,cwd=build,check=True)
