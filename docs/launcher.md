# Renaissance launcher

The launcher is included in the Windows setup and in the Windows x64, Linux x64
and macOS ARM64 ZIPs. Shortcuts, the macOS application and the Play scripts open
it. Starting the engine directly redirects to the launcher. A launch marker
allows the engine to continue after an update, an explicit skip or an offline
check; it is a routing convention, not authentication.

The window supports English and French. Release notes come from the published
GitHub release and retain their original language. Players can scroll the notes,
turn off automatic game startup, or play without updating. Checking the release
service has a six-second timeout. The game has no separate update notification.

## Update boundaries

Only newer stable releases from `cdomergue/arcanum-modern` are eligible. The
launcher selects its platform ZIP, checks its SHA-256 against `SHA256SUMS.txt`,
then verifies the target, version and every file in the package manifest.
Extraction rejects traversal, duplicate paths, links, oversized payloads and
paths outside the distributed mod resources. Downloads use HTTPS and a restricted
set of GitHub asset hosts.

Game archives, saves, preferences, generated maps, UAP installation state and
custom portraits/backgrounds are outside the update paths. Existing files that
were never owned by the installed manifest are not overwritten. An obsolete
distributed file is removed only if it still matches its previous package hash.

A journal and complete file backups support rollback and recovery after an
interrupted installation. Executables remain present until their replacements
are ready. Windows runs a private cached copy of the launcher so it can replace
the installed launcher. Unix replacements preserve executable permissions.
Installation is not cancellable during replacement; downloading can be skipped.
A recovery failure stops game startup instead of opening an inconsistent install.

The launcher stores its language and automatic-start preference in
`modern-ui/launcher.json`, cached release text in
`ArcanumModern/launcher-release.json`, and diagnostics in
`ArcanumModern/launcher.log`. These are not distributed update payloads.

## Building and verification

`tools/build-launcher.py --target windows|linux|macos --output PATH` uses the same
pinned Go toolchain as the UAP helper. Linux and macOS also include the small
`renaissance-launcher-ui` CMake target, linked with SDL, and the launcher artwork.
Windows uses native Win32 controls. No browser, Python installation or separate
SDL installation is required by players.

Run Go tests in `tools/launcher`. They exercise verified downloads, corrupt
archives, personal-file preservation, rollback, interrupted recovery and Unix
permissions. Windows tests also exercise real file locks, self-replacement and
game arguments. Set `RENAISSANCE_LAUNCHER_PACKAGE` to a ZIP to verify the complete
distributed payload. Set `RENAISSANCE_LAUNCHER_BINARY` and
`RENAISSANCE_ENGINE_BINARY` for the Windows bootstrap and real-engine redirect
tests. These tests use disposable fixtures.

`tests/test_launcher_ui.py PATH_TO_RENDERER` exercises the SDL interface in both
languages without a desktop. Release jobs run the launcher tests on their native
platforms, validate archive contents and permissions, and retain the installer
tests for shortcuts and uninstall behavior. macOS packaging signs the launcher,
renderer, engine and application bundle with ad-hoc signatures, then verifies
the bundle before creating the ZIP.
