#!/usr/bin/env bash
set -euo pipefail
arcanum_project=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
arcanum_revision=a7ff41b300ef712f0e7d088183a3d08957110cdd
arcanum_source=${ARCANUM_ENGINE_SOURCE:-/tmp/arcanum-ce-source}
arcanum_patch="$arcanum_project/patches/engine.patch"

if [[ ! -f "$arcanum_project/source/CMakeLists.txt" ]]; then
    arcanum_temp=$(mktemp -d /tmp/arcanum-source.XXXXXXXX)
    trap 'rm -rf -- "$arcanum_temp"' EXIT
    if ! git -C "$arcanum_source" cat-file -e "$arcanum_revision" 2>/dev/null; then
        arcanum_source="$arcanum_temp/upstream"
        git clone https://github.com/alexbatalov/arcanum-ce.git "$arcanum_source"
    fi
    mkdir -p "$arcanum_temp/export" "$arcanum_project/source"
    git -C "$arcanum_source" archive "$arcanum_revision" | tar -x -C "$arcanum_temp/export"
    # Export contains only upstream files; our new modules have distinct names.
    cp -a "$arcanum_temp/export/." "$arcanum_project/source/"
fi

arcanum_git=(git -c "safe.directory=$arcanum_project" -C "$arcanum_project")
if "${arcanum_git[@]}" apply --directory=source --reverse --check "$arcanum_patch" 2>/dev/null; then
    exit 0
fi
"${arcanum_git[@]}" apply --directory=source --check "$arcanum_patch"
"${arcanum_git[@]}" apply --directory=source "$arcanum_patch"
