"""Read-only inventory of campaign bug candidates; never opens or writes saves.

The report contains control flow and hashes, not redistributed dialogue text.
Pass archives in actual load order, earliest first, with --overlay.
"""
import argparse
import hashlib
import json
import re
import struct
from pathlib import Path

from map_archive import Archive, Module

CANDIDATES = {
    'willoughsby': ['01565willoughsby3', '01558willoughsby2'],
    'citizen-alignment': ['01823citizen'],
    'theodor': ['01033theodoreminequest', '02705theodorehb', '02119theodoredeath'],
    'witt': ['01940david_witt', '01941cynthia_witt', '01957game_of_witt', '03100game_of_witt_female'],
    'dante': ['01013dante', '01014dantekilling', '01016danteheartbeat', '01678dantesector',
              '02013dante_enter_combat', '02461danteresurrect', '02462dantedeath'],
    'island-exit': ['01061edward_teach', '01163ed_teach2'],
    'kraka-tur': ['01114kraka-tur', '01135eye_of_kraka-tur_exa'],
    'fahrkus': ['02792fahrkus', '02791fahrkusfirst', '02799fahrkusdeath',
                '02789brehgo', '02790brehgoressurect'],
}


def dialogue_flow(raw):
    fields = re.findall(rb'\{([^}]*)\}', raw)
    if len(fields) % 7:
        raise ValueError('Incomplete seven-column dialogue record')
    rows = []
    for n in range(0, len(fields), 7):
        row = fields[n:n+7]
        rows.append(dict(line=int(row[0]), intelligence=row[3].decode('ascii'),
                         condition=row[4].decode('ascii'), target=row[5].decode('ascii'),
                         action=row[6].decode('ascii')))
    return rows


def script_metadata():
    labels = (Path(__file__).resolve().parent / 'worlded/src/script_labels.inc').read_text(encoding='utf-8')
    result = []
    for kind in ('condition', 'action'):
        names = re.findall(r'"([A-Z_0-9]+)"', re.search(kind + r'Names\[\]=\{(.*?)\};', labels, re.S)[1])
        masks = [int(n) for n in re.findall(r'(\d+),', re.search(kind + r'Operands\[\]=\{(.*?)\};', labels, re.S)[1])]
        if len(names) != len(masks):
            raise ValueError('Inconsistent generated script metadata')
        result.append(list(zip(names, masks)))
    return result


def script_flow(raw, metadata):
    if len(raw) < 64:
        raise ValueError('Truncated script header')
    count, = struct.unpack_from('<I', raw, 52)
    if len(raw) < 64 + count * 132:
        raise ValueError('Truncated script instructions')
    rows = []
    for row in range(count):
        parts = []
        for part in range(3):
            offset = 64 + row * 132 + part * 44
            opcode, = struct.unpack_from('<I', raw, offset)
            table = metadata[part != 0]
            if opcode >= len(table):
                parts.append({'unknown_opcode': opcode})
                continue
            name, mask = table[opcode]
            operands = []
            for slot in range(8):
                if mask & (1 << slot):
                    value, = struct.unpack_from('<i', raw, offset + 12 + slot * 4)
                    operands.append({'slot': slot, 'type': raw[offset + 4 + slot], 'value': value})
            parts.append({'opcode': name, 'operands': operands})
        # Native script jumps are zero-based. Unused operand bytes are not data.
        rows.append({'instruction': row, 'condition': parts[0], 'then': parts[1], 'else': parts[2]})
    return rows


def audit(game, overlays):
    module = Module(game, 'Arcanum')
    for path in overlays:
        archive = Archive(path)
        module.files.update({k: (archive, k) for k in archive.entries})
    report = {'game': str(game.resolve()), 'overlays': [str(p.resolve()) for p in overlays],
              'status': 'Static evidence only; gameplay reproduction required', 'candidates': {}}
    metadata = script_metadata()
    for candidate, stems in CANDIDATES.items():
        entries = []
        prefixes = {stem[:5] for stem in stems}
        paths = sorted(path for path in module.files if re.match(r'^(dlg|scr)/\d{5}', path)
                       and path[4:9] in prefixes and path.endswith(('.dlg', '.scr')))
        for path in paths:
            raw = module.read(path)
            source = module.files[path]
            origin = str(source if isinstance(source, Path) else source[0].path)
            entry = dict(path=path, source=origin, sha256=hashlib.sha256(raw).hexdigest())
            try:
                entry['flow'] = dialogue_flow(raw) if path.endswith('.dlg') else script_flow(raw, metadata)
            except (ValueError, UnicodeError) as error:
                entry['error'] = str(error)
            entries.append(entry)
        report['candidates'][candidate] = entries
    report['engine_action_uses'] = []
    for path in sorted(module.files):
        if not path.startswith('scr/') or not path.endswith('.scr'):
            continue
        try:
            rows = script_flow(module.read(path), metadata)
        except ValueError:
            continue
        for row in rows:
            for branch in ('then', 'else'):
                action = row[branch]
                if action.get('opcode') == 'SAT_DIVIDE' or (action.get('opcode') == 'SAT_ADJUST_STAT'
                        and any(p['slot'] == 0 and p['type'] == 3 and p['value'] == 19
                                for p in action['operands'])):
                    report['engine_action_uses'].append(dict(path=path, instruction=row['instruction'],
                                                             branch=branch, action=action))
    report['object_candidates'] = {
        'mechanical-arachnid': {'prototype': 28343,
            'path': 'maps/tarant sewers-01/g_dec5f747_1580_11d4_8f1d_00e02920220c.mob',
            'inspection': 'Use check-campaign --actors to read native flags; expected mechanical bit 0x20000000'},
        'air-elemental': {'prototype': 28330, 'description': 28310,
            'inspection': 'Compare rules/monster.txt with check-campaign --prototypes; localized name parsing may affect regeneration'},
    }
    report['unresolved'] = ['air-elemental: establish intended damage and spells without importing AME balance',
                            'collisions: identify reproducible map coordinates']
    return report


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('game', type=Path)
    parser.add_argument('--overlay', type=Path, action='append', default=[])
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    result = audit(args.game, args.overlay)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')
