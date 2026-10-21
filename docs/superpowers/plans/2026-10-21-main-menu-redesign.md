# Main menu redesign implementation plan

> Execute inline with superpowers:executing-plans. Do not commit or publish.

**Goal:** Implement the approved wood and bronze main menu on branch `redesign`.

**Architecture:** Load separate main-menu and blank single-player illustrations. Draw localized MES labels and interactive plaque states using the same aspect-preserving transform for artwork and hit areas. Reuse sliced bronze plaques and carved borders for load/save, character selection, creation and the creation character sheet. Keep the existing menu renderer as a fallback and for other screens.

**Tech stack:** C, SDL3, stb_truetype, PNG.

**Spec:** User-approved image in `review/main-menu-concept/menu-preview-v1.png` and conversation approval of 2026-10-21.

## Constraints

- Do not modify the parent game installation or saves.
- Use installed MES entries; no extracted game text in Git.
- Keep generated captures and extracted references under ignored `review/`.
- No commit, tag, release or upload.
- Generated main-menu artwork is 1672 × 941; the single-player asset is 1673 × 940. Do not describe either as native 4K.

## Review focus

- Shared transform must keep clicks aligned at 16:9, 16:10 and 4:3.
- Long localized labels and accents must fit inside the bronze borders.
- Hover and pressed states must not leave a permanently selected first button.
- Missing artwork must retain the existing working menu.
- Options, single-player, credits and quit must keep existing action routing.

## Tasks

- [x] Save the textless image as `game/modern-ui/main-menu.png`, allow it in `.gitignore`, and include it in the shared packaging asset list. Save the generation prompt in `tools/art/`.
- [x] In `modern_ui.c`, add texture lifetime handling, a dedicated book font, fitted plaque labels, and a main-menu renderer that reuses existing button actions. Use source-space plaque rectangles for drawing and hit registration.
- [x] Verify rendering geometry and localized fitting, run existing menu/input checks, compile the native macOS target and test menu transitions in an isolated copy. Install the executable atomically in the repository-local app. Review the final diff.

## Execution notes

- Existing workspace is clean; the user explicitly requested branch `redesign` here, so no separate worktree is required.
- Native macOS compilation uses the existing `build/macos-arm64` CMake target; `build.sh` installs Linux `.so` files. Avoid invoking the release packager during this design iteration.

## Single-player extension

- [x] Generate a blank matching frame with the built-in imagegen tool; store `single-player.png` and its prompt. Include the asset in the shared package list.
- [x] Skin single-player navigation, new-game choice, load/save, pre-generated and custom characters, and the creation character sheet. Preserve engine actions and MES labels, red selection and native navigation sounds.
- [x] Verify the real engine on `build/redesign-game`: navigation, back, character cycling, race/background changes, character-sheet entry, save selection, held mouse press and delayed release. Check viewport bounds at 1280×720, 1024×768, 1280×800 and 3840×2160.
- [x] Check native sound routing, keyboard capture, save menu, localization and macOS packaging tests. Native window automation remains unavailable (`cgWindowNotFound`); engine-rendered captures and input tests cover the isolated copy without audible sound verification.

## In-game pause extension

- [x] Share the navigation artwork with the normal, locked and no-load pause menus, preserving native button indices and localized labels. Extend carved panels, plaques and red selection to Options and pause confirmations.
- [x] Build the native macOS target and verify save/load navigation, every Options tab, restricted pause variants, and Escape holds with delayed releases on the isolated engine instance.
- [x] Verify the native window on the isolated app: load a copied save, open Escape, traverse Options, return to play, open/cancel saving, and open/cancel the quit confirmation. Physical key-hold duration is not exposed by the automation tool; delayed events are covered by the engine harness.
- [x] Run keyboard capture, menu feedback, option controls, save menu and localization tests; inspect rendered menus at 1280×720, 1024×768, 1280×800 and 3840×2160. Install locally with an atomic executable replacement and verified signature; original save hashes unchanged.

## In-game header and footer

- [x] Inspect original interface artwork 185 and 184, then generate a wood, iron and brass atlas with imagegen. Store `hud-atlas.png` (1536×1024) and its prompt; include the resource in shared packaging.
- [x] Apply the atlas to normal and tactical bars, retaining existing button actions and hitboxes, hotkeys, localized labels and dynamic health/fatigue levels. Preserve the previous renderer if the atlas is missing.
- [x] Compile and verify normal/tactical rendering at 1280×720, 1024×768, 1280×800 and 3840×2160. Compare all 20 normal HUD action IDs and hitboxes against the fallback. Check inventory and spell access and return through the isolated native window.
- [x] Run HUD transitions/messages, hotkey assignment, keyboard capture, localization and macOS packaging checks. `test_combat_resources.py` cannot compile its existing sound stub because it lacks `modern_compat_get`; no combat-resource logic changed here.
- [x] Install the executable atomically in the repository-local macOS app, verify its signature and confirm all 32 original save hashes are unchanged.
