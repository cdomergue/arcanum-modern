#!/usr/bin/env python3
"""Reject Linux SDL builds that require development-only dlopen library names."""
import re
import sys
from pathlib import Path

def validate(build):
    configs = list(Path(build).glob('_deps/sdl3-build/**/SDL_build_config.h'))
    if not configs:
        raise ValueError('SDL build configuration not found')
    for config in configs:
        text = config.read_text()
        names = re.findall(r'^#define\s+SDL_(?:VIDEO|AUDIO)_DRIVER_\w*DYNAMIC\w*\s+"([^"]+)"', text, re.M)
        invalid = [n for n in names if not re.search(r'\.so\.\d', n)]
        if invalid:
            raise ValueError('SDL requires development-only libraries: '+', '.join(invalid)
                             +'. Repair the SDK library symlinks and reconfigure before building.')
    print('SDL runtime library names verified.')

if __name__ == '__main__':
    try:
        validate(sys.argv[1])
    except (ValueError, IndexError) as error:
        raise SystemExit(str(error))
