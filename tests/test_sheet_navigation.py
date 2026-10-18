#!/usr/bin/env python3
"""Check the real sheet action routing across independent attributes and tabs."""
from pathlib import Path
import os
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
source = (root / 'source/src/ui/modern_ui.c').read_text()
sheet = (root / 'source/src/ui/modern_sheet.inc').read_text()
a = source.index('        if(action>=651 && action<=653)')
b = source.index('\n    }\n    return false;', a)
actions = source[a:b]
a = sheet.index('static void sheet_select_detail(')
b = sheet.index('\nstatic void sheet_value(', a)
select = sheet[a:b]
code = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
static int sheet_category=1,sheet_page,sheet_selected,description_scroll;
static int sheet_detail_category=1,sheet_detail_index,sheet_degree=-1;
static char message_text[2];
static int changed_category=-1,changed_index=-1,changed_direction;
static void modern_charedit_adjust(int category,int index,int direction) {
    changed_category=category;changed_index=index;changed_direction=direction;
}
''' + select + '\nstatic bool action(int action){\n' + actions + r'''
return false;
}
static void expect_adjust(int button,int category,int index,int direction) {
    changed_category=changed_index=-1;
    assert(action(button));
    assert(changed_category==category && changed_index==index && changed_direction==direction);
}
int main(void) {
    /* Attribute controls remain attributes even with the magic tab selected. */
    action(653); action(675); /* Last of sixteen colleges. */
    assert(sheet_selected==15 && sheet_detail_category==3);
    expect_adjust(625,0,2,1); /* Strength, not a magic purchase. */
    expect_adjust(624,0,2,-1);
    assert(sheet_selected==15 && sheet_category==3);
    expect_adjust(641,3,15,1);expect_adjust(640,3,15,-1);
    action(680);assert(sheet_degree==4 && sheet_detail_index==15);
    /* Fatigue and all remaining attributes have independent increment pairs. */
    expect_adjust(621,0,0,1);expect_adjust(623,0,1,1);expect_adjust(639,0,9,1);
    /* Last technology discipline and seventh schematic stay addressable. */
    action(652);action(667);action(682);
    assert(sheet_detail_category==2 && sheet_detail_index==7 && sheet_degree==6);
    expect_adjust(641,2,7,1);assert(sheet_degree==-1);
    /* Technical skills are distinct from technological disciplines. */
    action(651);action(687);action(663);
    assert(sheet_selected==15 && sheet_detail_category==1);
    expect_adjust(607,1,15,1);expect_adjust(606,1,15,-1);
    action(685);expect_adjust(603,1,5,1); /* Pickpocket. */
    action(686);expect_adjust(605,1,10,1); /* Heal, social group. */
    description_scroll=12;action(690);
    assert(sheet_detail_category==0 && sheet_detail_index==0 && description_scroll==0);
    action(653);assert(sheet_selected==0 && sheet_detail_category==3 && sheet_degree==-1);
    puts("PASS: independent attribute edits, all skill groups, last college/discipline, recipe selection and detail reset");
}
'''
with tempfile.TemporaryDirectory(prefix='arcanum-sheet-routing-') as folder:
    path=Path(folder)/'test.c';exe=Path(folder)/'test';path.write_text(code)
    subprocess.run([os.environ.get('CC','cc'),'-std=c17','-Wall','-Wextra','-Werror',str(path),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
