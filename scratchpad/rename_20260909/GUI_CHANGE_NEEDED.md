# GUI changes the `.lodl` rename needs — for the lane that owns nifskope.cpp

Written 2026-09-09 by lane RENAME, which owns the writer, the reader, the CLI,
the docs and the harnesses but **not** `src/nifskope.cpp`, `src/nifskope.h`,
`src/nifskope_ui.cpp` or `src/glview.*` (one lane per file, CONSTITUTION 1).

**What moved.** bungo's ruling of 2026-09-09: the whole-worldspace LANDSCAPE
file is `.lodl` (it was `.lodt`), and `.lodt` now names the terrain TEXTURE
sheets, which were `.lodv`. The landscape file's bytes did not change — same
magic `LODT`, same v2 layout — only its extension. The texture container took a
NEW magic, `LDTX`, so each reader refuses the other's file by name.

**Nothing below changes behaviour except which suffix opens the picker.** The
C++ names (`LodtWorldInfo`, `lodtQueryRegion`, `lodtPendingRegion`) did NOT move
and must stay as they are.

---

## 1. `src/nifskope.cpp` — five one-line changes and two comments

| # | line (2026-09-09) | from | to |
|---|---|---|---|
| 1 | ~164 | `{ "Landscape Terrain", "lodt" },` | `{ "Landscape Terrain", "lodl" },` |
| 2 | ~9677 | `if ( file.endsWith( QStringLiteral( ".lodt" ), Qt::CaseInsensitive ) ) {` | `if ( file.endsWith( QStringLiteral( ".lodl" ), Qt::CaseInsensitive ) ) {` |
| 3 | ~10076 | `} else if ( f.suffix().compare( QLatin1String( "lodt" ), Qt::CaseInsensitive ) == 0 ) {` | `} else if ( f.suffix().compare( QLatin1String( "lodl" ), Qt::CaseInsensitive ) == 0 ) {` |
| 4 | ~10119 | `\|\| f.suffix().compare( QLatin1String( "lodt" ), Qt::CaseInsensitive ) == 0 ) ) {` | `\|\| f.suffix().compare( QLatin1String( "lodl" ), Qt::CaseInsensitive ) == 0 ) ) {` |
| 5 | ~10151 | `\|\| curFile.suffix().compare( QLatin1String( "lodt" ), Qt::CaseInsensitive ) == 0 )` | `\|\| curFile.suffix().compare( QLatin1String( "lodl" ), Qt::CaseInsensitive ) == 0 )` |

Two comments in the same file say `.lodt` about the landscape file and should
say `.lodl`: the block at ~9674 (*"A .lodt is the same species of file"*) and
the two mentions inside *"A GENERATED DOCUMENT IS NOT A MODIFIED ONE"* at ~10104
and ~10109 (*"The .btd and .lodt routes above"*, *"sends .btd and .lodt to Save
As"*).

**Do NOT add `lodt` back to `filetypes` for the texture sheets.** They are DDS
payloads in a container, not meshes, and there is no viewer route for them; the
CLI answers questions about them with `lodgen --lodt-check FILE.lodt`.

---

## 2. `src/nifskope_ui.cpp` — one line, and it is a live harness failure until it lands

Line ~27306, inside the `WW_LODGEN_TEST` self-test:

```cpp
							check( "with one, the panel says what it will write",
								gen->isEnabled() && summary->text().contains( QLatin1String( ".lodt" ) )
								&& summary->text().contains( QLatin1String( "HeightMap" ) ) );
```

must become

```cpp
							check( "with one, the panel says what it will write",
								gen->isEnabled() && summary->text().contains( QLatin1String( ".lodl" ) )
								&& summary->text().contains( QLatin1String( "HeightMap" ) ) );
```

**Why it cannot wait.** `src/lodgenmanager.cpp` (this lane's file) now builds the
summary sentence as `Terrain\<ws>.lodl (about N MB)`, so the string this check
looks for is gone. Until the line above changes,
`tests/spells/lod_generation.sh` fails on exactly that one check — an expected
red, not a regression in the panel. Every other check in that harness is
unaffected: the panel's objectNames (`LodgenLodtCheck`,
`LodgenLodtFullRadio`, `LodgenLodtAoOnlyRadio`, `LodgenLodtSection`) and the
QSettings key `LodGeneration/lodt` were deliberately left alone by this lane,
precisely so nothing else in that file has to move.

---

## 3. What this lane already did, so the two halves do not collide

* `src/lodtfile.{h,cpp}` — writes `Terrain/<EDID>.lodl`; `LODL_MAGIC` is public
  in the header now; `LodtFile::open` refuses a terrain texture file by name.
* `src/io/lodvfile.{h,cpp}` — writes `.lodt` with `LODTEX_MAGIC` (`LDTX`);
  `lodvValidate` refuses `LODT` (the landscape file) and the retired `LODV` by
  name.
* `src/btdterrain.{h,cpp}` — prose and the two environment overrides
  (`WW_LODL_REGION`, `WW_LODL_PLANE`; the `WW_LODT_*` spellings are refused
  aloud).
* `src/nifcli.cpp` — command `lodl`, flags `--lodl` and `--lodt-check`; the
  retired `lodt` command, `--lodt` and `--lodv-check` refuse by name.
* `src/lodgenmanager.cpp` — the panel's visible strings only.
