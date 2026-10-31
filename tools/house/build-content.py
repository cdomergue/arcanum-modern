#!/usr/bin/env python3
"""Compile the extensible estate-agent catalogue into native Arcanum resources.
Slots, ownership globals and key IDs are save-game identities: never renumber them.
The native modern_house module handles the guarded purchase/resupply commands.
"""
import argparse
import importlib.util
import hashlib
import json
import re
import shutil
import struct
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
LANGUAGES = ('en', 'fr')

def validate(catalog):
    if catalog['version'] != 1:
        raise ValueError('Unsupported catalogue version')
    for field in ('id', 'slot', 'ownership_global', 'discovery_global', 'key_id'):
        vals = [p[field] for p in catalog['properties']]
        if len(set(vals)) != len(vals):
            raise ValueError(f'Duplicate {field}')
    globals_ = [p[f] for p in catalog['properties'] for f in ('ownership_global', 'discovery_global')]
    if len(set(globals_)) != len(globals_):
        raise ValueError('Ownership and discovery globals must not overlap')
    sectors = [tuple(p['sector']) for p in catalog['properties']]
    if len(set(sectors)) != len(sectors):
        raise ValueError('Each property needs its own sector (guard and bed ownership use sectors)')
    for p in catalog['properties']:
        if not re.fullmatch(r'[a-z][a-z0-9_-]{0,62}', p['scene']):
            raise ValueError('Scene must be a simple folder name, at most 63 characters')
        if len(p['sector']) != 2 or any(type(v) is not int or not 0 <= v < 2000 for v in p['sector']):
            raise ValueError('Invalid property sector')
        if not isinstance(p['price'], int) or isinstance(p['price'], bool) or not 0 < p['price'] <= 2**31-1:
            raise ValueError('Price must be a positive 32-bit integer')
        if not 0 <= p['slot'] < 100 or not 0 <= p['ownership_global'] < 2000 or not 0 <= p['discovery_global'] < 2000:
            raise ValueError('Invalid persistent identity')
        for lang in LANGUAGES:
            for value in p['text'][lang].values():
                if any(c in value for c in '{}\r\n'):
                    raise ValueError('Dialogue text contains a format delimiter')
                value.encode('cp1252')

def row(n, text, target=0, player=False):
    if len(text.encode('cp1252')) > 990 or any(c in text for c in '{}\r\n'):
        raise ValueError('Invalid native dialogue text')
    return n, f'{{{n}}}{{{text}}}{{{"" if player else text}}}{{{1 if player else ""}}}{{}}{{{target}}}{{}}'

def dialogue(catalog, lang):
    fr = lang == 'fr'
    props = sorted(catalog['properties'], key=lambda p: p['slot'])
    rows = []
    add = lambda *a, **k: rows.append(row(*a, **k))
    back = 'Revenir au catalogue.' if fr else 'Back to the catalogue.'
    bye = 'Au revoir.' if fr else 'Goodbye.'
    add(1, f"{catalog['seller']['name']}, agent immobilier. Je peux vous présenter nos propriétés ou vous aider avec une maison déjà achetée." if fr else f"{catalog['seller']['name']}, estate agent. I can show you our properties or help you with a house you already own.")
    add(2, 'Voir les propriétés.' if fr else 'Show me your properties.', 1000, True)
    add(3, bye, 0, True)
    # Three listings + next page + exit: the engine allows five responses.
    pages = [props[i:i+3] for i in range(0, len(props), 3)] or [[]]
    for page, entries in enumerate(pages):
        base = 1000 + page * 10
        add(base, 'Quelle propriété vous intéresse ?' if fr else 'Here is our catalogue. Which property interests you?')
        for j, p in enumerate(entries, 1):
            t = p['text'][lang]
            price = f"{p['price']:,}".replace(',', ' ' if fr else ',')
            add(base+j, f"{t['name']} - {t['standing']} - {price} " + ('pièces' if fr else 'coins'), -(100+p['slot']*10), True)
        if len(pages)>1:
            add(base+4, 'Page suivante.' if fr else 'Next page.', 1000+((page+1)%len(pages))*10, True)
        add(base+5, bye, 0, True)
    for p in props:
        t = p['text'][lang]; b = 3000 + p['slot']*100
        command = 100 + p['slot']*10
        price = f"{p['price']:,}".replace(',', ' ' if fr else ',')
        add(b, f"{t['name']} - {t['standing']}. {t['description']} " + (f"Prix : {price} pièces." if fr else f"Price: {price} coins."))
        add(b+1, "Je souhaite l'acheter." if fr else "I would like to buy it.", b+10, True)
        add(b+2, back, 1000, True)
        add(b+10, t['disclosure']+' '+(f"L'achat coûte {price} pièces et comprend la clé. Confirmez-vous ?" if fr else f"The purchase costs {price} coins and includes the key. Shall we proceed?"))
        add(b+11, f"Oui, acheter pour {price} pièces." if fr else f"Yes, buy it for {price} coins.", -(command+1), True)
        add(b+12, 'Je vais réfléchir.' if fr else "I'll think about it.", 1000, True)
        add(b+20, f"Vous êtes déjà propriétaire de {t['name']}. Avez-vous besoin d'une clé de remplacement ?" if fr else f"You already own {t['name']}. Do you need a replacement key?")
        add(b+21, 'Oui, une clé de remplacement.' if fr else 'Yes, a replacement key.', -(command+2), True)
        add(b+22, back, 1000, True)
        messages = {
          30: ("La vente est conclue. Voici votre clé. La maison est à vous.", "The sale is complete. Here is your key. The house is yours."),
          40: (f"Il vous faut {price} pièces pour cet achat. Revenez quand vous aurez la somme.", f"You need {price} coins for this purchase. Come back when you have the funds."),
          50: ("Je n'ai pas pu vous remettre la clé. La transaction n'a pas été effectuée et vous n'avez rien payé.", "I could not give you the key. The transaction was not completed and you have not been charged."),
          60: ("Vous avez déjà la clé de cette propriété.", "You already have the key to this property."),
          70: ("Voici votre clé de remplacement, sans frais.", "Here is your replacement key, free of charge.")}
        for offset, texts in messages.items():
            add(b+offset, texts[0 if fr else 1]); add(b+offset+1, back, 1000, True); add(b+offset+2, bye, 0, True)
    ids = [n for n,_ in rows]
    if len(set(ids)) != len(ids): raise ValueError('Dialogue line collision')
    return '\n'.join(s for _,s in sorted(rows))+'\n'

def placeholder_script():
    # Valid native SCR: one unconditional return-and-skip row.
    op = lambda opcode: struct.pack('<I8B8i', opcode, *([0]*16))
    header = bytearray(64)
    struct.pack_into('<ii', header, 52, 1, 1)
    return bytes(header)+op(0)+op(1)+op(0)

def check_scenes(catalog, scenes):
    """Fail before generation if any listing would load an incomplete scene."""
    all_ids = set()
    for p in catalog['properties']:
        folder = scenes / p['scene']
        for name in ('scene.mes', 'terrain.mes', 'roofs.mes', 'removed.mes'):
            if not (folder / name).is_file():
                raise ValueError(f"{p['id']}: missing {name}")
        # Optional overlays use explicit IDs; reject malformed/duplicate records
        # before a content pack can be installed.
        for name, limit, width in [('blocking',4095,1),('environment',3,1),('scripts',4096,3)]:
            file = folder / (name + '.mes')
            if not file.exists():
                continue
            text = file.read_text(encoding='utf-8')
            pattern = r'\{(\d+)\}\{([^}]*)\}'
            if re.sub(pattern, '', text).strip():
                raise ValueError(f'{file}: invalid MES record')
            seen = set()
            for key, raw in re.findall(pattern, text):
                key = int(key)
                values = raw.split()
                if key > limit or key in seen or len(values) != width:
                    raise ValueError(f'{file}: invalid or duplicate overlay ID {key}')
                seen.add(key)
                try:
                    values = [int(v) for v in values]
                except ValueError as exc:
                    raise ValueError(f'{file}: expected integer values') from exc
                valid = (values[0] in (0,1) if name == 'blocking' else
                         -2**31 <= values[0] < 2**31 if name == 'environment' else
                         0 <= values[0] < 2**31 and all(0 <= v < 2**32 for v in values[1:]))
                if not valid:
                    raise ValueError(f'{file}: value out of range')
        light_file = folder / 'removed-lights.bin'
        if light_file.exists():
            data = light_file.read_bytes()
            if len(data) < 4:
                raise ValueError(f'{light_file}: missing light count')
            count, = struct.unpack_from('<I', data)
            if count > 4096 or len(data) != 4 + count * 48:
                raise ValueError(f'{light_file}: invalid light records')
            for offset in range(4, len(data), 48):
                owner, = struct.unpack_from('<Q', data, offset)
                aid, = struct.unpack_from('<I', data, offset + 28)
                if owner or aid >> 28 != 9:
                    raise ValueError(f'{light_file}: expected an independent native light')
        records = re.findall(r'\{(\d+)\}\{([^}]*)\}', (folder/'scene.mes').read_text(encoding='utf-8'))
        entries = dict(records)
        if len(entries) != len(records):
            raise ValueError(f"{p['id']}: duplicate manifest entry")
        if entries.get('0', '').split() != ['1', *map(str, p['sector'])]:
            raise ValueError(f"{p['id']}: scene sector does not match catalogue")
        count = 0
        for index, entry in entries.items():
            if int(index) < 100:
                continue
            parts = entry.split()
            if len(parts) != 2 or not re.fullmatch(r'0200[0-9a-f]{32}', parts[1]):
                raise ValueError(f"{p['id']}: invalid object entry")
            role, guid = parts
            if guid in all_ids:
                raise ValueError(f'Duplicate scene object GUID: {guid}')
            all_ids.add(guid)
            data = (folder/'objects'/f'{guid}.mob').read_bytes()
            if len(data) < 56 or (data[28:30]+data[36:52]).hex() != guid:
                raise ValueError(f'{guid}: object file ID does not match manifest')
            if role == 'guard' and int.from_bytes(data[52:56], 'little') != 16:
                raise ValueError(f'{guid}: guard must be an NPC')
            count += 1
        inventory_file = folder / 'inventory.mes'
        if inventory_file.exists():
            text = inventory_file.read_text(encoding='utf-8')
            pattern = r'\{(\d+)\}\{(0200[0-9a-f]{32}) (0200[0-9a-f]{32})\}'
            if re.sub(pattern, '', text).strip():
                raise ValueError(f'{inventory_file}: invalid inventory record')
            object_ids = {entry.split()[1] for key, entry in entries.items() if int(key) >= 100}
            seen_keys, seen_children = set(), set()
            for key, child, parent in re.findall(pattern, text):
                if key in seen_keys or child in seen_children or child == parent or child not in object_ids or parent not in object_ids:
                    raise ValueError(f'{inventory_file}: invalid or duplicate inventory reference')
                seen_keys.add(key)
                seen_children.add(child)
        if not count:
            raise ValueError(f"{p['id']}: scene has no objects")


def build_update_package(property_, source, base):
    """Compile a reference-backed template; old scenes keep the legacy loader."""
    template = source / 'update-template.json'
    if not template.exists():
        return None
    spec = importlib.util.spec_from_file_location('housing_world_updates', ROOT/'tools/world-updates/package.py')
    compiler = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(compiler)
    manifest = json.loads(template.read_text(encoding='utf-8'))
    for path, expected in manifest.get('housing_source_hashes', {}).items():
        file = compiler.resource_file(source, path)
        if hashlib.sha256(file.read_bytes()).hexdigest() != expected:
            raise ValueError(f'Housing authoring source changed; re-export the update template: {path}')
    scene_guids = {value.split()[1] for key, value in re.findall(r'\{(\d+)\}\{([^}]*)\}', (source/'scene.mes').read_text()) if int(key) >= 100}
    update_guids = {op['selector']['guid'] for op in manifest['operations'] if op['kind'] == 'add_object'}
    if scene_guids != update_guids:
        raise ValueError('Housing template object IDs differ from scene.mes; re-export the template')
    manifest['package_id'] = f"housing.{property_['slot']}"
    identity = manifest['package_id']
    revision = int(manifest['revision'])
    destination = base/'world-updates'/identity/str(revision)
    destination.mkdir(parents=True, exist_ok=True)
    for resource in manifest['resources']:
        path = compiler.relative(resource['path'])
        file = source/path if path.startswith('objects/') else source/'resources'/path
        data = compiler.resource_file(file.parent, file.name).read_bytes()
        if hashlib.sha256(data).hexdigest() != resource['sha256']:
            raise ValueError(f'Housing template resource changed; re-export it: {path}')
        target = destination/path
        target.parent.mkdir(parents=True, exist_ok=True)
        if target.exists() and target.read_bytes() != data:
            raise ValueError(f'Published housing resource is immutable: {path}')
        target.write_bytes(data)
    registry = compiler.registry_read(base/'world-updates')
    if identity in registry:
        old_manifest, _ = compiler.load_package(base/'world-updates'/registry[identity])
        compiler.validate_upgrade(old_manifest, manifest)
    runtime = compiler.compile_manifest(manifest, destination)
    prior = destination/'runtime.rwp'
    if prior.exists() and prior.read_bytes() != runtime:
        raise ValueError('Housing package changed without a new cumulative revision')
    prior.write_bytes(runtime)
    (destination/'manifest.json').write_text(json.dumps(manifest, indent=2)+'\n', encoding='utf-8')
    return identity, f'{identity}/{revision}'


def build(catalog, output, scenes=ROOT/'resources/house'):
    validate(catalog)
    check_scenes(catalog, scenes)
    resources = {}
    native_ids = {48300, 48301, 48304}
    for p in catalog['properties']:
        extra = scenes / p['scene'] / 'resources'
        if not extra.exists():
            continue
        for file in extra.rglob('*'):
            if file.is_symlink():
                raise ValueError('Linked scene resources are not supported')
            if not file.is_file():
                continue
            relative = file.relative_to(extra)
            if len(relative.parts) != 2 or relative.parts[0] not in ('scr', 'dlg', 'proto'):
                raise ValueError(f'Invalid scene resource: {relative}')
            match = re.match(r'(\d+)', file.name)
            if not match:
                raise ValueError(f'Resource needs a numeric ID: {relative}')
            key = (relative.parts[0], int(match[1]))
            if key[0] in ('scr', 'dlg') and key[1] in native_ids:
                raise ValueError(f'Resource conflicts with the estate service: {relative}')
            data = file.read_bytes()
            if key in resources and resources[key] != (relative, data):
                raise ValueError(f'Conflicting shared resource ID: {key}')
            resources[key] = (relative, data)
    base = output/'base'; (base/'house').mkdir(parents=True, exist_ok=True)
    (base/'scr').mkdir(exist_ok=True)
    pos = catalog['seller']['position']
    (base/'house/seller.mes').write_text('{0}{'+' '.join(str(pos[k]) for k in ('x','y','offset_x','offset_y','rotation'))+'}\n')
    for script, stem in [(48300,'estate_guard'), (48301,'estate_agent'), (48304,'estate_bed')]:
        (base/'scr'/f'{script}_{stem}.scr').write_bytes(placeholder_script())
    lines = []
    for p in catalog['properties']:
        lines.append(f"{{{p['slot']}}}{{{p['price']} {p['ownership_global']} {p['discovery_global']} {p['key_id']} {p['sector'][0]} {p['sector'][1]} {p['scene']}}}")
        if (scenes/p['scene']).is_dir():
            shutil.copytree(scenes/p['scene'], base/'house'/p['scene'], dirs_exist_ok=True)
    (base/'house/catalog.mes').write_text('\n'.join(lines)+'\n')
    update_entries = {}
    registry = base/'world-updates/index.mes'
    if registry.exists():
        update_entries = {value.split('/')[0]: value for _, value in re.findall(r'\{(\d+)\}\{([^}]*)\}', registry.read_text(encoding='utf-8'))}
    for property_ in catalog['properties']:
        entry = build_update_package(property_, scenes/property_['scene'], base)
        if entry:
            update_entries[entry[0]] = entry[1]
    if update_entries:
        registry.parent.mkdir(parents=True, exist_ok=True)
        registry.write_text(''.join(f'{{{i}}}{{{value}}}\n' for i, value in enumerate(sorted(update_entries.values()))), encoding='utf-8')

    for relative, data in resources.values():
        (base/relative).parent.mkdir(parents=True, exist_ok=True)
        (base/relative).write_bytes(data)
    for lang in LANGUAGES:
        local = output/lang; (local/'dlg').mkdir(parents=True,exist_ok=True); (local/'house').mkdir(exist_ok=True)
        (local/'dlg/48301_estate_agent.dlg').write_bytes(dialogue(catalog,lang).encode('cp1252'))
        seller = catalog['seller']
        guard = []
        for p in catalog['properties']:
            t = p['text'][lang]; price = f"{p['price']:,}".replace(',', ' ' if lang=='fr' else ',')
            message = (f"{t['name']} ({t['standing']}) est à vendre. Adressez-vous à {seller['name']}, {seller['location'][lang]}. Le prix est de {price} pièces." if lang=='fr' else f"{t['name']} ({t['standing']}) is for sale. Speak to {seller['name']}, {seller['location'][lang]}. The price is {price} coins.")
            line = 1+p['slot']*10
            guard.extend([row(line,message),row(line+1,'Merci.' if lang=='fr' else 'Thank you.',0,True)])
        (local/'dlg/48300_estate_guard.dlg').write_bytes(('\n'.join(s for _,s in sorted(guard))+'\n').encode('cp1252'))
        descriptions = [f"{{{seller['description_id']}}}{{{seller['name']}}}"]
        descriptions.append('{61005}{'+('Agent immobilier' if lang=='fr' else 'Estate agent')+'}')
        descriptions += [f"{{{p['key_id']}}}{{{p['text'][lang]['key']}}}" for p in catalog['properties']]
        descriptions += [f"{{{70000+p['slot']}}}{{{p['text'][lang]['name']}}}" for p in catalog['properties']]
        (local/'house/descriptions.mes').write_bytes(('\n'.join(descriptions)+'\n').encode('cp1252'))

if __name__ == '__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--catalog',type=Path,default=ROOT/'resources/house/catalog.json')
    parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--scenes',type=Path,default=ROOT/'resources/house')
    args=parser.parse_args(); build(json.loads(args.catalog.read_text(encoding='utf-8')),args.output,args.scenes)
