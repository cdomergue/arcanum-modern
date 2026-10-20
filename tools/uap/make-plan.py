#!/usr/bin/env python3
"""Build reference-only localization instructions from supplied game archives."""
import argparse,hashlib,json,re,sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from map_archive import Archive

ALIASES={'dlg/03100game_of_witt_female.dlg':'dlg/01957game_of_witt.dlg'}
TECHNICAL=re.compile(r'^[A-Z]:')
def records(raw,columns):
    pattern=r'(?m)^\s*'+r'\s*'.join([r'\{([^}]*)\}']*columns)
    return {int(x[0].strip()):list(x) for x in re.findall(pattern,raw.decode('cp1252',errors='surrogateescape')) if x[0].strip().isdigit()}
def archives(root):
    paths=sorted(root.glob('*.dat'),key=lambda p:p.name.lower())
    paths+=sorted((root/'modules').glob('Arcanum.*'),key=lambda p:p.name.lower())
    return [Archive(p) for p in paths if p.is_file() and p.suffix.lower() in ('.dat','.patch0')]
def read(repos,name):
    for a in reversed(repos):
        if name in a.entries:return a.read(name)
    return b''
def norm(s):return re.sub(r'[^a-z0-9@]','',s.lower())
def main():
    p=argparse.ArgumentParser();p.add_argument('--french',type=Path,required=True);p.add_argument('--english',type=Path,required=True);p.add_argument('--uap',type=Path,required=True);p.add_argument('--output',type=Path,required=True);args=p.parse_args()
    fr,en=archives(args.french),archives(args.english)
    uap=[('global',Archive(args.uap/name)) for name in ['Arcanum5.dat','ArcanumXRaces.dat']]+[('module',Archive(args.uap/'modules/Arcanum.PATCH1'))]
    resources={}
    for scope,a in uap:
        for name in a.entries:
            if name.endswith('.dlg') or name.startswith('mes/') and name.endswith('.mes'):resources[(scope,name)]=a.read(name)
    extra_path=Path(__file__).with_name('data')/'french.json'
    extra=json.loads(extra_path.read_text()) if extra_path.exists() else {}
    edits=json.loads(extra_path.with_name('edits.json').read_text())
    global_lookup={}
    for name in sorted({k for a in en for k in a.entries if k.endswith('.dlg')}):
        original=records(read(en,name),7);localized=records(read(fr,name),7)
        for id,row in original.items():
            if id not in localized:continue
            for col in ([1,2] if row[3].strip() in ('','0') else [1]):
                fc=col if localized[id][col].strip() else 1
                if row[col].strip() and localized[id][fc].strip():global_lookup.setdefault(norm(row[col]),(name,id,fc))
    result=[];missing=[];changed=[]
    for (scope,name),raw in sorted(resources.items()):
        columns=7 if name.endswith('.dlg') else 2;target=records(raw,columns);ref=ALIASES.get(name,name)
        french=records(read(fr,ref),columns);english=records(read(en,ref),columns)
        lookup={}
        for id,row in english.items():
            for col in ([1,2] if columns==7 and row[3].strip() in ('','0') else [1]):
                if id in french:
                    fc=col if french[id][col].strip() else 1
                    if french[id][fc].strip():lookup.setdefault(norm(row[col]),(id,fc))
        actions=[]
        for id,row in target.items():
            for col in ([1,2] if columns==7 and row[3].strip() in ('','0') else [1]):
                value=row[col];key=f'{scope}/{name}:{id}:{col}'
                if not value.strip() or columns==7 and TECHNICAL.match(value):continue
                if key in extra:actions.append({'id':id,'column':col,'text':extra[key]});continue
                source_id,source_col=id,col
                if norm(value) in lookup:source_id,source_col=lookup[norm(value)]
                elif source_id in french and french[source_id][col if french[source_id][col].strip() else 1].strip():
                    source_col=col if french[source_id][col].strip() else 1
                    if id not in english or norm(value)!=norm(english[id][col]):changed.append({'key':key,'uap':value,'original':english.get(id,['']*columns)[col],'french':french[id][source_col]})
                else:
                    # Reuse identical installed lines across dialogue files when IDs differ.
                    found=global_lookup.get(norm(value)) if columns==7 else None
                    if found:actions.append({'id':id,'column':col,'source':found[0],'source_id':found[1],'source_column':found[2]});continue
                    missing.append({'key':key,'text':value});continue
                action={'id':id,'column':col,'source':ref,'source_id':source_id,'source_column':source_col}
                if key in edits:action['replace']=edits[key]
                actions.append(action)
        result.append({'scope':scope,'path':name,'sha256':hashlib.sha256(raw).hexdigest(),'columns':columns,'actions':actions})
    args.output.parent.mkdir(parents=True,exist_ok=True)
    args.output.write_text(json.dumps({'version':1,'files':result},ensure_ascii=True,separators=(',',':')))
    (args.output.parent/'missing.json').write_text(json.dumps(missing,ensure_ascii=True,indent=2))
    (args.output.parent/'changed.json').write_text(json.dumps(changed,ensure_ascii=True,indent=2))
    print(f'{len(result)} resources; {len(missing)} untranslated fields; {len(changed)} changed English fields using existing French')
if __name__=='__main__':main()
