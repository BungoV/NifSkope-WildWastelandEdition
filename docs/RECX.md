# Record extension files (.recx)

A `.recx` file adds data to a record that lives in a plugin (.esm, .esp, .esl) without editing the plugin. The file
is plain JSON (UTF-8, no BOM). NifSkope, CORE and FO4CS share this format, so each can read the others' files.

## Where the files go

```
Data\Plugins\<Provider>\<Category>\<EditorID>.recx
```

- `Provider` is the mod that ships the data, by its plain name with no extension: `CORE`, `FO4CS`, `MyMod`. It is
  not the plugin that owns the record, so two mods adding data to the same record never share a path.
- `Category` is the record kind, plural and capitalized: `Weathers`, `Weapons`, `Ammunition`, ...
- The file name is the target record's EditorID, spelled exactly as its plugin spells it. A record with no EditorID
  is named by its 8-digit local id (`0001A2B3.recx`).
- Example: `Data\Plugins\FO4CS\Weathers\CommonwealthFoggy.recx`.

The folder and file names are there for people. What a file changes is decided only by the entries inside it.

## Contents

```json
{ "entries": [
  { "name": "CommonwealthFoggy", "weather": "Fallout4.esm|0x001C3473",
    "groundFog": { "thickness": 600, "strength": 4, "hours": { "5": { "strength": 5 } } } }
] }
```

- `entries` is a list. `name` is the EditorID, for people.
- Every entry for a single record must name that record inside the file. It does this with the record key named
  after the kind (`weather` for Weathers), written as `"Plugin.ext|0xLOCALID"`. LOCALID is the id inside the plugin that
  defines the record, 8 hex digits (`0x001C3473`), without the load-order byte. A bare EditorID is also accepted
  there.
- Family entries match many records at once and carry no record key. They use `editorId` with a wildcard, for
  example `"editorId": "CommonwealthClear*"`. `editorId` may also be an exact EditorID.
- After the match keys come the feature blocks, each named for what it does (`groundFog`). A reader ignores any
  block it does not use, so new blocks never break old readers. The names `climate` and `atmosphere` are reserved
  for later weather blocks.
- No game data is copied into a `.recx` file. The values are ours.

## How a reader picks an entry

Every provider's files are read: the reader walks `Plugins\*\<Category>\*.recx`.

1. Precedence, per block: a form match (`Plugin.ext|0xLOCALID`), then an exact EditorID, then a wildcard. After those
   comes the reader's own derivation from the record itself, used when no file carries the block.
2. Several providers for the same record merge block by block. At the same rank the provider later in the merge order
   wins that block only, and every other block is kept. The merge order:
   - a provider named like a loaded plugin (`CORE` for CORE.esm, .esp or .esl) takes that plugin's place in the load
     order;
   - a provider with no plugin (`FO4CS`) comes after all plugins, in name order.
   Every overridden claim is logged with its winner.
3. Two entries at the same rank from the same provider are a conflict. The conflict is logged, and the winner is
   fixed: the first file by path, then the first entry in it.
4. Files are read again when they change, so edits show up without a restart.
5. The census reports what was applied: the rank that matched (`form`, `editorId`, `wildcard`) and the file.

NifSkope looks for `Plugins\` under these folders: each loaded plugin's folder (a mod manager's mod folders), the
game's Data, and the files NifSkope ships. The same provider file under two of them is read once (the first). The
environment variable `WW_RECX_ROOT` replaces the whole list with one folder (for tests).

## groundFog (Weathers)

A layer of denser air that hugs the ground, added on top of the air derived from the weather record.

| key | meaning |
| --- | --- |
| `thickness` | height scale in game units: the layer's density falls by e (about 2.7 times) per `thickness` units of height |
| `strength` | the layer's density at its floor, as a multiple of the record-derived air's density |
| `hours` | optional: `{ "<hour>": { "thickness": .., "strength": .. } }`. Values are interpolated linearly between the keyed hours, around the clock. One key means that value all day. |

NifSkope ships `Plugins\FO4CS\Weathers\` with a file for every weather in the base game and the official add-ons
(`tools/recx_weathers.py` writes them). Their values follow one rule from each weather's own fog distances, so
nothing is tuned by hand: the denser the weather's haze at street distance, the denser and deeper its floor layer.
The night values are keyed at 02:00 and the day values at 14:00.

The layer depends only on height. That keeps it continuous out to distant mountains and far terrain with no seam,
and its column has a closed form. Its floor is the weather's near height band. With no file, or with a strength of
0, the air is the record-derived air alone.

NifSkope's air has two test switches, set in `WW_CELL_AIR_RED`:
- `nosidecar` ignores the files;
- `noground` draws the far band alone, which is the air from before this layer was added.
