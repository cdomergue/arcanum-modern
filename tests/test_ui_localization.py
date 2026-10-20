#!/usr/bin/env python3
"""Check English/French UI selection and translated format arguments."""
from pathlib import Path
import json, os, re, subprocess, tempfile

root=Path(__file__).resolve().parents[1]
ui=root/'source/src/ui'
source=(ui/'modern_ui.c').read_text()
function=re.search(r'^const char\* modern_ui_translate\([^\n]+\)\n\{[^}]+\}',source,re.M).group()
literal=r'"(?:[^"\\]|\\.)*"'
pair=re.compile(r'\b(?:tr|modern_ui_translate)\(\s*('+literal+r')\s*,\s*('+literal+r')\s*\)',re.S)
formats=re.compile(r'%(?:[-+ #0]*\d*(?:\.\d+)?(?:hh|ll|[hljztL])?[diuoxXfFeEgGaAcsp]|%)')
pairs=[]
for path in sorted(ui.glob('modern*')):
    if path.suffix not in ('.c','.inc'):continue
    for match in pair.finditer(path.read_text()):
        fr,en=map(json.loads,match.groups())
        assert en.strip(),(path,fr)
        assert formats.findall(fr)==formats.findall(en),(path,fr,en)
        pairs.append(match.groups())
assert pairs
code='''#include <assert.h>
#include <string.h>
#include <stdio.h>
static const char* level;
static const char* modern_intgame_text(int id){(void)id;return level;}
'''+function+'\nint main(void){\n'
for fr,en in pairs:
    code+=f'level="Level";assert(!strcmp(modern_ui_translate({fr},{en}),{en}));\n'
    code+=f'level="Niveau";assert(!strcmp(modern_ui_translate({fr},{en}),{fr}));\n'
code+='level="Stufe";assert(!strcmp(modern_ui_translate("fr","en"),"en"));\n'
code+=f'puts("PASS: {len(pairs)} bilingual UI strings, format arguments and English fallback");\n}}\n'
with tempfile.TemporaryDirectory(prefix='arcanum-localization-') as directory:
    p=Path(directory);(p/'test.c').write_text(code)
    subprocess.run([os.environ.get('CC','cc'),'-std=c17','-Wall','-Wextra',str(p/'test.c'),'-o',str(p/'test')],check=True)
    subprocess.run([str(p/'test')],check=True)
