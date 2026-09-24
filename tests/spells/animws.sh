#!/bin/bash
#
# The ANIMATION WORKSPACE: one dock that replaces the Animation Manager, with
# the .hkx clip fully editable (lane HKXEDIT2, 2026-09-10).
#
# WHY THIS EXISTS
#
# bungo, verbatim: "Just make hkx fully editable in our nifskope" and
# "Animation manager was one of the first features for nifskope, and it's
# pretty old and outdated btw". The standalone gate (release/hkxclipedit_gate.exe,
# 72 checks) proves the DOCUMENT -- keys, regeneration, trim, retime, bake,
# annotations, save. Nothing there touches a widget or the viewport, so
# nothing there can say whether the sheet shows 78 bone rows, whether a
# gizmo pose becomes a key, whether the bone row and the viewport select each
# other, or whether Undo through the shared group puts the bytes back. That is
# what this asks, from the UI end, inside the real application.
#
# THE FIXTURE IS THE POINT: the Mixamo clip is 60 fps and 93 frames -- a ruler
# at a fixed 30 would call frame 46 "frame 23". Its COM track carries 487 units
# of travel with the extracted motion all zero, which is what the bake gate
# turns around.
#
# WHAT IS MEASURED (the brief's gate letters; each check has a floor beside it)
#   (a) the list shows the clip; the sheet has 78 bone rows with 93 keys each;
#       the ruler reads 60 fps
#   (b) a key inserted on one bone at frame 46 with exactly 30 deg about X reads
#       back within 0.01 deg from the DOCUMENT and from the viewport's NODE;
#       every other bone and frame byte-identical
#   (c) deleting that key: the interpolation, then Undo -> the pre-edit decode
#       exactly
#   (d) "FootLeft" at frame 30 through Add annotation, save, reload -> present
#   (e) trim 10..50 -> 41 frames; retime 60->30 -> 47 frames, coincident frames
#       bit-identical
#   (f) bake root motion on COM: track travel 0, motion ~487; unbake byte-identical
#   (g) Save as .hkx -> our reader decodes it equal to the edited clip
#   (h) undo/redo across insert / delete / annotation / trim through the shared
#       QUndoGroup
#   (i) a NIF NiControllerSequence still plays and its rows still write the
#       block (cycle type, start / stop time)
#   (j) panel-style counts with floors; a dock grab at frame 46 and a viewport
#       grab with the gizmo on a bone
#   (k) the DRAWING, from the pixels the sheet painted (lane UINOTES1,
#       2026-09-12, bungo rulings 2 / 7b / 8): active selected key orange,
#       other selected keys orange-red, unselected plain, the same three on
#       the annotations, a blue playhead box and line, outside the clip range
#       darkened and inside untouched -- at two zoom levels, with the old
#       colour pair shown failing the same contrast test
#  (k8b) KEYS OUTSIDE THE RANGE ARE GREYED (ruling 08:2x, bungo verbatim:
#       "also, grey out diamond keyframes out of animations start and end
#       range, to indicate they're not being taking into consideration
#       anymore"). With the range at 10..50 on the 93-frame fixture the
#       diamonds at frames 5 and 80 must equal AnimDopeSheet::dimOutOfRange()
#       of the plain ink EXACTLY -- the `animOutOfRange` token at Blender's
#       own alpha 155/255 laid over the ink, the same arithmetic the darkened
#       band does to the ground -- while frame 30 stays plain. End is then
#       moved to 90 and frame 80 must be plain again with no reload (the "the
#       moment the grips move" half), and a SELECTED key out of range must
#       keep its orange / orange-red, greyed the same way. The two zoom passes
#       of (k) carry the same expectation, which is why pass 2's range of
#       25..40 leaves its ACTIVE key at frame 20 greyed. PICTURE:
#       sheet_range_dim.png, the sheet at 1:1 with that range. FLOOR: with the
#       range over the whole clip the same three samples are all plain again
#   (l) the PLAY RANGE (ruling 9): the ruler's two grips dragged with real
#       mouse events +10 / -10 frames and the transport's End box typed into,
#       each asserting four numbers at once (document, sheet, both boxes) and
#       the clip's 93 frames untouched; the darkened zone's edge read out of
#       the pixels; a save carrying the range comes back 61 frames and 60
#       frame-times long with frame 0 == the clip's frame 10; three gestures =
#       three undo commands. FLOOR: the same save with nothing out of range
#       writes all 93 frames
#   (m) RIGHT-CLICK ANYWHERE + THE ZOOM RESET (rulings 7 / 7a): the menu is
#       opened at three x positions (a bone row, the ruler twice) and its
#       entries read out of the live QMenu -- each names the CLICKED frame; on
#       an annotation it offers Rename and Remove by name; the inline editor
#       adds nothing until Return (Escape leaves no annotation and no undo
#       command) and costs exactly one command; M / Ctrl+M open it; then the
#       sheet is zoomed to 20..60, an existing annotation dragged 5 frames,
#       and the visible range must be unchanged. FLOORS: the frame in the menu
#       text differs at each x, and frameAll() must fail the same zoom check
#   (n) REMOVE TRANSFORM AXES (ruling 3): the right-click entry sits directly
#       under Remove track and opens a dialog of six boxes (translation and
#       rotation X/Y/Z) plus the line naming the rotation convention; ticking
#       translation X and Y on COM of Running_To_Slide_And_Back_To_Running
#       holds both at their FRAME-0 value over all 93 keys, leaves Z and every
#       rotation byte-identical, costs one undo command, saves and reads back
#       bit for bit, and one Undo restores every byte. FLOOR: before the strip
#       X, Y and Z must each travel more than 1 unit, and the numbers are
#       printed, so a no-op cannot pass
#   (p) THE BUTTON ROW AND THE KEYS BLOCK (rulings 6 and 6a): the fifteen
#       buttons at the bottom of the dock are gone -- the gate checks there is
#       no AnimWsActionBar and no QPushButton left in the dock at all -- and
#       every one of their sixteen texts is found by walking the header menu
#       bar's five menus (Clip / Key / Channel / Marker / View) and reading the
#       entries out of them; Save and Save as are read specifically out of
#       Clip. The right-side panel is forced open and then asked what it shows
#       for each kind of selection: a bone row -> Clip + Track only (and the
#       Bone field reads COM), keys -> Clip + Key, an annotation -> Clip +
#       Annotation, a float row -> Clip + Float track; every visible section is
#       NAMED in the line, so a panel showing all five fails the same check a
#       correct one passes. The panel is then dragged to 240 px, closed,
#       reopened, and must come back the same width, and the N shortcut's key
#       is asserted before its activated signal is fired (a synthesized key
#       event never reaches a QShortcut). FLOORS: 'Frobnicate' is NOT found by
#       the same menu search; the height of the row that was retired is not
#       remembered but MEASURED, by building one real QPushButton and adding
#       the old bar's margins, and the new header must be no taller than it
#   (q) THE TRANSPORT ICONS (ruling 4): the eight transport buttons plus the
#       loop and auto-key toggles are asked, one at a time, whether they carry
#       a drawn icon, whether they still show a text glyph, and what size their
#       icon is -- the sizes are printed, so "one size" is a number. Every
#       tooltip is compared to the EXACT sentence it should carry, each of
#       which names the action and the key that does it. The six new shortcuts
#       (Space, Shift+Ctrl+Space, Shift+Left, Shift+Right, Up, Down, all on the
#       sheet) have their key bindings asserted and two of them are fired, with
#       the playhead read back at both ends. The speed field must read "1.50x",
#       not "x1.50". Then the last line of the ruling: no button in the
#       "Load... / root" header row may be narrower than the width it asks for.
#       The bar is saved as transport_1x.png and transport_2x.png.
#       FLOORS: a sentence no button carries ("Frobnicate the sprocket") is run
#       through the same tooltip comparison and must NOT match; and the clipping
#       predicate is proved able to fail on the same run -- the left column is
#       squeezed to 40 px, the SAME predicate must call the header clipped
#       there, and only then is the restored width allowed to pass.
#       ADDED 2026-09-12 (lane UINOTES2, bungo: "Both icons"): the two GIZMO
#       TOGGLES -- Pose with the gizmo and Auto-key -- are drawings in the same
#       set now, not a word and a speck. The gate asks each of them for an icon
#       and no text, for the PLAY button's icon size, and for a LIT state: the
#       mean colour of the icon's own ink (alpha over half) is taken with the
#       toggle off and on, and each mean must land within 2 ON EVERY CHANNEL of
#       the token it is meant to be (per channel, because un-premultiplying an
#       antialiased mean costs up to 1 a channel and the pose ring is nearly all
#       edge): OFF on `text`, ON on `textBright`, the palette's brightest
#       ink. AMENDED 2026-09-12 07:3x, bungo over icons_before_after.png: "Why
#       do these turn yellow when selected? the buttons, keep them white" --
#       the lit ink was `accent` for one build. The two tokens are 33 apart in
#       summed RGB, so "it moved" is floored at 20 and not the 60 the accent
#       allowed; a button that only changed its plate still cannot pass, because
#       both means are pinned to their own token. Pose's tooltip joins the exact-text
#       table it was never in. FLOOR: the same arithmetic is run on Stop, which
#       has no lit state, and must report it moved 0. PICTURES: the same bar at
#       2:1 with both toggles off (transport_2x_off.png) and on
#       (transport_2x_on.png), and both switches are put back as they were found.
#   (o) THE ANIMATIONS LIST (ruling 5): three clips are made with Shift+D, then
#       Ctrl+A, Ctrl+Up/Down, Ctrl+C/V/X, Del, F2 and a double-click are each
#       fired through the list's OWN shortcut objects (whose key bindings are
#       asserted separately), every entry of the right-click menu is read out
#       of the live QMenu and photographed to animws_list_menu.png, 'Move down'
#       is chosen IN the menu, and a drop is delivered to the viewport. After
#       each one the row order, count and names are read back from the list,
#       from the clips themselves and from the scene's animations list.
#       FLOOR: a rename must show up in all three, and the gate leaves the
#       fixture exactly as it found it
#       CHANGED 2026-09-12 (lane UINOTES2): Ctrl+A is read at three moments and
#       ALL THREE are the check now -- straight after the key, one pump later,
#       and once the 50 ms rebuild has landed. It used to check the first alone
#       and print the rest, which was right while the defect was being measured
#       (the key's own effect is one fact; what eats the selection afterwards is
#       another). Both places are fixed, so a selection that survives the key and
#       dies 50 ms later is a failure, not a printed number.
#
#   PICTURES (lane UINOTES2, 2026-09-12): the six rulings that had none get one
#       each, taken inside the gate that already drives that state -- r3 the
#       Remove-transform-axes dialog with two boxes ticked, r6 the Clip menu open
#       on the header menu bar, r6a the side panel at two kinds of selection, r7
#       the ruler's right-click menu at the clicked frame, r7a the marker menu
#       and the sheet still at its zoom after the drag, r9 the ruler with both
#       range grips -- plus dock_overview.png, the whole dock at 1:1 with the
#       side panel open. A menu and a modal dialog can only be photographed from
#       inside the timer that drives them, which is why those two shots live in
#       the lambdas rather than beside their checks.
#
# EVERY PATH GIVEN TO THE EXE IS ABSOLUTE AND WINDOWS-SHAPED (winpath). Keep
# --port below 49152: the app exits SILENTLY on a bound port.
#
# USAGE
#   bash tests/spells/animws.sh
#   SRC=... CLIP=... SEQNIF=... bash tests/spells/animws.sh

set -u
. "$(dirname "$0")/_harness.sh"

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
NS="${EXE:-$ROOT/release/NifSkope.exe}"
SRC="${SRC:-$ROOT/fixtures/human_male_vanilla.nif}"
CLIP="${CLIP:-$ROOT/fixtures/Running_To_Slide_And_Back_To_Running.hkx}"
SKEL="${SKEL:-$ROOT/scratchpad/hkx1_20260910/clips/skeleton.hkx}"
JOG="${JOG:-$ROOT/scratchpad/hkx1_20260910/clips/jog.hkx}"
# a NIF with a NiControllerSequence for gate (i); the vanilla corpus, never a mod folder
SEQNIF="${SEQNIF:-E:/Tools/Fallout 4/DataUnpacked/Data/Meshes/Weapons/10mmPistol/10mmPistol.nif}"
OUTDIR="${OUTDIR:-$ROOT/scratchpad/hkxedit2_20260910}"
LOG="$ROOT/release/ww_animws_test.log"
PORT="${PORT:-42317}"

[ -x "$NS" ] || { echo "no NifSkope.exe at $NS"; exit 2; }
[ -f "$SRC" ] || { echo "no rigged NIF at $SRC"; exit 2; }
[ -f "$CLIP" ] || { echo "no clip at $CLIP"; exit 2; }
mkdir -p "$OUTDIR"

rm -f "$LOG"
WW_ANIMWS_TEST=1 \
WW_ANIMWS_CLIP="$(winpath "$CLIP")" \
WW_ANIMWS_SKEL="$(winpath "$SKEL")" \
WW_ANIMWS_JOG="$(winpath "$JOG")" \
WW_ANIMWS_SEQNIF="$(winpath "$SEQNIF")" \
WW_ANIMWS_OUT="$(winpath "$OUTDIR")" \
	"$NS" --port "$PORT" "$(winpath "$SRC")" >/dev/null 2>&1 &
pid=$!
for _ in $(seq 1 180); do
	[ -f "$LOG" ] && grep -q '^done$' "$LOG" 2>/dev/null && break
	kill -0 "$pid" 2>/dev/null || break
	sleep 1
done
kill "$pid" 2>/dev/null; wait "$pid" 2>/dev/null

[ -f "$LOG" ] || { echo "FAIL: the harness wrote no log (did the app exit before it ran, or is port $PORT bound?)"; exit 1; }
cat "$LOG"
echo "--- skips (a SKIP is never a pass) ---"
grep "SKIP" "$LOG" || echo "(none)"
ls -l "$OUTDIR"/dock_frame46.png "$OUTDIR"/viewport_gizmo.png "$OUTDIR"/sheet_colours.png 2>/dev/null
grep -q '^PASS$' "$LOG" && exit 0
exit 1
