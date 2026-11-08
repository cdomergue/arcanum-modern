#!/usr/bin/env python3
"""Build the native rendering probe from an existing Ninja engine build."""
from pathlib import Path
import argparse, shlex, shutil, subprocess
root=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--build-dir',type=Path,default=root/'build');args=p.parse_args()
build=args.build_dir.resolve();name='check-floating-health'
ninja=shutil.which('ninja') or '/tmp/arcanum-ce-sdk/usr/bin/ninja'
commands=subprocess.check_output([ninja,'-C',str(build),'-t','commands','arcanum-ce'],text=True).splitlines()
compile=shlex.split(next(x for x in commands if ' -c ' in x and '/src/ui/modern_ui.c' in x))
for flag in ['-o','-MF','-MT']:
 if flag in compile:compile[compile.index(flag)+1]=str(build/(name+('.d' if flag=='-MF' else '.o')))
compile[compile.index('-c')+1]=str(root/'tools'/f'{name}.c');subprocess.run(compile,cwd=build,check=True)
link=shlex.split(next(x for x in commands if ' -o arcanum-ce ' in x).split('&&')[1].strip())
link=[x for x in link if not any(x.endswith('/src/'+f+'.c.o') for f in ['main','ui/modern_ui'])]
link[1:1]=[str(build/(name+'.o'))];link[link.index('-o')+1]=str(build/name)
subprocess.run(link,cwd=build,check=True)
