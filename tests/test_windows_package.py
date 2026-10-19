#!/usr/bin/env python3
"""Check release contents and hashes; reject non-Windows executables."""
from pathlib import Path
import hashlib, importlib.util, json, sys, zipfile
root=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('packager',root/'tools/package-windows.py')
p=importlib.util.module_from_spec(spec);spec.loader.exec_module(p)
try:
    p.pe_imports(b'\x7fELF'+b'\0'*100)
    raise AssertionError('Accepted a Linux executable')
except ValueError:
    pass
archive=Path(sys.argv[1]) if len(sys.argv)>1 else root/'dist/ArcanumModern-windows-x64.zip'
with zipfile.ZipFile(archive) as z:
    metadata=json.loads(z.read('ArcanumModern/manifest.json'))
    full=metadata.get('includes_game_data',False)
    names=z.namelist();assert len(names)==len(set(names))
    expected={'arcanum-modern.exe','Play Arcanum Modern.bat'}|{'modern-ui/'+s for s in p.ASSETS}|set(p.MOVIE_LIBRARIES['windows'])
    p.validate_pe_dependencies({name:z.read(name) for name in ('arcanum-modern.exe',*p.MOVIE_LIBRARIES['windows'])})
    for name in names:
        assert full or name in expected or name.startswith('ArcanumModern/'),name
        assert not name.startswith('/') and '..' not in Path(name).parts
        assert not name.lower().endswith(('.sav','.gsi','.cfg','.ini','.tfaf','.tfai')),name
        assert full or not name.lower().endswith('.dat'),name
        assert 'notebook-' not in name.lower(),name
        assert not {'save','players','tigcache','screenshots','highres','__macosx'} & {part.lower() for part in Path(name).parts},name
        assert not any(part.startswith('.') or part.lower().endswith('.app') for part in Path(name).parts),name
    if full:
        assert p.REQUIRED_GAME_FILES.issubset({name.lower() for name in names})
    assert expected.issubset(names)
    assert z.read('Play Arcanum Modern.bat').count(b'\r\n')>10
    assert sorted(p.pe_imports(z.read('arcanum-modern.exe')))==metadata['imports']
    for name,digest in metadata['sha256'].items():
        actual=hashlib.sha256()
        with z.open(name) as entry:
            while block:=entry.read(1024*1024):
                actual.update(block)
        assert actual.hexdigest()==digest,name
    assert set(metadata['sha256'])==set(names)-{'ArcanumModern/manifest.json'}
print('PASS: Windows x64 executable, portable runtime, complete assets, hashes and no personal data; game data:',full)
