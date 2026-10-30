# WorldED Renaissance

WorldED is the C++17 / Qt 6 editor for Arcanum Renaissance. Use it to explore
game resources, edit sectors, build houses, and write scripts and dialogues.
It reads your own Arcanum installation and saves edits in a separate project.
The interface is in English; game resource names keep the installation's language.

- [Using WorldED](../../docs/worlded.md): choose a sector, edit a scene and test it.
- [Building WorldED on Windows and Linux](../../docs/worlded-build.md): dependencies,
  commands, tests and Windows DLL deployment.
- [Creating and selling houses](../../docs/housing.md): export a scene, add a
  catalogue entry and verify purchases and existing saves.
- [Terrain format reference](TERRAIN.md): material pairs and brush rules.

## Programs

| Program | Purpose |
| --- | --- |
| `worlded-explorer` | Graphical editor and asset browser |
| `worlded` | Resource inspection and format utilities |
| `worlded-house-export` | Convert a saved project to additive housing content |

The GUI and housing exporter require `WORLDED_BUILD_GUI=ON` when configuring
CMake. The default build produces only the `worlded` CLI. These programs do not
require compiling the game engine. Playing a test scene requires a compatible
Renaissance executable.

## CLI examples

Run `worlded` without arguments to see its command syntax. These examples assume
the executable is on your PATH; use its build path otherwise.

```sh
worlded list /path/to/Arcanum/modules/Arcanum.dat maps
worlded audit /path/to/Arcanum/modules/Arcanum.dat
worlded audit-assets /path/to/Arcanum/arcanum2.dat
worlded decode /path/to/object.mob
worlded decode /path/to/sector.sec
worlded init /path/to/new-project
worlded import /path/to/Arcanum/modules/Arcanum.dat maps/path/sector.sec /path/to/project
worlded tile /path/to/sector.sec /path/to/project 24 18 123456
```

The last command takes a numeric tile art ID; replace `123456` with an ID from
your installation. `worlded house GAME NEW_PROJECT [WIDTH DEPTH CHESTS]` creates
a standalone room for experiments. To make a property available from the estate
agent, edit an existing main-map sector and follow the housing guide.
