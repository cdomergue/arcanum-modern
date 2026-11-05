"""Narrow campaign guards: interrupted reward and one mechanical instance."""
import os
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
header = (root / 'source/src/game/modern_campaign_fixes.h').as_posix()
code = r'''
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "HEADER"
static int gold, quest;
static void visit(int line, const char* original) {
    const char* action=modern_campaign_reward_actions("Arcanum",1565,line,quest,original);
    if (action[0]=='$') gold+=atoi(action+2);
    if (strstr(action,"qu1098 4")) quest=4;
}
int main(void) {
    const char* normal="$$7000, qu1098 4";
    const char* best="$$11000, rp 1044, qu1098 4";
    gold=0;quest=3; visit(50,normal); assert(gold==7000 && quest==4);
    /* Interrupt before the player's later local-flag action, then re-enter. */
    visit(50,normal); assert(gold==7000);
    gold=0;quest=3; visit(73,best); visit(73,best); assert(gold==11000);
    /* Native saved completed state alone is sufficient, no migration ledger. */
    gold=123;quest=4;visit(50,normal);assert(gold==123);
    assert(modern_campaign_reward_actions("Other",1565,50,4,normal)==normal);
    assert(modern_campaign_reward_actions("Arcanum",1564,50,4,normal)==normal);
    assert(modern_campaign_reward_actions("Arcanum",1565,51,4,normal)==normal);
    assert(modern_campaign_reward_actions("Arcanum",1565,50,3,normal)==normal);
    const char* modified="$$8000, qu1098 4";
    assert(modern_campaign_reward_actions("Arcanum",1565,50,4,modified)==modified);
    assert(modern_campaign_reward_actions("Arcanum",1565,50,4,NULL)==NULL);
    puts("PASS: first reward, interrupted re-entry, native completed save, unrelated and modified content");
    unsigned char guid[] = {0x47,0xF7,0xC5,0xDE,0x80,0x15,0xD4,0x11,0x8F,0x1D,0,0xE0,0x29,0x20,0x22,0x0C};
    assert(modern_campaign_critter_flags("Arcanum",28343,guid,0x2003)==0x20002003);
    assert(modern_campaign_critter_flags("Arcanum",28343,guid,0x20002003)==0x20002003);
    assert(modern_campaign_critter_flags("Other",28343,guid,0x2003)==0x2003);
    assert(modern_campaign_critter_flags("Arcanum",28344,guid,0x2003)==0x2003);
    assert(modern_campaign_critter_flags("Arcanum",28343,NULL,0x2003)==0x2003);
    assert(modern_campaign_critter_flags(NULL,28343,guid,0x2003)==0x2003);
    guid[0]++;
    assert(modern_campaign_critter_flags("Arcanum",28343,guid,0x2003)==0x2003);
    puts("PASS: only the identified mechanical instance changes; idempotent native flag; unrelated content unchanged");
}
'''.replace('HEADER', header)
with tempfile.TemporaryDirectory(prefix='renaissance-reward-') as tmp:
    source=Path(tmp)/'test.c'; binary=Path(tmp)/'test'
    source.write_text(code,encoding='utf-8')
    subprocess.run([os.environ.get('CC','cc'),'-std=c17','-Wall','-Wextra','-Werror',str(source),'-o',str(binary)],check=True)
    subprocess.run([str(binary)],check=True)
