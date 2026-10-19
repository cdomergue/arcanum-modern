# Repository Guidelines

## Scope

This repository contains the Arcanum Modern interface mod. The local `game/`
installation and the upstream engine in `source/` are working copies, not content
to import wholesale into Git. Never change the parent installation or its saves.

## Source Organization

Our modules are `source/src/ui/modern_ui.*` and `modern_graphics.*`.
Changes to existing upstream engine files are represented by `patches/engine.patch`.
`prepare-source.sh` restores the pinned upstream revision and applies that patch.
`game/modern-ui/` contains the mod's fonts, background, and local preferences.

## Development and Verification

Use `./build.sh` to compile and install the executable atomically. After editing an
existing upstream file, run `python3 tools/update-engine-patch.py` before building.
`ARCANUM_ENGINE_SOURCE` points to the upstream Git checkout used by that script.
Do not include unrelated upstream changes in the patch.

Run `python3 tests/test_keyboard_capture.py` for input routing changes. Test live
menu transitions on an isolated game copy. Include physical key holds and delayed
releases when checking keyboard behavior; instantaneous synthetic taps can hide
event-order bugs. Identify the test process and window before automating input.

## Git and Documentation

Track only the mod's code, integration patch, scripts, tests, documentation, and
interface resources. Keep game archives, executables, saves, generated captures,
logs, and personal settings excluded. Inspect the exact staged list before any
commit. Do not commit unless requested.

Batch requested changes and finish validation before creating a release tag or
starting release packaging. Publish one release for the completed set of changes.

The README is a user and contributor guide. Keep it focused on installation,
features, controls, configuration, build instructions, and limitations. Do not add
chronological progress reports, descriptions of our own bug fixes, or manual test
diaries. Report task-specific changes and test results in the conversation.

## Localization

Reuse the installed game's MES entries and engine text accessors for labels,
descriptions and feedback. Do not hardcode translated copies or track extracted
game text. Keep encoding conversion at the presentation boundary and check long
translations, accented names and scrollable descriptions.
