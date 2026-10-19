#!/usr/bin/env python3
"""Check every engine quest state against the journal's exclusive filters."""
from pathlib import Path
import os, re, subprocess, tempfile
root = Path(__file__).resolve().parents[1] / 'source/src'
source = (root / 'ui/modern_ui.c').read_text()
function = re.search(r'static bool quest_filter_matches\(.*?\n\}', source, re.S).group()
enum = re.search(r'typedef enum QuestState \{.*?\} QuestState;', (root / 'game/quest.h').read_text(), re.S).group()
code = '#include <stdbool.h>\n#include <assert.h>\n' + enum + '\n' + function + r'''
int main(void) {
    const int expected[] = {0,4,1,1,2,3,3};
    for(int state=0;state<QUEST_STATE_COUNT;state++) {
        assert(quest_filter_matches(0,state)==(state!=QUEST_STATE_UNKNOWN));
        int matches=0;
        for(int filter=1;filter<=4;filter++) {
            bool result=quest_filter_matches(filter,state);
            assert(result==(filter==expected[state])); matches+=result;
        }
        assert(matches==(state!=QUEST_STATE_UNKNOWN));
    }
}
'''
with tempfile.TemporaryDirectory(prefix='arcanum-quest-filters-') as directory:
    p=Path(directory);(p/'test.c').write_text(code)
    subprocess.run([os.environ.get('CC','cc'),'-std=c17','-Wall','-Wextra',str(p/'test.c'),'-o',str(p/'test')],check=True)
    subprocess.run([str(p/'test')],check=True)
print('PASS: all quest states, pending turn-in, rival completion, exclusive filters')
