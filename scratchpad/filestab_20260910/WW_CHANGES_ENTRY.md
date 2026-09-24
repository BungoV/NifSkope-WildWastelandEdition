### The NIFs tab becomes the FILES tab, and .hkx rows play (lane FILESTAB, 2026-09-10)

bungo, verbatim: *"add hkx files to the NIFs tab, search for them in already set
game folders ... rename 'available NIFs' to 'available files', and rename NIFs
tab to 'Files', then also rename Loaded NIFs to 'Loaded Files', Basically
replace mentions about nifs to generic 'files', because now we'll be able to
browse and open not just nifs, well, we already can, with stuff like bto or
btr"*.

**STATUS: BUILD PENDING.** Lane BUILD8 owned `src/nifskope.cpp`,
`src/nifskope_ui.cpp`, `NifSkope.pro` and `src/gltfexport.*` for the whole of
this lane, so the four files' edits are a REFUSING PATCH SCRIPT
(`scratchpad/filestab_20260910/hookup.py`, 71 edits, every anchor matched as
declared, **nothing written**) rather than applied text. Nothing was built, no
gate was run and no picture was taken. Resume:
`scratchpad/filestab_20260910/PENDING.md`.

**The renames, 60 strings.** The tab is `Files`; the tree root is
`Available files`; the two search fields say `Search files...` and
`Search loaded files...`; the list header is `Loaded files · N`; the four
context menus, the drop menu, the background-load status lines and the
splitter's accessible strings follow. `src/ui/nifskope.ui`'s `BrowserDock` and
its View-menu entry become `File Browser`. What deliberately keeps the word:
the Header tab (it shows a NIF header), the file-dialog filters `NIF files
(*.nif)` (they filter `.nif`), the faceBones and Merge/Flatten actions (they
write a `.nif`), and every C++ identifier, object name and QSettings key —
renaming `nifBrowserFavoritesPath()` would lose a user's favourites for nothing.
The full before/after table is `scratchpad/lane_filestab_report.md` §1.2-1.3.

**The extensions.** `archiveFilterFunction` in `src/nifskope.cpp` was the whole
policy and was three suffixes; it now calls `wwFilesTabAccepts()`
(`src/filestab.cpp`), one list: `.nif .bto .btr .hkx .gltf .lodl .lodt`. The
browser feeds every configured resource root — folder or `.ba2` alike — to the
same `BA2File` indexer, so an `.hkx` inside `Fallout4 - Animations.ba2` lists
exactly like a loose file. **Measured caveat:** the tree additionally keeps only
paths under `meshes/`, so `.hkx` and `.gltf` are reachable (FO4 keeps every clip
under `meshes/actors/<actor>/animations/`) while `.lodl` and `.lodt` are
accepted by the predicate and shipped under `meshes/` by nothing.

**An .hkx row is an animation, not a document.** Opening one would otherwise
REPLACE the model it is meant to play on. It goes instead to the same
`HkxPlayback::load()` the render toolbar's "Load Animation (.hkx)…" button calls
and answers with the same summary sentence; a row inside a `.ba2` is staged to a
temporary file first, because lane HKX1's reader takes a path. With nothing open
to play it on it REFUSES IN WORDS and loads nothing — and "nothing open" is
measured, not guessed: it is `HkxPlayback::mapNames()`'s own named-node count,
the same instrument the summary line quotes.

**Loaded clips sit in the Loaded-files list** with a play-triangle icon painted
from `wwSkinColor("toggle")`, and their row menu is the first and only caller of
`HkxPlayback::unload()` — which has existed since lane HKX2 with nothing calling
it.

**Gates, pre-registered, WRITTEN AND UNRUN** (`tests/spells/files_tab.sh`,
`src/filestabtest.cpp`, `WW_FILESTAB_TEST`): (1) zero "NIF" strings in the page's
user-visible text READ OFF THE WIDGET TREE, with the floor that one seeded
offender must be found and then gone; (2) the tree lists `.hkx` and `.btr`, over
resource roots the harness FORCES rather than inheriting from Settings; (3)
opening the clip from a real tree row yields 78 matched / 17 unmatched / 4
case-folded; (4) unload restores the bind pose byte-identically, with the floor
that the clip must have moved the rig first; (5) the panel-style counts with a
blanked-tooltip floor; (6) the refusal. New files pass `g++ -fsyntax-only` with
the real `Makefile.Release` flags, RC=0, zero new warnings.
