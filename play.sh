#!/bin/sh
set -eu
cd -- "$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)/game"
if [ "$#" -eq 0 ]; then
    set -- -geometry=1280x720
fi
for arcanum_arg in "$@"; do
    if [ "$arcanum_arg" = -window ]; then
        export ARCANUM_IGNORE_DISPLAY_SETTINGS=1
    fi
done
exec ./renaissance-launcher "$@"
