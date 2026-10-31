#!/usr/bin/env python3
"""Compile and install bounded Renaissance world-update packages.

Installation publishes the registry last and retains earlier package revisions.
It never opens or modifies personal saves; the engine backs up the selected save
before applying a package. See FORMAT.md and docs/world-updates.md.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import re
import shutil
import struct
import tempfile
import time
import uuid

KINDS = dict(add_object=1, move_object=2, set_field=3, set_flags=4,
             set_ground=5, set_roof=6, set_block=7, hide_scenery=8, global_=9)
KINDS['global'] = KINDS.pop('global_')
SELECTORS = dict(none=0, guid=1, static=2)
POLICIES = dict(conditional=0, authoritative=1)
FIELDS = {'DESCRIPTION','NAME','SOUND_EFFECT','CRITTER_DESCRIPTION_UNKNOWN',
          'PORTAL_LOCK_DIFFICULTY','PORTAL_KEY_ID','CONTAINER_LOCK_DIFFICULTY','CONTAINER_KEY_ID'}
ZERO = '0'*64
MAX_BYTES = 64*1024*1024


def integer(value, bits=32):
    if isinstance(value, bool) or not re.fullmatch(r'\d+', str(value)):
        raise ValueError('Expected an unsigned integer, preferably a decimal string for uint64')
    result = int(value)
    if not 0 <= result < 1 << bits:
        raise ValueError(f'Integer outside uint{bits}')
    return result


def identifier(value, limit=63):
    if not isinstance(value,str) or len(value)>limit or not re.fullmatch(r'[A-Za-z0-9][A-Za-z0-9_.-]*',value):
        raise ValueError(f'Invalid stable identifier: {value!r}')
    return value


def relative(value):
    if not isinstance(value,str) or not value or len(value.encode())>=260 or value.lower()!=value or any(ord(c)<32 or ord(c)>=127 for c in value):
        raise ValueError('Resource paths must be lowercase ASCII and shorter than 260 bytes')
    if '\\' in value or ':' in value or value.startswith('/') or any(part in ('','.','..') for part in value.split('/')):
        raise ValueError(f'Unsafe resource path: {value}')
    return value


def digest(value):
    if not isinstance(value,str) or not re.fullmatch(r'[0-9a-f]{64}',value):
        raise ValueError('Expected a lowercase SHA-256 digest')
    return bytes.fromhex(value)


def string(value, cap):
    if not isinstance(value,str) or '\x00' in value:
        raise ValueError('Invalid string')
    raw=value.encode('utf-8')
    if len(raw)>=cap:
        raise ValueError('String exceeds native field capacity')
    return struct.pack('<I',len(raw))+raw


def resource_file(root, name):
    root=Path(root).resolve(); name=relative(name); file=root.joinpath(*PurePosixPath(name).parts)
    if root not in file.resolve().parents or any(p.is_symlink() for p in [file,*file.parents] if p!=root and root in p.parents):
        raise ValueError('Linked or escaping resource')
    if not file.is_file() or file.stat().st_size>MAX_BYTES:
        raise ValueError(f'Missing or oversized resource: {name}')
    return file


def encode_operation(op, revision):
    identifier(op['id']); introduced=integer(op['revision'])
    if not 0<introduced<=revision: raise ValueError('Invalid operation introduction revision')
    kind=KINDS[op['kind']]; policy=POLICIES[op.get('policy','conditional')]
    map_name=op.get('map',''); sector=integer(op.get('sector','0'),64)
    if kind!=9:
        identifier(map_name)
        if sector&0x3ffffff>=2000 or sector>>26>=2000: raise ValueError('Invalid sector coordinates')
    elif map_name or sector: raise ValueError('Global migrations cannot name a local sector')
    target=op.get('selector',{}); selector=SELECTORS[target.get('kind','none')]
    guid=target.get('guid',''); ordinal=integer(target.get('ordinal',0)); proto=integer(target.get('prototype',0)); typ=integer(target.get('type',0))
    if guid and not re.fullmatch(r'0200[0-9a-f]{32}',guid): raise ValueError('Expected persistent GUID')
    base=target.get('base_path',''); base_hash=target.get('base_sha256',ZERO)
    if base: relative(base)
    if (selector==2 or kind in (5,6,7)) and (not base or base_hash==ZERO): raise ValueError('Original source digest required')
    if selector==1 and not guid: raise ValueError('GUID selector is empty')
    field=op.get('field',''); index=integer(op.get('index',0)); mask=integer(op.get('mask','0'),64)
    before=[integer(v,64) for v in op.get('before',['0']*3)]; after=[integer(v,64) for v in op.get('after',['0']*3)]
    if len(before)!=3 or len(after)!=3: raise ValueError('Operations need three before/after values')
    payload=op.get('payload','')
    if kind==1:
        if not guid or selector != 1 or not payload.startswith('objects/'): raise ValueError('New objects require a private MOB and GUID')
        relative(payload)
    elif payload: raise ValueError('Unexpected object payload')
    if kind==2 and (selector!=1 or typ not in (2,3)): raise ValueError('Only GUID mobile containers/scenery can move')
    if kind==2:
        for values in (before,after):
            if (values[0]&0xffffffff)//64 != sector&0x3ffffff or (values[0]>>32)//64 != sector>>26:
                raise ValueError('Cross-sector movement requires a separate explicit migration')
            if values[1]>0xffffffff or values[2]>0xffffffff: raise ValueError('Offsets must be signed32 bit patterns')
    if kind==3 and field not in FIELDS: raise ValueError(f'Field is not migration-safe: {field}')
    if kind==4 and (not mask or mask&~0xd02): raise ValueError('Unsafe flag mask')
    if kind==8 and typ!=3: raise ValueError('Only scenery can be hidden')
    if kind in (5,6,7) and index >= (256 if kind==6 else 4096): raise ValueError('Cell outside sector')
    if kind==7 and (before[0]>1 or after[0]>1): raise ValueError('Blocking values must be 0/1')
    if kind==9:
        if policy: raise ValueError('Global state conversions must be conditional')
        limits={'global_var':2000,'global_flag':3200,'quest_state':2000}
        if field not in limits or index>=limits[field]: raise ValueError('Invalid global state target')
        if field=='global_flag' and (before[0]>1 or after[0]>1): raise ValueError('Flags must be boolean')
        if field=='quest_state' and (index<1000 or max(before[0],after[0])>6 or (before[0]>=4 and before[0]!=after[0])):
            raise ValueError('Quest conversion would reset a terminal choice')
    out=string(op['id'],64)+struct.pack('<III',introduced,kind,policy)+string(map_name,64)+struct.pack('<QI',sector,selector)
    out+=string(guid,37)+struct.pack('<III',ordinal,proto,typ)+string(base,260)+digest(base_hash)
    out+=string(field,64)+struct.pack('<IQ',index,mask)+struct.pack('<6Q',*(before+after))+string(payload,260)
    return out


def compile_manifest(manifest, resource_root):
    if manifest.get('format')!='renaissance-world-update' or manifest.get('format_version')!=1:
        raise ValueError('Unsupported authoring format')
    identity=identifier(manifest['package_id']); relative(identity); module=identifier(manifest['module']); revision=integer(manifest['revision'])
    minimum=integer(manifest.get('min_abi',1)); maximum=integer(manifest.get('max_abi',1))
    if not revision or not minimum<=1<=maximum: raise ValueError('Incompatible package revision/ABI')
    out=b'RWPKG01\0'+struct.pack('<I',1)+string(identity,64)+string(manifest['version'],32)+struct.pack('<III',revision,minimum,maximum)+string(module,64)
    requires=manifest.get('requires',[])
    if len(requires)>64: raise ValueError('Too many dependencies')
    out+=struct.pack('<I',len(requires)); seen=set()
    for dep in requires:
        key=identifier(dep['package_id']); relative(key); version=integer(dep['min_revision'])
        if key==identity or key in seen or not version: raise ValueError('Invalid or duplicate dependency')
        seen.add(key);out+=string(key,64)+struct.pack('<I',version)
    resources=manifest.get('resources',[])
    if len(resources)>4096: raise ValueError('Too many resources')
    out+=struct.pack('<I',len(resources)); paths=set()
    for resource in resources:
        path=relative(resource['path'])
        if path.split('/')[0] not in ('objects','proto','scr','dlg') or path in paths: raise ValueError('Invalid/duplicate resource namespace')
        paths.add(path);file=resource_file(resource_root,path);actual=hashlib.sha256(file.read_bytes()).digest()
        if actual!=digest(resource['sha256']): raise ValueError(f'Resource digest mismatch: {path}')
        out+=string(path,260)+actual
    operations=manifest.get('operations',[])
    if len(operations)>8192: raise ValueError('Too many operations')
    out+=struct.pack('<I',len(operations));seen=set();last_revision=0
    for op in operations:
        if op['id'] in seen: raise ValueError('Duplicate operation ID')
        seen.add(op['id'])
        if int(op['revision'])<last_revision: raise ValueError('Operation history must be ordered by introduction revision')
        last_revision=int(op['revision'])
        if op.get('payload') and op['payload'] not in paths: raise ValueError('Undeclared object payload')
        out+=encode_operation(op,revision)
    if len(out)>MAX_BYTES: raise ValueError('Compiled package too large')
    return out


def load_package(folder):
    folder=Path(folder).resolve();manifest_file=resource_file(folder,'manifest.json')
    data=json.loads(manifest_file.read_text(encoding='utf-8-sig'))
    runtime=compile_manifest(data,folder)
    if resource_file(folder,'runtime.rwp').read_bytes()!=runtime: raise ValueError('Manifest and runtime.rwp disagree')
    return data,runtime


def compile_package(manifest_path, destination):
    source=Path(manifest_path).resolve();destination=Path(destination).resolve()
    if destination.exists(): raise ValueError('Package output must be a new folder')
    manifest=json.loads(source.read_text(encoding='utf-8-sig'));runtime=compile_manifest(manifest,source.parent)
    destination.parent.mkdir(parents=True,exist_ok=True)
    stage=Path(tempfile.mkdtemp(prefix='.world-update-',dir=destination.parent))
    try:
        for resource in manifest.get('resources',[]):
            target=stage/resource['path'];target.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(resource_file(source.parent,resource['path']),target)
        (stage/'manifest.json').write_text(json.dumps(manifest,indent=2,ensure_ascii=False)+'\n',encoding='utf-8')
        (stage/'runtime.rwp').write_bytes(runtime);os.replace(stage,destination)
    finally:
        if stage.exists(): shutil.rmtree(stage)
    return destination


def registry_read(root, name="index.mes"):
    file=root/name
    if not file.exists(): return {}
    text=file.read_text(encoding='utf-8-sig');pattern=r'\{(\d+)\}\{([^{}\r\n]+)\}'
    if re.sub(pattern,'',text).strip(): raise ValueError('Malformed update registry')
    result={}
    for _,value in re.findall(pattern,text):
        relative(value);parts=value.split('/')
        if len(parts)!=2 or not parts[1].isdigit() or parts[0] in result: raise ValueError('Invalid/duplicate installed package')
        result[parts[0]]=value
    return result


def validate_upgrade(old,new):
    if new['module'].casefold()!=old['module'].casefold() or int(new['revision'])<int(old['revision']): raise ValueError('Package downgrade/module change refused')
    old_ops={op['id']:encode_operation(op,int(old['revision'])) for op in old['operations']}
    new_ops={op['id']:encode_operation(op,int(new['revision'])) for op in new['operations']}
    if any(int(op['revision'])<=int(old['revision']) and op['id'] not in old_ops for op in new['operations']): raise ValueError('New operation inserted into published history')
    if any(new_ops.get(key)!=value for key,value in old_ops.items()): raise ValueError('Published operation changed or removed')
    old_assets={r['path']:r['sha256'] for r in old.get('resources',[])};new_assets={r['path']:r['sha256'] for r in new.get('resources',[])}
    if any(new_assets.get(key)!=value for key,value in old_assets.items()): raise ValueError('Published resource changed or removed')


def operations_overlap(a,b):
    if a['kind']=='global' or b['kind']=='global':
        return a['kind']==b['kind'] and a.get('field')==b.get('field') and a.get('index',0)==b.get('index',0)
    if a.get('map')!=b.get('map') or int(a.get('sector',0))!=int(b.get('sector',0)):return False
    cells={'set_ground','set_roof','set_block'}
    if a['kind'] in cells or b['kind'] in cells:return a['kind']==b['kind'] and a.get('index',0)==b.get('index',0)
    x,y=a.get('selector',{}),b.get('selector',{})
    same=(x.get('guid') and x.get('guid')==y.get('guid')) or (x.get('kind')==y.get('kind')=='static' and x.get('ordinal',0)==y.get('ordinal',0) and x.get('base_sha256')==y.get('base_sha256'))
    if not same:return False
    if 'add_object' in (a['kind'],b['kind']):return True
    if a['kind']==b['kind']=='set_field':return a.get('field')==b.get('field')
    if a['kind'] in ('hide_scenery','set_flags') and b['kind'] in ('hide_scenery','set_flags'):
        return bool((2 if a['kind']=='hide_scenery' else int(a.get('mask',0)))&(2 if b['kind']=='hide_scenery' else int(b.get('mask',0))))
    return a['kind']==b['kind']=='move_object'


def validate_composition(entries):
    def needs(key,other):
        return any(dep['package_id']==other or needs(dep['package_id'],other) for dep in entries[key].get('requires',[]))
    values=list(entries.values())
    for i,a in enumerate(values):
        for b in values[:i]:
            if a['module'].casefold()==b['module'].casefold() and not needs(a['package_id'],b['package_id']) and not needs(b['package_id'],a['package_id']):
                if any(operations_overlap(x,y) for x in a['operations'] for y in b['operations']):raise ValueError('Overlapping operations require an explicit dependency')
            for x in a.get('resources',[]):
                if x['path'].startswith('objects/'):continue
                for y in b.get('resources',[]):
                    if x['path']==y['path'] and x['sha256']!=y['sha256']:raise ValueError('Conflicting package resources')
                    left=re.match(r'(proto|scr|dlg)/(0*[0-9]+)',x['path']);right=re.match(r'(proto|scr|dlg)/(0*[0-9]+)',y['path'])
                    if left and right and left[1]==right[1] and int(left[2])==int(right[2]) and x['path']!=y['path']:raise ValueError('Conflicting resource numeric IDs')


def install_package(package, game):
    package=Path(package).resolve();game=Path(game).resolve();manifest,runtime=load_package(package)
    if not game.is_dir() or not ((game/'arcanum1.dat').exists() or (game/'modules').is_dir()): raise ValueError('Choose an Arcanum installation')
    root=game/'ArcanumModern/world-updates'
    if root.exists() and root.resolve()!=root: raise ValueError('Redirected installation directory')
    root.mkdir(parents=True,exist_ok=True);registry=registry_read(root);bundled=registry_read(root,'bundled.mes');identity=manifest['package_id'];revision=int(manifest['revision']);target=root/identity/str(revision)
    if identity in registry or identity in bundled:
        old,_=load_package(root/(registry.get(identity) or bundled[identity]));validate_upgrade(old,manifest)
    proposed=dict(registry);proposed[identity]=f'{identity}/{revision}'
    effective=dict(bundled);effective.update(proposed)
    entries={key:(manifest if key==identity else load_package(root/value)[0]) for key,value in effective.items()}
    visiting=set();visited=set()
    def visit(key):
        if key in visiting: raise ValueError('Package dependency cycle')
        if key in visited:return
        visiting.add(key)
        for dep in entries[key].get('requires',[]):
            other=entries.get(dep['package_id'])
            if other is None or int(other['revision'])<int(dep['min_revision']) or other['module'].casefold()!=entries[key]['module'].casefold(): raise ValueError('Missing or incompatible package dependency')
            visit(dep['package_id'])
        visiting.remove(key);visited.add(key)
    for key in entries:visit(key)
    validate_composition(entries)
    if target.exists():
        _,existing=load_package(target)
        if existing!=runtime: raise ValueError('A published revision is immutable')
        if registry.get(identity)==proposed[identity]: return target
    backup=root/'backups'/(time.strftime('%Y%m%d-%H%M%S',time.gmtime())+'-'+uuid.uuid4().hex[:8]);backup.mkdir(parents=True)
    if (root/'index.mes').exists():shutil.copy2(root/'index.mes',backup/'index.mes')
    if (root/'bundled.mes').exists():shutil.copy2(root/'bundled.mes',backup/'bundled.mes')
    if identity in registry:shutil.copytree(root/registry[identity],backup/'previous-package')
    (backup/'restore.txt').write_text('Restore index.mes from this directory and the matching complete pre-update save. Old installed package revisions are retained. Removing a package alone does not undo saved changes.\n',encoding='utf-8')
    if not target.exists():
        target.parent.mkdir(parents=True,exist_ok=True);stage=Path(tempfile.mkdtemp(prefix='.install-',dir=root))
        try:
            for resource in manifest.get('resources',[]):
                file=stage/resource['path'];file.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(resource_file(package,resource['path']),file)
            shutil.copy2(package/'manifest.json',stage/'manifest.json');(stage/'runtime.rwp').write_bytes(runtime);os.replace(stage,target)
        finally:
            if stage.exists():shutil.rmtree(stage)
    text=''.join('{'+str(i)+'}{'+value+'}\n' for i,(_,value) in enumerate(sorted(proposed.items()),1))
    temporary=root/('index-'+uuid.uuid4().hex+'.tmp');temporary.write_text(text,encoding='utf-8');os.replace(temporary,root/'index.mes')
    with (root/'install.log').open('a',encoding='utf-8') as log:log.write(f'{time.strftime("%Y-%m-%dT%H:%M:%SZ",time.gmtime())} installed {identity} revision {revision}; previous registry: {backup}\n')
    return target


def main():
    parser=argparse.ArgumentParser(description=__doc__);commands=parser.add_subparsers(dest='command',required=True)
    compile_=commands.add_parser('compile');compile_.add_argument('manifest');compile_.add_argument('destination')
    install=commands.add_parser('install');install.add_argument('package');install.add_argument('game')
    check=commands.add_parser('check');check.add_argument('package')
    args=parser.parse_args()
    if args.command=='compile':print(compile_package(args.manifest,args.destination))
    elif args.command=='install':print(install_package(args.package,args.game))
    else:
        manifest,_=load_package(args.package);print(f"Valid: {manifest['package_id']} revision {manifest['revision']}")

if __name__=='__main__':main()
