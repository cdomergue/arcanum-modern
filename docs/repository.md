# Repository guide

Renaissance keeps the engine, interface, authoring tools and their tests in one
repository. Start with the [development guide](development.md) to build a target
platform. Read [engine provenance](../source/README.md) for upstream authorship
and the original revision.

## Source and resources

| Directory | Contents |
| --- | --- |
| `source/` | Complete engine source, including Renaissance modules and platform support. |
| `shared/` | Code shared between the engine and authoring tools. |
| `resources/` | Versioned content, templates, translations and license notices. |
| `game/modern-ui/` | Versioned interface artwork and fonts; local settings are ignored. |
| `tools/` | Build, packaging, content-generation and diagnostic programs. |
| `tests/` | Regression tests and standalone native test harnesses. |
| `docs/` | User guides, contributor references and diagnostic procedures. |
| `.github/workflows/` | Platform builds and release validation. |

`build/`, `dist/`, local game installations, saves and generated reports are
working data. They do not belong in Git. Keep builds out of `source/`.

## Tools

The root `build*.sh` scripts compile platform packages. `tools/package-*.py`
assembles distributions; `tools/build-ffmpeg.py` manages the movie dependency.
`tools/launcher/` contains the updater, and `tools/uap/` the UAP integration.

`tools/worlded/` and `tools/script-editor/` implement and support the authoring
tools. `tools/house/`, `tools/tutorial/` and `tools/world-updates/` generate or
validate content. `generate-*`, `map_archive.py` and the map-cache utilities
support repeatable resource generation and inspection.

`tools/check-*`, `tools/build-*-check.py` and test programs inside tool directories
are part of the verification suite. Some require installed game resources or a
marked disposable game copy; they are not obsolete merely because the release
workflow does not run every probe. Preserve these alongside the tests in `tests/`.

## References

- [Campaign compatibility and checks](campaign-compatibility.md)
- [Native engine diagnostics](engine-diagnostics.md)
- [Save and crash diagnostics](save-diagnostics.md)
- [Combat diagnostics](combat-diagnostics.md)
- [Follower animation diagnostics](follower-diagnostics.md)
- [WorldED build instructions](worlded-build.md)

Documentation describes current behavior, interfaces, constraints and reusable
verification procedures. Keep session logs, completed implementation plans and
machine-specific test reports in local working directories rather than the guides.
