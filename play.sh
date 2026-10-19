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
arcanum_auto_renderer=0
if [ -z "${SDL_RENDER_DRIVER:-}" ]; then
    arcanum_auto_renderer=1
fi
export SDL_RENDER_DRIVER="${SDL_RENDER_DRIVER:-vulkan}"
if [ "$SDL_RENDER_DRIVER" = software ]; then
    export SDL_VIDEO_DRIVER="${SDL_VIDEO_DRIVER:-x11}"
    export SDL_FRAMEBUFFER_ACCELERATION="${SDL_FRAMEBUFFER_ACCELERATION:-0}"
fi
arcanum_status=0
./arcanum-modern "$@" > modern-launch.log 2>&1 || arcanum_status=$?
if [ "$arcanum_auto_renderer" -eq 1 ] && [ "$arcanum_status" -eq 139 ] \
    && ! grep -q '^Arcanum Modern: startup complete$' modern-launch.log; then
    cp modern-launch.log modern-launch-vulkan.log
    printf '%s\n' 'Startup crashed with Vulkan; retrying once with X11 software rendering.' >&2
    export SDL_VIDEO_DRIVER=x11
    export SDL_RENDER_DRIVER=software
    export SDL_FRAMEBUFFER_ACCELERATION=0
    exec ./arcanum-modern "$@" > modern-launch.log 2>&1
fi
exit "$arcanum_status"
