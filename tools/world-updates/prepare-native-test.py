"""Create a new disposable native update test installation.

Only immutable DAT archives are hard-linked. Saves, settings and loose resources
are copied, so tests never write through to the source installation.
"""
from pathlib import Path
import os
import shutil
import sys


def prepare(source, destination):
    source, destination = Path(source).resolve(), Path(destination).resolve()
    if not (source / '.renaissance-house-test').is_file():
        raise ValueError('Source must be a marked disposable test installation')
    if destination.exists() or source in destination.parents:
        raise ValueError('Destination must be new and outside the source')

    def copy(src, dst):
        if Path(src).suffix.lower() == '.dat':
            try:
                os.link(src, dst)
                return dst
            except OSError:
                pass
        return shutil.copy2(src, dst)

    def ignore(directory, names):
        if Path(directory).name.casefold() == 'save':
            return [name for name in names if not name.startswith(('Slot0002', 'Slot0088'))]
        return [name for name in names if name.casefold() in ('tigcache', 'current', 'world-updates')]

    shutil.copytree(source, destination, copy_function=copy, ignore=ignore)
    print(destination)


if __name__ == '__main__':
    if len(sys.argv) != 3:
        raise SystemExit('Usage: prepare-native-test.py MARKED_TEST_GAME NEW_COPY')
    prepare(*sys.argv[1:])
