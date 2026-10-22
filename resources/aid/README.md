# Native AID descriptions

`descriptions.json` adapts SCARaw's **Arcanum Improved Descriptions** under the
MIT license in `LICENSE.txt`. The upstream URL and pinned revision are recorded
in the JSON. Both French and English are embedded in the executable; nothing is
downloaded at runtime. Existing game MES files, maps and saves are untouched.

The catalog contains the nonempty description changes relative to the first
uploaded base `item_effect.mes` and `gameitemeffect.mes`, plus entry 31197 used
by the unique-item fixes. It excludes the unchanged names of NPCs, locations
and objects. No original game archives, extracted game text or MOB files are
distributed. The `variants` list records the item GUID, description ID and
effect ID from the upstream unique-item fixes; identity is checked at display
time instead of replacing objects or clearing map caches.

Templates are UTF-8. `@sN@`, `@kN@`, `@tN@`, `@pN@`, `@iN@` and `@dN@` resolve
through the installed game's stat, skill, technological discipline, spell,
interface and description accessors. Translate new AID prose, but use those
references for existing game terminology. Numeric values and identification
rules are preserved. Parenthesized bonuses depend on magical aptitude; strength
pairs give the requirement with the other hand free / occupied. Weapon speed
and character speed are distinct.

After editing the catalog, run:

```sh
python3 tools/generate-aid.py
python3 tests/test_aid_descriptions.py
```

The generated `source/src/ui/modern_aid_data.inc` is tracked so normal builds do
not need Python generation, network access or the upstream mod checkout. Only
the Arcanum campaign uses this catalog. Other modules use their installed text.
Rebalanced item mods may have different statistics from AID's reference data.
