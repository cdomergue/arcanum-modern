"""Restore campaign start in an older isolated tutorial installation."""
from pathlib import Path
import argparse,re
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('game',type=Path)
args=parser.parse_args()
assert (args.game/'modern-ui/tutorial/session-v1').is_file(), 'Prepared tutorial installation required'
p=args.game/'modules/Arcanum/rules/maplist.mes'
data=p.read_bytes()
# Only migrate the exact old test-island override; preserve other module entries.
new,count=re.subn(rb'(\{5000\}\{Arcanum1-024-fixed,\s*)25621,\s*50332',rb'\g<1>92958,82592',data,flags=re.I)
if count:
    backup=p.with_suffix('.mes.before-campaign-start')
    if not backup.exists():backup.write_bytes(data)
    p.write_bytes(new)
print('Campaign start restored.' if count else 'No old island start override found; unchanged.')
