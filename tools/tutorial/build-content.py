#!/usr/bin/env python3
"""Validate and assemble the interactive tutorial runtime pack.

All assets in the pack are original mod content (sectors 52747567503 and
52747567504, 7 mobile objects, guide script/dialogues, island townmap 145,
bilingual journal entries, and English voiceover clips). Zero base-game
archives or MES files are copied or modified.
"""
import argparse
import json
from pathlib import Path
import re
import shutil
import struct

ROOT = Path(__file__).resolve().parents[2]
LANGUAGES = ('en', 'fr')
SECTORS = ('52747567503.sec', '52747567504.sec')
EXPECTED_MOBS = (
    'g_2411bc86cc811443bde1c31ec377ff39.mob',
    'g_2ac20546fdde714882a9cfc0a5d8330d.mob',
    'g_3088abcbb0f1324f8474be7a8195ce9d.mob',
    'g_80972fd0cb7823429a94a66f117ca2da.mob',
    'g_886e4ed560907d4daa2ae26d8f582dc4.mob',
    'g_9446d492810bd640a40425cbfac90f01.mob',
    'g_fd90283762782f49aa8ae397622f6bd3.mob',
)
QUEST_IDS = ('1980', '1981', '1982', '1983')
RUMOR_IDS = ('2980', '2981', '2982', '2983')


def expected_voice_ids(lessons_source=ROOT / 'source/src/ui/modern_tutorial_lessons.inc'):
    text = lessons_source.read_text(encoding='utf-8')
    ids = tuple(sorted(set(re.findall(r'"(TUTORIAL_[A-Z0-9_]+)"', text))))
    if not ids:
        raise ValueError('No TUTORIAL_* voice IDs found in lesson source')
    return ids


def validate_dlg(data, label):
    text = data.decode('cp1252')
    for lineno, raw in enumerate(text.splitlines(), 1):
        if not raw.strip():
            continue
        fields = re.findall(r'\{([^}]*)\}', raw)
        if len(fields) != 7 or ''.join(f'{{{f}}}' for f in fields) != raw:
            raise ValueError(f'{label}:{lineno}: invalid 7-field DLG record')
        int(fields[0])
        int(fields[5])


def compile_journal_mes(journal_path):
    data = json.loads(journal_path.read_text(encoding='utf-8'))
    quests = data.get('quests', {})
    rumors = data.get('rumors', {})
    if tuple(sorted(quests)) != QUEST_IDS:
        raise ValueError(f'{journal_path}: expected quest IDs {QUEST_IDS}')
    if tuple(sorted(rumors)) != RUMOR_IDS:
        raise ValueError(f'{journal_path}: expected rumor IDs {RUMOR_IDS}')
    lines = []
    for key in (*QUEST_IDS, *RUMOR_IDS):
        value = quests[key] if key in quests else rumors[key]
        if not value or any(c in value for c in '{}\r\n'):
            raise ValueError(f'{journal_path}: invalid MES text for {key}')
        lines.append(f'{{{key}}}{{{value}}}')
    return ('\n'.join(lines) + '\n').encode('cp1252')


def validate_resources(resources=ROOT / 'resources/tutorial',
                       journal_dir=ROOT / 'tutorial/journal',
                       lessons_source=ROOT / 'source/src/ui/modern_tutorial_lessons.inc'):
    for sector in SECTORS:
        path = resources / 'maps/Arcanum1-024-fixed' / sector
        if not path.is_file() or path.stat().st_size < 1024:
            raise ValueError(f'Missing or invalid tutorial sector: {sector}')

    mob_dir = resources / 'mobs'
    found_mobs = tuple(sorted(p.name for p in mob_dir.glob('*.mob'))) if mob_dir.is_dir() else ()
    if found_mobs != EXPECTED_MOBS:
        raise ValueError(f'Tutorial mobs mismatch: expected {EXPECTED_MOBS}, got {found_mobs}')

    scr = resources / 'scr/48874_guide.scr'
    if not scr.is_file() or scr.stat().st_size < 64:
        raise ValueError('Missing or invalid guide script: 48874_guide.scr')

    for lang in LANGUAGES:
        dlg = resources / 'dlg' / lang / '48874_guide.dlg'
        if not dlg.is_file():
            raise ValueError(f'Missing guide dialogue for {lang}')
        validate_dlg(dlg.read_bytes(), str(dlg))
        compile_journal_mes(journal_dir / f'{lang}.json')

    tmi_dir = resources / 'townmap/tutorial-island'
    tmi = tmi_dir / 'tutorial-island.tmi'
    if not tmi.is_file() or tmi.stat().st_size < 36:
        raise ValueError('Missing or invalid tutorial-island.tmi')
    tmi_bytes = tmi.read_bytes()
    if len(tmi_bytes) != 48:
        raise ValueError('Invalid tutorial-island.tmi size')
    version, _, map_id, _, _, cols, rows = struct.unpack_from('<iiiiiii', tmi_bytes, 0)
    if version != 2 or map_id != 145 or cols <= 0 or rows <= 0 or cols * rows > 256:
        raise ValueError('Invalid tutorial-island.tmi header')
    expected_bmps = {f'tutorial-island{i:06d}.bmp' for i in range(cols * rows)}
    found_bmps = {p.name for p in tmi_dir.glob('*.bmp')}
    if found_bmps != expected_bmps:
        raise ValueError('Incomplete or unexpected townmap tiles in tutorial-island')

    voice_ids = expected_voice_ids(lessons_source)
    sound_dir = resources / 'sound'
    found_mp3s = tuple(sorted(p.stem for p in sound_dir.glob('*.mp3'))) if sound_dir.is_dir() else ()
    if found_mp3s != voice_ids:
        missing = set(voice_ids) - set(found_mp3s)
        extra = set(found_mp3s) - set(voice_ids)
        raise ValueError(f'Voice clips mismatch (missing={sorted(missing)}, extra={sorted(extra)})')
    for clip in voice_ids:
        if (sound_dir / f'{clip}.mp3').stat().st_size < 256:
            raise ValueError(f'Empty or corrupt voice clip: {clip}.mp3')


def build(output,
          resources=ROOT / 'resources/tutorial',
          journal_dir=ROOT / 'tutorial/journal',
          lessons_source=ROOT / 'source/src/ui/modern_tutorial_lessons.inc'):
    validate_resources(resources, journal_dir, lessons_source)
    base = output / 'base'
    if base.exists():
        shutil.rmtree(base)

    (base / 'tutorial').mkdir(parents=True, exist_ok=True)
    (base / 'tutorial/session-v1').write_bytes(b'1')

    sec_out = base / 'maps/Arcanum1-024-fixed'
    sec_out.mkdir(parents=True, exist_ok=True)
    for sector in SECTORS:
        shutil.copy2(resources / 'maps/Arcanum1-024-fixed' / sector, sec_out / sector)

    mob_out = base / 'tutorial/mobs'
    mob_out.mkdir(parents=True, exist_ok=True)
    for mob in EXPECTED_MOBS:
        shutil.copy2(resources / 'mobs' / mob, mob_out / mob)

    scr_out = base / 'scr'
    scr_out.mkdir(parents=True, exist_ok=True)
    shutil.copy2(resources / 'scr/48874_guide.scr', scr_out / '48874_guide.scr')

    for lang in LANGUAGES:
        lang_out = base / 'tutorial' / lang
        lang_out.mkdir(parents=True, exist_ok=True)
        dlg_bytes = (resources / 'dlg' / lang / '48874_guide.dlg').read_bytes().replace(b'\r\n', b'\n')
        (lang_out / '48874_guide.dlg').write_bytes(dlg_bytes)
        (lang_out / 'journal.mes').write_bytes(compile_journal_mes(journal_dir / f'{lang}.json'))

    tm_out = base / 'townmap/tutorial-island'
    tm_out.mkdir(parents=True, exist_ok=True)
    for item in sorted((resources / 'townmap/tutorial-island').iterdir()):
        if item.is_file():
            shutil.copy2(item, tm_out / item.name)

    snd_out = base / 'sound/tutorial'
    snd_out.mkdir(parents=True, exist_ok=True)
    for clip in expected_voice_ids(lessons_source):
        shutil.copy2(resources / 'sound' / f'{clip}.mp3', snd_out / f'{clip}.mp3')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--resources', type=Path, default=ROOT / 'resources/tutorial')
    parser.add_argument('--journal', type=Path, default=ROOT / 'tutorial/journal')
    args = parser.parse_args()
    build(args.output, args.resources, args.journal)
