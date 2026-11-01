"""Exercise the public compiler CLI with real files and symbolic dialogues."""
import json
from pathlib import Path
import struct
import subprocess
import sys
import tempfile

exe = str(Path(sys.argv[1]).resolve())


def run(*args, success=True):
    result = subprocess.run([exe, *map(str, args)], capture_output=True, timeout=30)
    assert (result.returncode == 0) == success, result.stderr.decode(errors='replace')


with tempfile.TemporaryDirectory(prefix='script-cli-') as temporary:
    root = Path(temporary)/'Projet été'
    root.mkdir()
    source = root/'01999.wscript'
    source.write_text('dialog(1); label reward: global(1900)=1; return;\n', encoding='utf-8')
    dialogue = {'format':'renaissance-dialogue-source', 'version':1, 'lines':[
        ['1','Bonjour.','Bonjour.','','','0',''],
        ['2','Merci.','','1','','@reward','']]}
    (root/'01999.wdlg').write_text(json.dumps(dialogue), encoding='utf-8')
    run('--compile', source, '--output', root/'01999.scr')
    run('--validate', root/'01999.scr')
    header = bytearray((root/'01999.scr').read_bytes())
    struct.pack_into('<I', header, 0, 0xa5a5a5a5)
    (root/'01999.scr').write_bytes(header)
    output = Path(temporary)/'Export été'
    run('--compile-project', root, '--output', output)
    assert struct.unpack_from('<I', (output/'scr/01999.scr').read_bytes())[0] == 0xa5a5a5a5
    native_dialogue = (output/'dlg/01999.dlg').read_text(encoding='cp1252')
    assert '{-1}' in native_dialogue, native_dialogue
    run('--compile-project', root, '--output', output, success=False)
    assert (output/'dlg/01999.dlg').read_text(encoding='cp1252') == native_dialogue
    source.write_text('do_nothing(); dialog(1); label reward: return;', encoding='utf-8')
    second = Path(temporary)/'second'
    run('--compile-project', root, '--output', second)
    assert '{-2}' in (second/'dlg/01999.dlg').read_text(encoding='cp1252')
    source.write_text('return;', encoding='utf-8')
    missing = Path(temporary)/'missing'
    run('--compile-project', root, '--output', missing, success=False)
    assert not missing.exists(), 'Failed compilation wrote a partial export'
    source.write_text('let x = ;', encoding='utf-8')
    run('--compile', source, '--output', missing, success=False)
    assert not missing.exists()
print('PASS: CLI, Unicode paths, properties, dialogue relocation and export boundaries')
