#!/bin/bash
#
# WW_WATERUI_TEST -- lane WATER7 / UI2: the Water tab, and one compact height
# for every bar at the top of the window.
#
# bungo's two rulings, 2026-09-10, both on screenshots, both about the same
# strip:
#
#   (A) "Two issues with water window and water marking appearing here"
#       (the Workspaces menu), then the same minute "They should be in the LOD
#       gen workspace", then over the Header | Blocks | Files strip "You'd
#       access them like this".  So the water tool is a FOURTH TAB of that
#       strip, shown while the LOD Generation workspace is open; there is no
#       water dock and no entry for either half of the tool in that menu.
#
#   (B) "compact these vertically like this, the top bar and the buttons".
#       BUILD9 gave the main toolbars, the viewport header and the dock strip
#       one height.  It left out the MENU BAR and everything inside every bar.
#
# WHY GEOMETRY AND NOT A PICTURE.  A picture shows a step; only the rectangles
# say by how many pixels, and only they can say it is gone.  Every number is
# read off the live widgets in MAIN-WINDOW coordinates -- two widgets in
# different parents cannot be compared any other way.  The two pictures this
# writes are for bungo, not for the verdict.
#
# THE GATES (src/wateruitest.cpp holds the detail and the floors)
# (A2) CORRECTED, 2026-09-10 20:3x, verbatim: "What? I wanted it in that right
#      panel though".  The strip was the STYLE and the LOD GENERATION PANEL is
#      the PLACE.  Group T measured the left strip and is retired; group L
#      (src/wateruitest_lod.cpp) measures the panel's own strip.
#
#   L1  the LOD panel's strip carries exactly 2 tabs, LOD then Water
#       (floor: the same search finds no "Watre")
#   L2  its pages are LodgenPanel at 0 and WaterMarkPanel at 1, and each tab's
#       data IS its page index
#       (floor: the SAME predicate asked at the wrong index goes false)
#   L3  selecting Water shows the marking rows AND the flow-window button
#       (floor: selecting LOD puts the generator's page and Generate back)
#   L4  that strip is wwBarRowHeight() and the same height as the left strip
#   L5  ...and carries the SAME stylesheet, byte for byte, hashes printed
#       (floor: the compact default is a different string)
#   L6  the LEFT strip is back to 3 tabs and 3 pages, with the LOD workspace
#       OPEN and again with it CLOSED -- both states in one run
#   L7  no WaterMarkDock, and no "Water Marking" / "Water window" in the
#       Workspaces menu (floors: the same scans still find the LOD dock and
#       the "LOD Generation" entry)
#   L8  the panel's strip paints its segments 4 px clear of the row, top and
#       bottom (floor: the LEFT strip reads the same two numbers)
#   R1  every visible bar at the top of the window is wwBarRowHeight() +/- 1
#   R2  the menu bar and the dock tab strip are one height (floor: +4 px goes red)
#   R3  EVERY tool button in tFile / tLOD / tView is the row's own height
#       within 1 px, and agrees with its neighbours within 1 px.  BUILD12
#       shipped 39 px buttons in a 35 px row and this gate passed them,
#       because it allowed 8 px where this header already promised 1.  It is
#       1 px, and every button is printed by name, not just the extremes.
#       (floors: at least 4 buttons found; the calibration measured a real
#        style overhead rather than falling back; and THE SAME TEST GOES RED,
#        live in the same run, when BUILD12's own arithmetic is appended over
#        the shipped sheet -- then green again when it is taken away, which is
#        also what makes the picture below the shipped state)
#   R4  the row's box is stated ONCE by the skin, from the row's own number:
#       one min-height and one pair of vertical paddings per selector, nothing
#       horizontal, and the bar's own box taken away -- which is what puts a
#       button at y 0 instead of y 4.  It also pins the menu ARROW to the
#       middle of the button: a QToolButton's default indicator sits in the
#       bottom-right corner, invisible while the button is only as tall as
#       its text and obvious the moment it is the row's height (UI3's first
#       build put the viewport header's arrows a row below their glyphs).
#   R5  the search row still starts where the viewport's content starts
#
#   (C) "just do what is on my screenshot, 4 pixels from each nearby element
#       of separation for the header / blocks / files" (2026-09-10 20:1x, over
#       a screenshot of the 18:25:20 window).  The row height STAYS 35 and the
#       toolbars and their buttons stay as lane UI3 left them; the SEGMENTS get
#       4 px of air from every neighbour and from one another, and so get 8 px
#       shorter inside the same row.
#
#   S1  the strip starts 4 px below the row's top edge
#   S2  ...and ends 4 px above its bottom edge (the search row does not move)
#   S3  the first segment is 4 px from the window's content edge
#   S4  the last segment is 4 px from the toolbar beside it.  Only part of that
#       air is ours: QMainWindow::separator (res/style.qss:66) already paints
#       3 px between the dock and the column, so the segment adds the fourth.
#   S5  the segments TOUCH -- 0 px apart (lane UI6; bungo, "Why are they
#       separated?"). The FOUR OUTER distances are still 4.
#   S6  and the ROW did not move -- the bar is still wwBarRowHeight() and the
#       segments are that less twice the air
#       (floors: every segment found as a real painted box; the SAME scan finds
#        nothing for a colour the strip does not carry; and THE SAME FIVE GO
#        RED, live in the same run, when the 18:25:20 flush strip is appended
#        over the shipped sheet -- then back to 4 when it is taken away, which
#        is also what makes the pictures the shipped state)
#
#   S measures PIXELS, not rects, and that is not a stylistic choice: QSS
#   margins on QTabBar::tab ARE honoured, but QTabBar::tabRect() RETURNS THE
#   RECT INCLUDING THE MARGIN (measured, scratchpad/ui4_20260910/probe.cpp), so
#   a gate built on rects reads the same numbers before and after the change
#   and would be green on both.
#
# USAGE
#   bash tests/spells/water_ui.sh
#   SHOT=C:/path/topbar.png TABSHOT=C:/path/watertab.png bash tests/spells/water_ui.sh
#   STRIPSHOT=C:/path/strip4x.png bash tests/spells/water_ui.sh
#   LODSHOT=C:/path/lodtab_lod.png LODSHOT2=C:/path/lodtab_water.png \
#       bash tests/spells/water_ui.sh
#   SRC=/path/model.nif bash tests/spells/water_ui.sh
#
# A SKIP is never a pass: every SKIP line is printed after the log.

set -u

. "$(dirname "$0")/_harness.sh"

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
EXE="${EXE:-$ROOT/release/NifSkope.exe}"
SRC="${SRC:-E:/Tools/Fallout 4/DataUnpacked/Data/meshes/SetDressing/35CourtSign/35CourtSign01.nif}"
PORT="${PORT:-42317}"
LOG="$ROOT/release/ww_waterui_test.log"

[ -x "$EXE" ] || { echo "no NifSkope.exe at $EXE"; exit 2; }
[ -f "$SRC" ] || { echo "no source NIF at $SRC"; exit 2; }

rm -f "$LOG"
WW_WATERUI_TEST=1 \
	WW_WATERUI_SHOT="${SHOT:+$(winpath "$SHOT")}" \
	WW_WATERUI_TABSHOT="${TABSHOT:+$(winpath "$TABSHOT")}" \
	WW_WATERUI_STRIPSHOT="${STRIPSHOT:+$(winpath "$STRIPSHOT")}" \
	WW_WATERUI_LODSHOT="${LODSHOT:+$(winpath "$LODSHOT")}" \
	WW_WATERUI_LODSHOT2="${LODSHOT2:+$(winpath "$LODSHOT2")}" \
	"$EXE" --port "$PORT" "$(winpath "$SRC")" >/dev/null 2>&1 &
pid=$!
for _ in $(seq 1 90); do
	[ -f "$LOG" ] && grep -q '^done$' "$LOG" 2>/dev/null && break
	kill -0 "$pid" 2>/dev/null || break
	sleep 1
done
kill "$pid" 2>/dev/null; wait "$pid" 2>/dev/null

[ -f "$LOG" ] || { echo "FAIL: the harness wrote no log"; exit 1; }
cat "$LOG"

if grep -aq '^  SKIP ' "$LOG"; then
	echo "--- skips (a SKIP is never a pass) ---"
	grep -a '^  SKIP ' "$LOG"
fi

fails=0
# read the gates back BY NAME, from a VERDICT line only: the harness prints an
# informational line carrying the same words above several of them, and taking
# the informational one is how water_flow.sh called a green gate red (lane
# WATER7, red 2 of BUILD10).
for g in "(L floor) the LOD Generation panel's strip" \
	"(L1) the LOD panel's strip carries exactly 2 tabs" "(L1) they are" "(L1 floor)" \
	"(L2) the stack carries the generator's page" "(L2) each tab's data" "(L2 floor)" \
	"(L3) selecting Water shows the water page" "(L3) ...and the marking rows" \
	"(L3) ...and the full-screen flow window's button" "(L3 floor)" \
	"(L4) the LOD panel's strip is the shared row" \
	"(L4) ...the same height as the left strip" "(L4 floor)" \
	"(L5) the two strips carry the SAME stylesheet" "(L5 floor)" \
	"(L6) the left strip has exactly 3 tabs" "(L6) ...and exactly 3 with it CLOSED" \
	"(L6) no tab named" "(L6) LeftColumnStack is back to 3 pages" "(L6 floor)" \
	"(L7) the Workspaces menu offers no" "(L7) ...and no" "(L7 floor)" \
	"(L7) there is no Water Marking dock" \
	"(L8) the LOD strip's first segment" "(L8 floor) ...and the LEFT strip" \
	"(L8) the LOD strip's two segments TOUCH" "(L8 floor) the SAME gap test" \
	"(L8 floor) ...and taking it away" \
	"(R1) every visible bar" "(R2) the menu bar and the dock tab strip" \
	"(R2 floor)" "(R3) every button in the row" "(R3) ...and the calibration" \
	"(R3 floor) the SAME test goes red" "(R3 floor) ...and taking it away" \
	"(R4) the skin states the height" "(R4) ...and the only horizontal rule" \
	"(R4) ...and the menu arrow is centred" \
	"(R4) the bar's own box is taken away" "(R4) ...and the bars actually carry it" \
	"(R4 floor)" "(R5) the search row" \
	"(S floor) every segment of the strip" "(S floor) ...and the SAME scan finds nothing" \
	"(S1) the strip starts" "(S2) ...and ends" "(S3) the first segment" \
	"(S4) the last segment" "(S5) the segments TOUCH" "(S6) and the ROW did not move" \
	"(S floor) the SAME five go red" "(S floor) ...and taking it away" \
	"(S5 floor) the SAME gap test" "(S5 floor) ...and taking it away" \
	"(A floor) the row carries menu buttons" "(A1) every menu button's arrow" \
	"(A1 floor) the SAME test goes red" "(A1 floor) ...and taking it away" \
	"(A2) every button with a menu" "(A2 floor) ...and no button WITHOUT a menu" \
	"(M floor) every painted title in the menu bar" "(M floor) ...and the SAME scan finds nothing" \
	"(M1) the menu bar is still exactly" "(M2) every title's text is on the row's centre" \
	"(M2) ...and the five agree" "(M3) the menu bar's own size hint" \
	"(M3) ...and every other bar in the row" "(M4) the row's menu-item rule" \
	"(M4 floor)" "(M4) the way back puts the titles back high" \
	"(M5 floor) the SAME test goes red" "(M5 floor) ...and taking it away" \
	"(M6) the skin states one top" "(M6) ...and nothing horizontal"; do
	line="$(grep -a -F "$g" "$LOG" | grep -aE '^  (ok|FAIL) ' | head -1)"
	if [ -z "$line" ]; then
		echo "FAIL: gate '$g' did not run"; fails=$((fails+1))
	elif ! printf '%s' "$line" | grep -aq '^  ok '; then
		echo "FAIL: gate '$g' is red"; fails=$((fails+1))
	fi
done

COUNT="$(grep -a ' checks, ' "$LOG" | tail -1 | awk '{print $1}')"
# The named gates above plus the floors and the three picture checks.
# Arithmetic, not a wish, re-counted by lane UI4: T-group 15, R-group 19 (1
# row-height floor, 2 for R1, 2 for R2, 6 for R3 including BOTH halves of its
# live floor, 5 for R4, 1 for R5, 2 for the top-strip grab), S-group 10 (2
# floors, S1..S6, and BOTH halves of the live flush floor), the document check
# 1, the strip crop 1.  Measured: UI3's run was 37 with two pictures; the S
# group and its crop make it 48 with all three, 47 with no crop, 44 with no
# pictures at all.  41 is three below the smallest of those, so losing a check
# still goes red however the spell is called.
# LANE WATER8 re-counted it: group T (15 checks, one of them a picture) is
# retired and group L takes its place -- 30 with both LOD-panel pictures, 28
# with none, and 5 of those 30 skip if the shared bar row is off
# (UI/CompactTopBars false), so the smallest run that is still WORKING is
# 44 - 14 + 23 = 53.  48 is five below that, so losing a check still goes red
# however the spell is called, and the first green run tightens it.
# LANE UI5 re-counted it against the MEASURED run, not the model: lane
# WATER8-GATE's own chain read 59 checks on the 21:02:12 exe (its DONE line),
# where the arithmetic above had predicted 53.  Group M -- the menu bar's
# titles on the row's centre line -- adds 14: 2 floors, M1, 2 for M2, 2 for
# M3, 2 for M4 plus its own floor, BOTH halves of the live M5 floor, and 2
# for M6.  So a full run reads 73 and the smallest run that is still WORKING
# (no pictures asked for, 53 + 14) reads 67.  62 is five below that, and it
# is also above 59 -- so a build that lost group M entirely goes red on the
# count alone, however the spell is called.  All 14 skip by name when
# UI/CompactTopBars is off.
# LANE UI6 re-counted it against UI5's MEASURED 76: group S gains BOTH halves
# of its own gap floor (+2), group L gains L8's missing half and its two floor
# halves (+3), and the new group A -- the menu arrows' air -- adds 6 (its
# readable floor, A1, BOTH halves of A1's live floor, A2 and A2's floor). So a
# full run reads 87 and the smallest run that is still WORKING (no pictures
# asked for: 67 + 11) reads 78. 72 is six below that and well above UI5's 76
# only when the pictures are on, so a build that lost group A entirely goes red
# on the count alone however the spell is called. All 6 of group A skip by name
# when UI/CompactTopBars is off, exactly as group R's do.
echo "checks run: ${COUNT:-none} (floor 72)"
case "${COUNT:-}" in
	''|*[!0-9]*) echo "FAIL: the log carries no check count"; fails=$((fails+1)) ;;
	*) [ "$COUNT" -ge 72 ] || { echo "FAIL: only $COUNT checks ran, floor is 72"; fails=$((fails+1)); } ;;
esac
grep -aq '^PASS$' "$LOG" || { echo "FAIL: the harness did not pass"; fails=$((fails+1)); }

echo
if [ "$fails" -eq 0 ]; then
	echo "water_ui.sh PASS"
	exit 0
fi
echo "water_ui.sh FAIL ($fails)"
exit 1
