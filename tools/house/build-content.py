#!/usr/bin/env python3
"""Compile the extensible estate-agent catalogue into native Arcanum resources.
Slots, ownership globals and key IDs are save-game identities: never renumber them.
The native modern_house module handles the guarded purchase/resupply commands.
"""
import argparse
import json
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
    for p in catalog['properties']:
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
    add(1, "Edwin Harrow, agent immobilier. Je peux vous présenter nos propriétés ou vous aider avec une maison déjà achetée." if fr else "Edwin Harrow, estate agent. I can show you our properties or help you with a house you already own.")
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

def build(catalog, output, scenes=ROOT/'resources/house'):
    validate(catalog)
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
    args=parser.parse_args(); build(json.loads(args.catalog.read_text()),args.output)
