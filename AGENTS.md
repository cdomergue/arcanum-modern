# Repository Guidelines

## Scope

This repository contains Arcanum Renaissance and its complete engine source in
`source/`. The local `game/` installation is a working copy; only the selected
interface resources in `game/modern-ui/` belong in Git. Deploying validated builds
and mod resources to the main game installation at `/mnt/sshd/Jeux/Arcanum/` is
authorized. Never modify,
overwrite, rename, or delete its saves. Any testing that writes saves must use an
isolated game copy, not the main installation.

## Source Organization

Engine and Renaissance modules are versioned directly in `source/`. UI code
lives in `source/src/ui/`, gameplay code in `source/src/game/`, and platform
support in `source/first_party/`. Preserve the authorship, revision reference
and license notices documented in `source/README.md`.
`game/modern-ui/` contains the mod's fonts, background, and local preferences.

## Development and Verification

For each newly reported bug whose existing diagnostics do not explain the failure,
add focused persistent logging as part of the investigation or fix. Record useful
context, timing and outcomes; bound log growth and exclude personal save contents.
Test the logging and do not present an unconfirmed cause as established.
For save failures and crash reports, read `docs/save-diagnostics.md` first. It
documents log locations, fields, rotation, limitations and isolated verification.

Edit engine files directly and use `./build.sh` to compile and install the
executable atomically into the repository-local `game/` directory. Compilation
uses the checked-in source; no separate Arcanum CE checkout is required.

Run `python3 tests/test_keyboard_capture.py` for input routing changes. Test live
menu transitions on an isolated game copy. Include physical key holds and delayed
releases when checking keyboard behavior; instantaneous synthetic taps can hide
event-order bugs. Identify the test process and window before automating input.

## Git and Documentation

Track engine and mod code, scripts, tests, documentation, and
interface resources. Keep game archives, executables, saves, generated captures,
logs, and personal settings excluded. Inspect the exact staged list before any
commit. Do not commit unless requested.

Batch requested changes and finish local validation before release preflight.
Validate complete packages before creating the release tag, as described below.
Publish one release for the completed set of changes.

The README is a user and contributor guide. Keep it focused on installation,
features, controls, configuration, build instructions, and limitations. Do not add
chronological progress reports, descriptions of our own bug fixes, or manual test
diaries. Report task-specific changes and test results in the conversation.

## Release Preparation

- Run `.github/workflows/release.yml` with `workflow_dispatch` on the candidate
  branch before tagging. Record the candidate commit and wait for Linux, macOS,
  Windows, Windows installation, WorldED and Script Editor checks to pass. The
  dispatch run builds validation packages without publishing a release.
- A Windows build or portable Python test is not proof of macOS compatibility.
  Validate the actual packaged launcher, movie decoder and native helpers on
  their target OS. Keep installer install/reinstall/uninstall tests on disposable
  CI runners; never bypass their runner guard on a personal game installation.
- Inspect the actual uploaded artifact layout when paths change. Multiple upload
  directories retain paths relative to their common ancestor. Consumers must use
  those paths, not assumed basenames.
- Finalize macOS `Info.plist`, including the launcher entry point, before signing
  the engine. Sign nested code first and the outer app last, then require
  `codesign --verify --strict --deep` and packaged runtime checks to pass.
- Keep dependency downloads pinned by SHA-256. Use bounded fallback attempts and
  timeouts, verify before caching, and remove partial files on failure. Never
  bypass TLS or checksums to get a release through. Test recovery and rejection
  of corrupted downloads without relying on the live network.
- Classify failures from their logs before retrying. Retry failed jobs for a
  transient outage; repeated failures require investigating the download or build
  path. Do not restart successful jobs or move the tag merely to retry a network
  failure. Use the existing GitHub connector and normal authenticated Git flow;
  do not extract stored credentials to initialize another tool.
- Tag the validated commit only after preflight is green. Any code change needs
  appropriate revalidation. Never move a published tag or replace its assets;
  post-release documentation updates belong in a separate commit.
- Before publishing, verify all five runtime packages, the three optional
  `ArcanumModern-dependency-sources-*.zip` assets, and `SHA256SUMS.txt`,
  matching tag/manifest revisions, and checksums of the final files. Confirm the
  public release and latest-release endpoint afterwards. Report exactly which
  tests ran and where; do not promise that passing tests proves absence of bugs.
- Keep dependency sources and build scripts out of runtime ZIPs and installers.
  Publish matching FFmpeg/7-Zip sources separately with the same GitHub release;
  preserve licenses and versioned source links in runtime packages. Verify source
  hashes and the presence of all optional source assets before publishing. Do not
  expand source trees into a game installation or a Nexus runtime download.
- For Nexus distribution, provide the exact verified installer or runtime ZIP.
  Check current Nexus requirements; antivirus quarantine requires investigation,
  not speculative repacking. Keep release
  highlights focused on user features; put build incident details in the requested
  report and these contributor instructions.

## Localization

Reuse the installed game's MES entries and engine text accessors for labels,
descriptions and feedback. Do not hardcode translated copies or track extracted
game text. Keep encoding conversion at the presentation boundary and check long
translations, accented names and scrollable descriptions.

## Interface Integration

When adding a command or preference, place it in the existing list or group for
that type of control, alongside related commands. Follow the existing ordering,
layout and interaction patterns instead of adding a separate special-purpose row.
Before declaring UI work complete, inspect the full rendered screen, including
pagination and footer controls, against the user's request.
