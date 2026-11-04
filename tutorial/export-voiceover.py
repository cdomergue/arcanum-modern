"""Export the tutorial's actual bilingual strings for voice recording (UTF-8)."""
from pathlib import Path
import csv
import json
import re
ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'tutorial' / 'doublage'
PAIR = re.compile(r'tr\(\s*("(?:\\.|[^"\\])*")\s*,\s*("(?:\\.|[^"\\])*")\s*\)')
def export():
    source = (ROOT/'source/src/ui/modern_tutorial_lessons.inc').read_text(encoding='utf-8')
    blocks = re.findall(r'case (TUTORIAL_\w+):(.*?)(?=\n    case |\n    default:)', source, re.S)
    rows=[]
    for key, block in blocks:
        pairs=PAIR.findall(block)
        for i,(fr,en) in enumerate(pairs):
            rows.append((key + (f'_{i+1}' if len(pairs)>1 else ''),json.loads(fr),json.loads(en)))
    # The route lesson is played after the menu lesson, not in enum order.
    route=next(row for row in rows if row[0]=='TUTORIAL_ROUTE')
    rows.remove(route)
    rows.insert(next(i for i,row in enumerate(rows) if row[0]=='TUTORIAL_FIGHT'),route)
    invite=(ROOT/'source/src/ui/modern_tutorial.inc').read_text(encoding='utf-8')
    invite=invite[invite.index('static void tutorial_draw_invite'):invite.index('// Preserve the module')]
    pairs=PAIR.findall(invite)
    pairs.sort(key=lambda pair: 0 if json.loads(pair[0])=='Bienvenue sur l’île' else 1)
    extras=[(f'INVITATION_{i:02d}',json.loads(fr),json.loads(en)) for i,(fr,en) in enumerate(pairs,1)]
    extras.append(('BUTTON_SKIP','Passer','Skip'))
    OUT.mkdir(parents=True,exist_ok=True)
    with (OUT/'tutoriel-fr-en.tsv').open('w',encoding='utf-8-sig',newline='') as f:
        w=csv.writer(f,delimiter='\t');w.writerow(['ID','FR','EN']);w.writerows(rows+extras)
    for lang,col in [('fr',1),('en',2)]:
        text='\n\n'.join(f'[{row[0]}]\n{row[col]}' for row in rows)
        text+='\n\n'+('INVITATION ET BOUTONS (facultatif)' if lang=='fr' else 'INVITATION AND BUTTONS (optional)')+'\n\n'
        text+='\n\n'.join(f'[{row[0]}]\n{row[col]}' for row in extras)+'\n'
        (OUT/f'tutoriel-{lang}.txt').write_bytes(text.replace('\n','\r\n').encode('utf-8-sig'))
    assert len(rows)==35, len(rows)
    print(f'Exported {len(rows)} lesson lines (including comparison variants), {len(extras)} invitation/button lines.')
if __name__=='__main__': export()
