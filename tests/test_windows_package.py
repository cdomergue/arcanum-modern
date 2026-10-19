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
    assert z.testzip() is None
    names=z.namelist();assert len(names)==len(set(names))
    expected={'arcanum-modern.exe','Play Arcanum Modern.bat'}|{'modern-ui/'+s for s in p.ASSETS}
    for name in names:
        assert name in expected or name.startswith('ArcanumModern/'),name
        assert not name.startswith('/') and '..' not in Path(name).parts
        assert not name.lower().endswith(('.dat','.sav','.gsi','.cfg','.ini')),name
        assert 'notebook-' not in name and 'Save/' not in name
    assert expected.issubset(names)
    assert z.read('Play Arcanum Modern.bat').count(b'\r\n')>10
    metadata=json.loads(z.read('ArcanumModern/manifest.json'))
    assert sorted(p.pe_imports(z.read('arcanum-modern.exe')))==metadata['imports']
    for name,digest in metadata['sha256'].items():
        assert hashlib.sha256(z.read(name)).hexdigest()==digest,name
print('PASS: Windows x64 executable, portable runtime, complete assets, hashes and no game data/preferences/saves')
