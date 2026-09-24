#!/usr/bin/env python3
"""Append sections 2-7 to scratchpad/lane_filestab_report.md.

Written as a script rather than a heredoc: a quoted Bash heredoc through the
tool halves backslashes and, on this text, died with "unexpected EOF while
looking for matching quote" and wrote nothing (root MISTAKES.md, lane FILESTAB).
The report is LF-only and stays so; the append is idempotent (it refuses when
section 2 is already there).
"""
import io
import os

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
DEST = os.path.join(REPO, 'scratchpad', 'lane_filestab_report.md')

TEXT = r"""
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
"""


def main():
    with io.open(DEST, 'rb') as f:
        data = f.read()
    if b'## 2. The renames, as edits' in data:
        print('already appended; nothing written')
        return
    data += TEXT.encode('utf-8')
    with io.open(DEST, 'wb') as f:
        f.write(data)
    print('appended %d bytes; CR now %d' % (len(TEXT), data.count(b'\r')))


if __name__ == '__main__':
    main()
