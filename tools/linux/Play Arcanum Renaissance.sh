#!/bin/sh
set -eu
cd -- "$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
# UAP capitalizes archive names; Linux filesystems distinguish their case.
arcanum_data_found=false
for arcanum_data in [aA][rR][cC][aA][nN][uU][mM]1.[dD][aA][tT]; do
    if [ -f "$arcanum_data" ]; then
        arcanum_data_found=true
        break
    fi
done
if [ "$arcanum_data_found" != true ] || [ ! -f modern-ui/Book.ttf ]; then
    printf '%s\n' 'Extract the complete ZIP into your Arcanum game folder, beside arcanum1.dat.' >&2
    exit 1
fi
if [ "$#" -eq 0 ]; then
    set -- -geometry=1280x720
fi
for arcanum_arg in "$@"; do
    if [ "$arcanum_arg" = -window ]; then
        export ARCANUM_IGNORE_DISPLAY_SETTINGS=1
    fi
done
# Keep launcher graphics independent of the game renderer.
export RENAISSANCE_LAUNCHER_SOFTWARE="${RENAISSANCE_LAUNCHER_SOFTWARE:-1}"
exec ./renaissance-launcher "$@"
