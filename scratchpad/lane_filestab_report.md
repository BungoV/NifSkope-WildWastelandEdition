# Lane FILESTAB — the NIFs tab becomes the FILES tab (2026-09-10)

Repo `E:\Projects\NifskopeWildWastelandEdition`, `main`, **nothing committed**.

bungo's ruling, verbatim: *"add hkx files to the NIFs tab, search for them in
already set game folders ... rename 'available NIFs' to 'available files', and
rename NIFs tab to 'Files', then also rename Loaded NIFs to 'Loaded Files',
Basically replace mentions about nifs to generic 'files', because now we'll be
able to browse and open not just nifs, well, we already can, with stuff like bto
or btr"*.

**STATUS: BUILD PENDING.** Lane BUILD8 was still running at 14:59 (no
`scratchpad/build8_20260910/DONE`; it had just relinked `release/NifSkope.exe`
at 14:37:53), so nothing was applied, nothing was built, no exe was launched, no
gate was run and no picture was taken. Resume:
`scratchpad/filestab_20260910/PENDING.md`. The hook-up's `--check` was re-run at
**14:59:09** against the tree as BUILD8 left it: **71 edits, every anchor
matched as declared, nothing written.**

**FILE RULE.** Lane BUILD8 owns `src/nifskope.cpp`, `src/nifskope_ui.cpp`,
`NifSkope.pro` and `src/gltfexport.*` while this lane runs. Every edit to those
four is therefore a REFUSING PATCH SCRIPT
(`scratchpad/filestab_20260910/hookup.py`, `ww-anchored-hookup`), not an applied
edit. This lane's own code is in NEW files.

Skills invoked: `nifskope-ww-panel-style` (mandatory), `nif`, `ww-hkx-animation`,
`ww-anchored-hookup`, `nifskope-ww-build-verify`, `nifskope-ww-resume-pending`.

---

## 1. INVENTORY — where the dock is, and every "NIF" in it

### 1.1 What "that dock" is

There is no separate NIF-browser dock any more. The `QDockWidget` `BrowserDock`
still exists in `src/ui/nifskope.ui:845` (window title `NIF Browser`,
`src/ui/nifskope.ui:851`, and again as its View-menu entry at `:1900`), but its
two children are re-parented at start-up into the LEFT COLUMN's third page:

* the strip is a `QTabBar` `LeftColumnModeSelector`, built at
  `src/nifskope_ui.cpp:24060`, three tabs — `Header` (`:24075`), `Blocks`
  (`:24078`), **`NIFs` (`:24081`)**, each mapped to a `LeftColumnMode` through
  `tabData`, so the tab INDEX means nothing outside that widget;
* the page itself is `nifWorkspaceSplitter` (object name `NifBrowserSplitter`,
  `src/nifskope_ui.cpp:24098`), a vertical splitter of
  * the BROWSER: `ui->bsaTitleBar` (hidden), the one compact row
    `ui->horizontalLayout_2` holding `ui->bsaFilter` plus five `QToolButton`s,
    and the tree `bsaView` (a `NifBrowserTreeView`, object name `bsaView`,
    substituted for the Designer widget at `src/nifskope.cpp:2229-2238`), whose
    model is `bsaModel` (`BSAModel`) behind `bsaProxyModel`;
  * the LOADED list: `loadedNifsPane` (`LoadedNifsPane`) holding
    `loadedNifsFilter` (`LoadedNifsFilter`) and `loadedNifsView`
    (`LoadedNifsView`, a `LoadedNifsTreeView` with `LoadedNifsDelegate`), model
    `loadedNifsModel`.

So the user-visible text of "that dock" is: the tab's own text and tooltip, the
splitter's accessible strings, everything under `nifWorkspaceSplitter`, the two
models' header labels and group rows, the tree view's empty message, and the
four context menus the two views raise.

### 1.2 The strings, by file and line (before)

Extracted by `scratchpad/filestab_20260910/inventory.py` (every `tr("…")` whose
text contains `nif`, any case, in the two files that build this page), then cut
down by hand to the ones this page actually shows.

**A. The tab and the page frame — `src/nifskope_ui.cpp`**

| line | string | becomes |
|---|---|---|
| 24081 | `NIFs` (tab text) | `Files` |
| 24083 | `NIF Browser and Loaded NIFs` (tab tooltip) | `File Browser and Loaded files` |
| 24102 | `Drag to resize NIF Browser and Loaded NIFs` | `Drag to resize File Browser and Loaded files` |
| 24103 | `NIF Browser and Loaded NIFs splitter` | `File Browser and Loaded files splitter` |
| 24110 | `Drag to resize NIF Browser and Loaded NIFs` | as 24102 |
| 24111 | `Resize NIF Browser and Loaded NIFs` | `Resize File Browser and Loaded files` |
| 29427 | `Drag to resize NIF Browser and Loaded NIFs` | as 24102 |
| 29428 | `Resize NIF Browser and Loaded NIFs` | as 24111 |

`src/nifskope_ui.cpp:24077` `NIF Header` is the HEADER tab's tooltip and stays:
that page really does show a NIF header.

**B. The browser's one compact row — `src/nifskope.cpp`**

| line | string | becomes |
|---|---|---|
| 2258 | `Search NIFs...` (placeholder) | `Search files...` |
| 2270 | `Show favorite NIFs only` | `Show favorite files only` |
| 2275 | `Choose which NIF sources are shown` | `Choose which file sources are shown` |
| 2282 | `Loose NIFs` (sources menu) | `Loose files` |
| 2290, 2292 | `Load every selected NIF as a document` (x2, tooltip + accessibleName) | `Load every selected file as a document` |
| 2296, 2298 | `Reload available NIFs from the resource paths configured in Settings` (x2) | `Reload available files from …` |

**C. The tree's own rows — `src/nifskope.cpp`**

| line | string | becomes |
|---|---|---|
| 8345 | `Available NIFs` (the tree root) | `Available files` |
| 8425 | `Loose NIF` / `Archive NIF` (per-row tooltip) | `Loose file` / `Archive file` |
| 8436 | `No configured NIF resources for %1` | `No configured file resources for %1` |

`8346`'s root tooltip ("Merged archive and loose files from the configured %1
resource paths") is already generic and is untouched.

**D. The Loaded list — `src/nifskope.cpp`**

| line | string | becomes |
|---|---|---|
| 2319 | `Loaded NIFs · 0` (header label) | `Loaded files · 0` |
| 2530 | `Search loaded NIFs…` (placeholder) | `Search loaded files…` |
| 3208 | `Loaded NIFs · %1 of %2` | `Loaded files · %1 of %2` |
| 3210 | `Loaded NIF · 1` | `Loaded file · 1` |
| 3212 | `Loaded NIFs · %1` | `Loaded files · %1` |
| 3217 | `Drag a NIF here, or right-click to add files.` | `Drag a file here, or right-click to add files.` |
| 3219 | `No loaded NIFs match “%1”.` | `No loaded files match “%1”.` |
| 543 | `Drag one Loaded NIF at a time to save it` | `Drag one loaded file at a time to save it` |
| 946 | `The skeleton for Loaded NIFs — click to unmark it` | `… for Loaded files …` |
| 947 | `Use as the skeleton for Loaded NIFs — only one at a time` | `… for Loaded files …` |
| 9150 | `Loaded NIFs row marks — 1x as drawn, 8x as inspected` (the contact-sheet caption) | `Loaded files row marks — …` |

**E. The four context menus these two views raise — `src/nifskope.cpp`**

| line | string | becomes |
|---|---|---|
| 2588 | `Open NIF` | `Open` |
| 2589 | `Open NIF in New Window` | `Open in New Window` |
| 2591 | `Add %1 Selected to Loaded NIFs` | `Add %1 Selected to Loaded files` |
| 2592 | `Add to Loaded NIFs` | `Add to Loaded files` |
| 2640 | `Add NIF to Loaded NIFs…` | `Add file to Loaded files…` |
| 2641 | `Load any NIF from disk as a workspace document` | `Load any file from disk as a workspace document` |
| 3322, 4098 | `Use as Skeleton for Loaded NIFs` | `Use as Skeleton for Loaded files` |
| 3326, 4102 | `Every other loaded NIF evaluates its bones against this file, …` | `Every other loaded file …` |
| 3361, 4139 | `Show All Secondary NIFs` | `Show All Secondary Files` |
| 3362, 4140 | `Hide All Secondary NIFs` | `Hide All Secondary Files` |
| 3369, 4160 | `Remove from Loaded NIFs` | `Remove from Loaded files` |
| 3630 | `Remove %1 from Loaded NIFs\tX` | `Remove %1 from Loaded files\tX` |
| 4095 | `Write this loaded NIF to a file` | `Write this loaded file to disk` |
| 4096 | `Add NIF to Loaded NIFs…` | `Add file to Loaded files…` |
| 4129 | `… It appears in Loaded NIFs unsaved, …` | `… in Loaded files unsaved, …` |
| 4163 | `Revert Loaded NIF` (dialog title) | `Revert Loaded File` |
| 4164 | `Reverting %1 discards every unsaved change in that loaded NIF.` | `… in that loaded file.` |
| 4458 | `Add NIFs to Loaded NIFs` (file-dialog title) | `Add files to Loaded files` |
| 4471 | `Add NIFs` (undo command) | `Add files` |
| 4474 | `Added %1 NIF(s) to Loaded NIFs` | `Added %1 file(s) to Loaded files` |
| 4623 | `Unsaved Loaded NIF` | `Unsaved loaded file` |
| 4624 | `… Removing it from Loaded NIFs permanently …` | `… from Loaded files …` |
| 8780, 8788 | `NIF Browser Favorites` (message-box title) | `File Browser Favorites` |
| 9565 | `Could not load %1 into the Loaded NIFs workspace.` | `… Loaded files workspace.` |
| 9609 | `Finished loading background NIFs` | `Finished loading background files` |
| 9614 | `Loading background NIFs... %1 remaining` | `Loading background files... %1 remaining` |
| 9882 | `%1 dropped NIF(s) ready in this workspace.` | `%1 dropped file(s) ready …` |
| 9908 | `Open First Here; Add Rest to Loaded NIFs` | `… to Loaded files` |
| 9912 | `Add to Loaded NIFs` | `Add to Loaded files` |
| 9915 | `The current document is the clean starter. Open the first NIF here; additional NIFs stay in Loaded NIFs.` | `… the first file here; additional files stay in Loaded files.` |
| 9917 | `Keep the current document and all unsaved work; add every dropped NIF to this workspace.` | `… every dropped file …` |

**F. The `.ui` file — `src/ui/nifskope.ui`**

| line | string | becomes |
|---|---|---|
| 851 | `NIF Browser` (`BrowserDock` window title) | `File Browser` |
| 1900 | `NIF Browser` (the View-menu action for that dock) | `File Browser` |

### 1.3 What deliberately KEEPS the word "NIF", and why

These are inside or near the page but they name the FORMAT, not the list, and
bungo's ruling is about "mentions about nifs" as a synonym for "files":

* `src/nifskope_ui.cpp:24077` `NIF Header` — the Header tab's tooltip.
* `src/nifskope.cpp:4459`, `:4499`, `:3757` — file-dialog FILTERS
  (`NIF files (*.nif)`). A filter that says `.nif` is telling the truth about
  what it filters. The Add-to-Loaded dialog's filter is WIDENED instead (see
  §3), not renamed.
* `src/nifskope.cpp:2200`, `:5137`-`:5160`, `:6057`, `:6062` — the Header and
  Blocks pages ("Untitled NIF", "NIF version", "No blocks in this NIF").
* `:4124`, `:4128`, `:4205`, `:4208`, `:4393`, `:4400` — the faceBones NIF
  generator, which really does write a `.nif`.
* `:3715`, `:3723`, `:3755` — Merge / Flatten, which write a `.nif`.
* `:10303` — "NifSkope must be restarted…", the application's own name.
* `nifBrowserFavoritesPath()` and the `NifBrowser*` C++ identifiers,
  object names (`bsaView`, `LoadedNifsView`, `NifBrowserSplitter`) and QSettings
  keys are NOT user-visible; renaming them would lose a user's favourites and
  his saved splitter sizes for nothing. Untouched, deliberately.

### 1.4 How the tree is filled today

`NifSkope::populateConfiguredNifBrowserNow()` (`src/nifskope.cpp:8267`):

1. resource roots = `Game::GameManager::folders( game )` — exactly the paths the
   user configured in **Settings → Resources**, folders and `.ba2`/`.bsa` alike;
   a root whose leaf name is `textures` or `materials` is skipped and counted
   into the root tooltip;
2. each root is fed to `BA2File::loadArchivePath( …, &archiveFilterFunction )`,
   so **archives and loose folders go through the same indexer** — anything the
   filter accepts inside a BA2 is listed exactly like a loose file, and the
   per-row tooltip is the only thing that distinguishes them;
3. `archiveFilterFunction` (`src/nifskope.cpp:8248`) is the whole extension
   policy, and today it is three suffixes:
   `s.ends_with(".nif") || s.ends_with(".bto") || s.ends_with(".btr")`;
4. every indexed path is then required to start with `meshes/`
   (`:8404`), and the folder tree is built from the remainder.

So: the walker reads BA2s, but it filters by EXTENSION — `.hkx` does **not**
come for free, and step 3 is the one line that decides.

---

## 2. The renames, as edits

All 60 string edits are in `scratchpad/filestab_20260910/hookup.py`
(`RENAMES_CPP`, `RENAMES_UI_CPP`, `RENAMES_UI_XML`). They are pure substring
substitutions with no line ending in them, so no file's CR count moves because
of a rename -- `src/nifskope.cpp` is mixed CRLF-with-LF-blocks and the whole
+141 CR delta the script predicts belongs to the CODE insertions, not to these.

Same widgets, same layout, same icons: nothing here constructs, reparents,
restyles or reorders anything. The one thing that gains a widget property is a
loaded-animation ROW, which gains an icon (see section 4).

Three counts in the table are not 1, and each is deliberate:

* `"Load every selected NIF as a document"` and `"Reload available NIFs from
  the resource paths configured in Settings"` are each written TWICE -- once as
  `compactTool()`'s argument (which sets both the tooltip and the accessible
  name) and once as an explicit `setToolTip`. Both are renamed.
* `"Use as Skeleton for Loaded NIFs"` occurs THREE times: the two menu items,
  and a comment at `src/nifskope.cpp:921` that quotes the menu item by name. The
  comment is renamed with them, because a comment quoting a label that can no
  longer be found by that name is worse than no comment.
* `"Drag to resize NIF Browser and Loaded NIFs"` occurs three times and
  `"Resize NIF Browser and Loaded NIFs"` twice (the splitter, its handle, and
  the handle again where `restoreUi` rebuilds it).

The `.ui` file's two `NIF Browser` strings are the dock's window title and its
View-menu entry; they must move together or the menu stops naming the dock.

## 3. The tree extension: which extensions, and which of them are reachable

`archiveFilterFunction` (`src/nifskope.cpp:8248`) was the whole policy:

```cpp
return ( s.ends_with( ".nif" ) || s.ends_with( ".bto" ) || s.ends_with( ".btr" ) );
```

It now delegates to `wwFilesTabAccepts()` in the new `src/filestab.cpp`, whose
list is `.nif .bto .btr .hkx .gltf .lodl .lodt` -- ONE list, so the predicate,
the harness's census and any later caller cannot drift apart. The suffix compare
is on the raw bytes with an ASCII case fold rather than through a QString,
because the predicate is called once per archive entry and the FO4 animation
archive alone has 29,716.

**"if the walker reads BA2s, hkx come for free" -- it does not, and this is
which of the two it is.** The walker DOES read BA2s: every configured resource
root, folder or `.ba2` alike, goes through the one
`BA2File::loadArchivePath( ..., &archiveFilterFunction )` call, so archives and
loose folders are the same code path and the per-row tooltip is the only thing
that tells them apart. But the predicate FILTERS BY EXTENSION, so `.hkx` was
excluded at index time, and adding it to that list is the whole change.

**Reachability, stated because it is not uniform:**

| ext | reachable in the tree | why |
|---|---|---|
| `.nif` `.bto` `.btr` | yes, as before | they live under `meshes/` |
| `.hkx` | **yes** | FO4 keeps every clip under `meshes/actors/<actor>/animations/<group>/` and every skeleton under `meshes/actors/<actor>/characterassets/`, inside `Fallout4 - Animations.ba2` |
| `.gltf` | yes, wherever one is put under `meshes/` | same rule |
| `.lodl` `.lodt` | **accepted but not shipped** | after the extension test the tree keeps only paths starting with `meshes/` (`src/nifskope.cpp:8404`), because the folder walk strips that prefix to build the tree; nothing puts a `.lodl` there. They stay in the list so a user who does put one under `meshes/` sees it, and they are opened from disk otherwise. NOT FIXED, and not silently -- this is the honest answer to the brief's "state which" |

## 4. Opening an .hkx from the tree

`NifSkope::openArchiveFile()` (`src/nifskope.cpp:9340`) is where a tree row
becomes an action -- the double-click, the context menu's Open, and the Load
Selected button all pass through it. The hook-up inserts one branch there,
BEFORE the source dispatch, so it catches a clip whether the row came from a
`.ba2` or from a loose folder:

* **loose row** -> its real disk path goes straight to
  `wwFilesTabOpenAnimation()`, which is what lets `resolveNames()`'s second arm
  look for `skeleton.hkx` beside the clip and in `CharacterAssets` above it;
* **archive row** -> the bytes are extracted through the browser's own
  `BA2File` and staged into one per-session `QTemporaryDir`, because lane HKX1's
  reader takes a PATH. The path is the only thing lost, and the consequence is
  named rather than hidden: the beside-the-clip arm cannot fire from a temporary
  folder, so `resolveNames()` falls through to the skeletons already loaded this
  session and then to the game archives, and `summary()` says which arm served.

`wwFilesTabOpenAnimation()` then makes exactly the calls the "Load Animation
(.hkx)..." button makes (`src/nifskope_ui.cpp:26890`, lane HKX2):
`sc->hkx->load( path, &added )`, `ogl->setSceneSequence( added.first() )`,
`emit ogl->sequencesUpdated()`, and returns `sc->hkx->summary()` -- the SAME
sentence, not a reworded one. It goes to the status bar for 12 s and onto the
new row's tooltip.

**The refusal.** "No model is loaded" is not guessed from `currentFile` or from
a block count: it is `HkxPlayback::mapNames( scene, {} ).nodesInNif`, the count
of NAMED NODES the scene offers, which is the same instrument the summary line
quotes. Zero means the clip has nothing to bind to, and the answer is a sentence
-- *"<clip> is an animation, not a model: there is nothing open for it to play
on. Open the rigged NIF first, then open the animation."* -- shown in a message
box and in the status bar, with nothing loaded.

**The Loaded-files list.** `rebuildLoadedNifsBrowserGroup()` gains a third loop
after the documents and the background documents: one row per entry of
`Scene::animGroups`, carrying `NifBrowserClipRole` (the clip's name) and a play
triangle painted from `wwSkinColor( "toggle" )` -- no colour literal, and
repainted when the skin changes, so it follows a theme switch like every other
control on the page. A clip carries NEITHER document role, so the row delegate
finds no marks to draw on it and neither document menu opens for it; a third
branch routes it to `showLoadedClipMenu()`, which offers **Play This Animation**
and **Unload Animation**. That unload is the first and only caller of
`HkxPlayback::unload()` in the program -- lane HKX2's report, section 6 item 2,
recorded that it existed and that nothing called it.

## 5. The gates, pre-registered, WRITTEN AND UNRUN

`src/filestabtest.cpp` (`WW_FILESTAB_TEST`, its own translation unit so the
footprint in the 31,000-line `nifskope_ui.cpp` is one line), driven by
`tests/spells/files_tab.sh`.

| gate | what it asks | the floor beside it |
|---|---|---|
| (1) | 0 user-visible strings under the Files page still say "NIF", READ OFF THE LIVE WIDGET TREE -- tab text and tooltip, every label / button / placeholder / tooltip / accessible name / what's-this under the page, both models' header labels, the tree's group rows, the loaded list's empty message, and the tool buttons' menus. Offenders are LISTED, not counted | one offender is seeded (the loaded-files placeholder is set back to `Search loaded NIFs...`), the scan must find exactly it, then it is removed and the count must return to 0 |
| (2) | the tree lists at least one `.hkx` and at least one `.btr`, counted per extension over the model the browser is actually showing; the first `.hkx` path is printed so the log names the archive | `.nif > 0` in the same walk -- a walker that reaches no leaves proves nothing |
| (3) | opening the clip from a REAL row in the browser's model, through the view's own `doubleClicked`, yields HkxPlayback's own summary carrying **78** matched, **17** unmatched NAMED, **4** case-folded (lane HKX1's measurement on the player skeleton, pre-registered here) | the clip count must have gone up by exactly 1 and the clip must have become the playing sequence |
| (4) | after unload, every node's `Transform` is byte-identical to the pre-load snapshot -- all of it, memcmp, every node, not only the bound ones | while the clip plays at t = mid, **> 0** nodes must differ, or "it restored" is a statement about nothing |
| (5) | panel style: 0 `QGroupBox`, >= 4 tool buttons each with a tooltip AND an accessible name, both search fields with a placeholder, 0 check-box labels carrying " - ", and the loaded list BELOW the browser measured as GEOMETRY | one tooltip is blanked and the count must go to exactly 1, then it is restored |
| (6) | with the scene emptied (0 named nodes), opening an `.hkx` refuses IN WORDS, naming the reason, and loads nothing | the emptied scene must really report 0 named nodes first |
| (shot) | the left dock is grabbed to `scratchpad/filestab_20260910/dock_after.png` | -- |

**The resource roots are FORCED, not inherited.** Gate (2) over whatever happens
to be in Settings > Resources would be a measurement of the machine, not of the
code. The harness writes two roots into the game manager in memory
(`update_folders`, so bungo's saved settings are never touched): a loose
`Data/meshes/ww` tree the script builds -- `probe.nif`, `probe.bto`,
`probe.btr` -- and `Fallout4 - Animations.ba2`, so the `.hkx` come out of a real
ARCHIVE and it is the archive walker that is gated.

**The before/after pictures.** The AFTER grab is gate (shot). The BEFORE grab
has to be taken from the CURRENT exe before anything is applied, because the
harness that takes the after one does not exist in it -- and the current exe
already grabs this exact dock inside `WW_LOADEDNIFS_TEST`
(`src/nifskope_ui.cpp:10610` writes `release/ww_nifbrowser_test.png`). The
resume's step 0 is that grab. The pair shows the RENAMES; it is not a pixel
diff, because the two harnesses open different fixtures.

**What is NOT measured: everything.** No build, no harness run, no picture. In
particular these are UNVALIDATED claims of mechanism, each with what refutes it:

* that the `.hkx` route is reached at all -- refuted by gate (3) reporting the
  clip count unchanged, or by the harness's own named SKIP if the hook-up was
  not applied;
* that `.hkx` inside a `.ba2` survive `BA2File`'s index -- refuted by gate (2)
  counting 0 `.hkx`. `wwFilesTabAccepts` is exercised by nothing yet;
* that the play-triangle icon is legible against the row background at 16 px --
  refuted by the after grab. Counts do not see a mark the colour of its row;
* that emitting `doubleClicked` on the tree drives the same slot a real
  double-click does -- true by construction (Qt signals are public, and the
  connection at `src/nifskope.cpp:2241` is the only one on that signal),
  refuted if gate (3) opens nothing while the same clip loads from the button;
* that `QTemporaryDir` staging works on this machine at all: gate (3) uses a
  LOOSE path, so the ARCHIVE branch is untested by every gate here. Named, not
  hidden.

## 6. Mistakes (also in MISTAKES.md)

1. **A Bash heredoc halved the backslashes in the first inventory script**, so
   `[^"\\]` arrived as `[^"\]` and Python refused with "unterminated character
   set". This trap has now been paid for repeatedly in this repo in two days --
   `nifskope-ww-build-verify`, `nifskope-ww-resume-pending` and
   `ww-anchored-hookup` all name it -- and it was walked into anyway on a script
   that "was only a grep". Then it was walked into a SECOND time in the same
   lane, appending this report through `cat >> file << EOF`, which died with
   "unexpected EOF while looking for matching quote" and wrote nothing. The
   rule, restated so it has no exception: **every script and every multi-line
   text goes through the Write tool, whatever it is for.**
2. **Two anchors were declared unique that were not.**
   `const int source = nameIndex.data( NifBrowserSourceRole ).toInt();` occurs in
   three functions (widened with the line above it, which is the one place that
   declares a non-const `QModelIndex nameIndex`), and
   `"Use as Skeleton for Loaded NIFs"` occurs three times, not two, because a
   comment quotes the menu item by name. Both were caught by `--check` printing
   the COUNT rather than "ok", which is exactly why `ww-anchored-hookup` says to
   print counts -- so the cost was two minutes, not a build.

## 7. Finished-work skill review

**Loaded and used in earnest:** `nifskope-ww-panel-style` (its shared-helper and
counted-with-a-floor rules are gate (5); its "colours come from the skin table,
never a literal" rule is why the clip icon is repainted when `wwSkinColor`
changes rather than cached once); `ww-anchored-hookup` (the whole hook-up:
`--check` as the default, counts rather than "ok", the line ending IN the
anchor, the marker rather than the anchor as the applied-or-not test, and the
compile-with-and-without guard that lets `filestabtest.cpp` pass a syntax pass
today and run its seams after the hook-up); `nifskope-ww-build-verify` (the real
`Makefile.Release` flags for the syntax pass, and the heredoc trap it names --
ignored twice, section 6); `nifskope-ww-resume-pending` (the PENDING format,
qmake-before-make, the dependency read-back BY OBJECT, the exe-newer sweep over
every changed file); `ww-hkx-animation` (sections 11 and 12: the summary line,
`Scene::animGroups` as the animations list, the 78/17/4 numbers, and the
absolute-path trap that makes `hkxanim_play.sh` lie); `nif` (read; its dock
section does not exist -- see below).

**Declined, with the reason:** `nifskope-ww-render-shot` -- the deliverable here
is a DOCK grab, which is `SHOT=<png>` from inside the app, not a render through
the render hook; the skill says so itself, and this lane has no geometry to
photograph.

**The skill that should exist and does not, and it is the same one lane HKX2
named:** *"add a WW_*_TEST harness"*. This lane reconstructed the shape -- the
`completeLoading` + `singleShot(1500)` pattern, the `check` / `fails` counters,
`release/ww_<name>_test.log` ending `PASS` / `FAIL` / `done`, clearing the undo
stack before `QApplication::quit`, the `_harness.sh` window rules and
`winpath()`, the `--port <unused>` requirement, the poll-for-`^done$` loop in
the driver, the "a harness FORCES the state it measures" rule (here: the
resource roots, exactly as `loaded_nifs.sh` seeds the Block List mode), and the
"put it in its own translation unit when `nifskope_ui.cpp` is contended" trick
-- by reading `tests/spells/loaded_nifs.sh` and `src/hkxplaybacktest.cpp` end to
end. That is a full read of two large files, in two consecutive lanes, for a
procedure used in thirty harnesses. **This is now two lanes' worth of evidence
that it should be written**, and it is the director's to charter, because doing
it properly needs a survey of the thirty existing harnesses to separate the
convention from one lane's habit -- a survey this lane could not do while ending
BUILD PENDING.

**A second one, specific to this kind of work:** *"rename a user-visible string
family"* -- the `tr()` extraction with line numbers as the inventory, the
decision rule for what KEEPS the old word (it names the format, not the list),
the identifiers and QSettings keys that must NOT move (`nifBrowserFavoritesPath`
would have cost a user his favourites), the duplicate-count trap, and the
widget-tree scan with a seeded offender as the gate rather than a grep. Not
written here for the same reason as above: the gate has never been run, so the
procedure is not yet known to work.

**An amendment owed to `ww-anchored-hookup`** (the director mirrors it to the
other tree). Section 1 should gain all three of these, because this lane needed
them and none were there:

* **An anchor may need to be a LIST of consecutive lines**, and the script must
  then try BOTH line endings and require exactly one match across the two --
  `src/nifskope.cpp` is CRLF with LF blocks, and a single-ending search silently
  finds 0 in the wrong half of the same file.
* **`before` is a third mode, not a luxury.** A block that must run BEFORE an
  `if` whose own line is not unique cannot be placed with `after` alone: you end
  up anchoring on the line above, which is usually `}`.
* **A rename table needs an expected COUNT per row, not an implied 1.** Half of
  this lane's 60 renames legitimately occur two or three times (a tooltip set
  twice, two context menus sharing a label, a comment quoting a menu item), and
  forcing each to be unique would mean inventing longer anchors that carry no
  information. The count IS the assertion, and it caught two real errors here.


## Build (BUILD9)

Built and gated 2026-09-10 by lane BUILD9. BUILD8 never wrote a `DONE` marker;
its folder's newest file was 14:56:05 and `tasklist` showed nothing running at
15:16, so it was finished-or-dead and the slot was taken.

`hookup.py --check` at 15:17: 71 edits, every anchor matched as declared.
`--apply`: five files, byte and CR deltas exactly as predicted
(`src/nifskope.cpp` CR 9,379 -> 9,520 = the +141 CRLF lines inserted; the other
four LF-only and unchanged at CR 0). Syntax pass ALL-RC=0 in both
configurations. qmake RC=0, make RC=0. Dependency read-back by object:
`src/filestab.h` is named by `filestab.o`, `filestabtest.o`, `nifskope.o` and
`nifskope_ui.o`; `src/hkxplayback.h` by `nifskope.o` among nine.
`grep -c "lane FILESTAB"`: pro 0, `nifskope.cpp` 6, `nifskope.h` 2,
`nifskope_ui.cpp` 1.

### The gate table (final exe 15:52:46)

| gate | number | verdict |
|---|---|---|
| (1) | 0 strings say NIF; seeded offender found = 1, then 0 again | pass |
| (2) | `.nif 1 .bto 1 .btr 1 .hkx 14939 .gltf 0 .lodl 0 .lodt 0`; first `.hkx` `meshes/actors/_testcharacter/behaviors/_testcharacter.hkx` | pass, floor pass |
| (3) | 78 play / 17 named with no node / 4 by case; clip count +1; became the playing sequence | pass |
| (4) | posed 78 of 139; after unload **1 of 139 differs -- `PipboyBone`** | FAIL, cause measured |
| (5) | 0 group boxes; 6 tool buttons, **2 untipped, both `QLineEditIconButton`**; 2 placeholders; loaded list below browser (516 / 549) | FAIL, cause measured |
| (6) | refuses in words with 0 named nodes, loads nothing | pass |
| (shot) | `scratchpad/filestab_20260910/dock_after.png` written | pass |

**29 checks, 2 failures.** Before picture:
`scratchpad/filestab_20260910/dock_before.png`, taken from the OLD exe at
15:16:33 through `WW_LOADEDNIFS_TEST` before anything was applied -- tab "NIFs",
"Available NIFs", "Search loaded NIFs...", "Loaded NIFs / 2". After:
`dock_after.png` -- "Files", "Available files", "Search loaded files...",
"Loaded file / 1", and the animation archive's folders in the tree beside the
loose `ww` folder. The pair shows the RENAMES; it is not a pixel diff, because
the two harnesses open different fixtures.

### The two reds, measured, not amended

* **(5)** the two untipped buttons are `QLineEditIconButton` -- the clear buttons
  Qt creates inside a `QLineEdit` with `setClearButtonEnabled(true)`, one per
  search field. They are not controls this lane put on the row. Narrowing the
  gate's population after seeing its numbers is what CONSTITUTION 1 forbids, so
  it stays red; excluding Qt's internal buttons is bungo's call.
* **(4)** the node is `PipboyBone`, and `NifSkope -no-gui list` on the fixture
  shows `[72] NiNode 'PipboyBone'` followed by `[73] NiTransformController`: the
  fixture's own animation drives it. The gate snapshots the bind pose at t=0,
  scrubs to t=mid, unloads and compares WITHOUT stepping back, so the scene is
  still at the clip's time. Corroborated twice: `hkxanim_play.sh`'s restore gate
  is 27/0 on `skeleton.nif`, which has no such controller, and lane HKX3's gate
  (d), which DOES step after unloading, reads 0 of 139 on this same fixture.
  **For bungo: unloading a clip leaves the scene at the clip's time.**

### Repairs made to the instruments (not to the assertions)

1. `tests/spells/files_tab.sh` built its loose fixture tree with `mktemp -d`;
   `winpath()` only rewrites drive-style `/x/...` paths, so the binary was handed
   `/tmp/tmp.XXXX/Data`, could not open it, and the first run's census read
   `.nif 0 | .bto 0 | .btr 0 | .hkx 14939`. The tree now lives under
   `scratchpad/filestab_20260910/fixture_tree` and the script refuses if
   `winpath` does not return an `X:/` path.
2. The panel-style floor blanked `tools.first()`, which was already untipped, so
   it could not fire. The victim is now the first button that currently HAS a
   tooltip. The floor passes.
3. `src/filestabtest.cpp` now NAMES what fails: the nodes that did not restore,
   and the class/object name of every untipped button. Both reds only became
   leads once it did.

### The neighbours

`loaded_nifs.sh` went 3 failures -> 9 on the renames, because
`WW_LOADEDNIFS_TEST` asserts the expected TEXT of six labels this lane renamed.
Only the expected literals were updated
(`scratchpad/build9_20260910/fix_loadednifs_expect.py`, 6 anchors x1, CR 0 -> 0);
no check name, assertion or widget was touched, and it went back to **166 checks
/ 3 failures** -- the same three the before-run had.
`hkxanim_play.sh` 27 checks / 0 failures.

### Still owed / not measured

* the ARCHIVE branch of the `.hkx` open (the `QTemporaryDir` staging): gate (3)
  uses a loose path, so it is untested by every gate here;
* `.lodl` / `.lodt` remain accepted by the predicate but unreachable, because the
  tree keeps only paths under `meshes/`;
* the drop route for an `.hkx` dropped on the window is lane HKX3's, and its
  gate (e) is a SIMULATED drop -- a real Explorer drag is bungo's to try.

### The clocks, in one table

| artefact | time |
|---|---|
| `release/NifSkope.exe` (final) | 2026-09-10 15:52:46 |
| `release/style.qss` | 15:52:46, `cmp` equal to `res/style.qss` |
| FILESTAB hook-up applied | 15:17 |
| FILESTAB first gated exe | 15:20:34, re-gated on 15:25:05 and 15:52:46 |
| HKX3 hook-up applied | 15:29 |
| HKX3 gated exe | 15:32:06, re-gated on 15:35:05 and 15:52:46 |
| SKELOVERLAY hook-up applied | 15:40 |
| SKELOVERLAY gated exe | 15:36:50, re-gated on 15:52:46 |
| alignment before-measurement | 15:43:14 exe |
| alignment after-measurement | 15:52:46 exe |

Exe-newer sweep over every path `git status --porcelain -- src res tools tests`
reports: 0 stale at each gate run.
