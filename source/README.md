# Renaissance engine source

This directory contains the complete engine used by Arcanum Renaissance.
It is derived from **Arcanum Community Edition**, created by **Alex Batalov**
with its contributors:

- Original project: https://github.com/alexbatalov/arcanum-ce
- Original revision: [a7ff41b300ef712f0e7d088183a3d08957110cdd](https://github.com/alexbatalov/arcanum-ce/commit/a7ff41b300ef712f0e7d088183a3d08957110cdd)
- License: [Sustainable Use License, version 1.0](LICENSE.md)

**This is a modified version maintained by the Renaissance project.** It includes
Renaissance's interface, gameplay changes, diagnostics, tools integration and
platform packaging. It is not an unmodified release of Arcanum Community Edition.
Original copyright and license notices are retained. Components with their own
licenses retain those notices in their respective directories.

The engine source is tracked directly in this repository. A separate checkout
or download of Arcanum Community Edition is not required to build it. External
dependencies retain their own download and build configuration.

## Layout

- `src/game/`: world simulation, objects, scripts, combat and save handling.
- `src/ui/`: native and Renaissance interface, rendering and input.
- `first_party/`: TIG platform layer and Bink/Miles compatibility code.
- `third_party/`: dependency configuration and bundled headers with their notices.
- `android/`, `ios/`, `macos/`: inherited platform resources. Their presence does
  not imply that Renaissance supports every upstream platform.

Use the repository-root build scripts and [development guide](../docs/development.md).
Supported platforms and installation instructions are in the [project README](../README.md).
