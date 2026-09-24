# HANDOFF top block — lane UINOTES2, 2026-09-12

**Where it stands.** The two gizmo switches in the animation dock's transport row
are icons in ruling 4's set and light up in WHITE (`textBright`) when their mode
is on — ruling 07:3x, "keep them white", after the first cut lit them in the
accent; every key diamond outside the play range is greyed out (ruling 08:2x);
Ctrl+A in the Animations list keeps every row; the `lodl_open` crash lane ROADS4
handed back is measured and is NOT the UI's. Built and gated in the copy tree
only. **Nothing committed, nothing stashed, nothing under
E:/Projects/NifskopeWildWastelandEdition opened, edited, built or run** — except
one read-only copy of `scratchpad/uinotes1_20260912/RESUME_BY_ROADS4.md`, which
the director told me to take.

**The exe.** `E:/Projects/NifskopeWWE_ui/release/NifSkope.exe`, **2026-09-12
08:47:03, 21,867,008 B, md5 `1b41b4f9407732f00fd9d02399dac5e3`**, make exit 0
(`BUILD4-RC=0` in `build_main4.log`), PE header `MZ` read back before the exe was
run, `res/style.qss` and `release/style.qss` identical. It replaces the 06:55:52
/ 21,850,624 B exe this lane's first delivery shipped. The rung it replaces is kept
as `release/NifSkope.before_uinotes2.exe` (05:48:33, 21,817,856 B, md5
980e64c1aa4e5478b5833d83ebea9655 — the same bytes lane ROADS4 measured).

**Gates on that exe.** animws **236 / 0** (2 skips, both fixture skips — the
same two) against 224 / 0 before rulings 07:3x and 08:2x and a 210 / 1 rung; lodl_open **23 / 0 PASS** against 23 / 2 with six segfaults on the
rung; ui_align 15 / 0; water_ui 82 / 0 (82 on the rung too — the brief's 84 is
the main tree's number); top_bar 43 / 5, the same five as before; skeleton_overlay
46 / 0 in-app plus its mask and dots gates run by hand under a numpy-equipped
python. hkxanim_ui and files_tab were **skipped**: both want
`scratchpad/hkx1_20260910/clips/*.hkx`, absent from this copy, and its only home
is the main tree.

**Six files changed, all pure LF**: `src/animworkspace.cpp` (122,142 B),
`src/animworkspacetest.cpp` (162,827 B), `src/animdopesheet.cpp` (40,890 B),
`src/animdopesheet.h` (12,302 B), `tests/spells/animws.sh` (13,928 B), and the
skill `.claude/skills/ww-toggle-lit-gate/SKILL.md` (6,646 B) — plus
`.claude/skills/nifskope-ww-build-verify/SKILL.md` amended in the first
delivery. `src/animdopesheet.*` are the two outside the first delivery's five:
ruling 08:2x lives in the sheet's painter, and its dim is exposed as
`AnimDopeSheet::dimOutOfRange()` so the harness measures the pixels against the
painter's own arithmetic instead of re-deriving the constant. Byte counts, CR
counts and the not-touched list are in
`scratchpad/uinotes2_20260912/CHANGED_FILES.txt`.

**What the next lane needs to know.**

1. **The lodl viewer crash is still owed, and not to the UI.** On the 05:48:33
   binary every `.lodl` render in the viewer segfaults (3 of 3 here, 6 of 6 in
   the harness). It is not scene size and it is not the UI chrome: that same
   binary headless-renders a plain NIF and a 2.65 MB terrain NIF built from the
   same lodl, both rc 0 and both pictures byte-identical to my exe's. Only the
   `.lodl` DOCUMENT in the viewer dies, and the stack is 19 frames with no
   repetition, inside a queued call after "meshed and built". My exe is green,
   but my build also recompiled lodgen.o, lodgenchunkpass.o, lodgenmanager.o,
   nativeemit.o and nifcli.o against lane ROADS4's in-flight `src/lodgen.h`, so
   **my green row does not prove the rung's lodgen sources are green.** Whoever
   owns that path should re-measure once ROADS4's edits land.
2. **Loop does not light and that is deliberate.** The dock's loop button takes
   its icon from the shared `aAnimLoop` action, and the render toolbar re-skins
   that action on every popup refresh with a single-state icon
   (`src/nifskope_ui.cpp`, `tlMakeIcon( "loop", ... )`). Making Loop light means
   changing the RENDER TOOLBAR's own glyph, which bungo did not ask for. The
   harness prints loop's two inks beside the two it gates, measured and
   deliberately not asserted.
3. **Three things the pictures show that nobody has ruled on**: annotation labels
   overlap each other at 1:1 when markers are close (Blender drops the text
   instead); the right-side panel keeps an empty "Retime to" row when an
   annotation is selected; the Animations list row is one long clipped label.
4. **Ctrl+A's deliberate trade**, written beside the code: a selection the user
   made outranks the viewport's echo of it. If bungo wants the list to collapse
   to the clip the viewport switched to, it is the condition on one line.

**Pictures** in `scratchpad/uinotes2_20260912/images/after/` (r3, r6 x2, r6a x2,
r7, r7a x2, r9, the 1:1 dock overview, the transport row at 2:1 off and on) and
the before/after composites beside them.

5. **This session could not EXECUTE `release/NifSkope.exe` from its bash shell,
   and the exe is fine.** From 08:47 on, `execve` of that path returned EACCES
   ("Permission denied", exit 126) — and so did `cmd /c` from the same shell,
   while the very same bytes copied to another name in the same folder ran
   normally, an older exe at a different name ran normally, and PowerShell
   launched `release/NifSkope.exe` itself without complaint. It is the path, in
   this shell, not the binary: md5 of the shipped file equals md5 of the copy the
   harness ran. If a lane hits it again, copy the exe beside itself and point the
   spell at it with `EXE=`; do not rebuild chasing a corrupt binary.
