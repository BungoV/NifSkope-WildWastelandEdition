# HANDOFF block -- lane UINOTES1 (for HANDOFF.md's top block)

**Lane UINOTES1 -- bungo's nine animation-workspace rulings of 2026-09-12
01:3x-02:0x. All nine items are CODE IN and syntax-clean in
`E:\Projects\NifskopeWWE_ui` (a robocopy of the repo taken 2026-09-12 01:53).
BUILD PENDING: `Fallout4.exe` was up (pid 22908, started 03:48:37) from before
the build step to the end of the lane, and the standing rule is no builds while
the game runs. Nothing in this lane is proven beyond `g++ -fsyntax-only`; no
gate has produced a number and no picture has been taken.**

**Resume from** `E:\Projects\NifskopeWWE_ui\scratchpad\uinotes1_20260912\PENDING.md`.
The report is `scratchpad\lane_uinotes1_report.md` (sections 0-15); the merge
list is `scratchpad\uinotes1_20260912\CHANGED_FILES.txt`.

**What is on disk, by ruling:** 1 the window's status bar retired, the messages
moved to a fading viewport line; 2 / 7b / 8 Blender's key, annotation, playhead
and out-of-range colours as new `wwskin` tokens read from the installed Blender
4.5; 9 draggable play-range grips plus Start / End fields; 7 / 7a right-click
annotations at the clicked frame and the zoom-reset on marker drag fixed; 3
"Remove transform axes…" with its six boxes; 5 the Animations list's shortcuts,
right-click menu and drag-reorder; 6 / 6a the fifteen-button bar retired into a
header menu bar and a right-side panel on the N key; 4 the transport icons drawn
on one grid at one weight with tooltips that name their keys, and the
"Load… / root" header no longer clipped.

**Two things the next lane must not get wrong:**

1. **`qmake NifSkope.pro` has been run in that tree and must be re-run if the
   tree moves again.** The Makefiles the robocopy carried across pointed at
   `E:/Projects/NifskopeWildWastelandEdition/GeneratedFiles/.obj/icon_res.o`, so
   a `make` before qmake would have written an object file into the other lane's
   tree.
2. **`release/NifSkope.before_uinotes1.exe`** is the baseline every "before"
   picture must come from: 2026-09-11 23:26:29, 21,484,032 bytes, sha256
   `a77ede97ff487ec766a53ea79c8b848b1552d5ab0413d3221ee627a8f137496d`.

**Chain to run after the build** (second monitor, own unused `--port`, and check
`tasklist` for a NifSkope started from the MAIN tree first): `animws.sh` (a)-(q),
`hkxanim_ui.sh`, `ui_align.sh`, `top_bar.sh`, `skeleton_overlay.sh`. Skipping
`water_ui.sh` and `files_tab.sh` -- this lane touched neither water nor the file
browser.

**Owed, in the report's section 13:** a key-level clipboard (Ctrl+C/V and
Shift+D on KEYS, from ruling 6's second bullet) is not implemented; the icons are
drawn with QPainterPath rather than real .svg files because NifSkope links no Qt
Svg module and neither `Qt6Svg.dll` nor the `qsvg` plugin is deployed -- bungo's
call; and four smaller questions are listed there for him (whether Rename should
rewrite a clip's internal name on save, whether Move up/down should reorder the
NIF's own sequence blocks, item 6b's Material Manager ambiguity, and whether
"Remove transform axes" should use frame 0, the mean, or the playhead as its
reference).

**Nothing in bungo's game folder was touched, no NifSkope was launched at any
point in this lane, and nothing was committed or stashed in either tree.**
