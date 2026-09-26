#!/bin/bash
#
# .lodi VERSION 7 -- the group table and the per-vertex sky stream.
#
# WHY THIS EXISTS
#
# bungo, 2026-09-18 09:4x: "The houses should be one object each though, for
# identity" and "We need per vertex sky visbility too".
#
# Two additions to a file format that four other readers already parse, so the
# thing that can go wrong is not "does the new payload work" -- it is "did the
# new payload move a byte that was not mine to move", and after that "is the
# picture I am about to be shown the numbers the file actually carries". This
# gate answers both before any picture is made:
#
#   G1 THE WAY BACK. `--lodi-v6` writes today's v6 bytes. The rung exe's plain
#      bake and the new exe's `--lodi-v6` bake are compared BYTE FOR BYTE, both
#      files, and a version-7 file read by the rung's own reader is REFUSED BY
#      NAME. A version word that is only cosmetic is not a way back.
#
#   G2 THE GROUPING, four pre-registered refuters, each with a control that
#      must go RED: a SCOL's parts are one group within a chunk; two houses
#      across a street are two ids; a tree beside a wall keeps its own; the
#      components cover every architecture placement exactly once. Plus the two
#      invariants the rest of the tree depends on -- `identity` is untouched and
#      still unique, and group ids are dense per chunk from 0.
#
#   G3 THE SKY STREAM. s4.8's layout exactly, and the stream is measured against
#      the 0x11 placement byte it must agree with. It agrees in the MEDIAN
#      (0.38) and in CORRELATION (r = 0.985, against the AO stream's 0.988 on
#      the same file) but only 72% of per-placement means land within 2, where
#      AO manages 92%. That gap is measured, explained and pre-registered in the
#      lane report s1.5 and s3 -- the byte averages the .BTO chunk mesh's
#      vertices and the stream averages the authored LOD mesh's, and sky varies
#      across one object far more than AO does, so the same population
#      difference moves a sky mean much further. The gate is therefore set on
#      the median, the correlation, and the flat-slice subset where the two
#      populations CANNOT disagree -- not on a 95% bar that the mechanism says
#      is not reachable. A shuffled stream scores r = 0.019 against it.
#
#   G4 THE VIEWER. `identity` draws the GROUP, `placement` draws what `identity`
#      drew before, `sky` draws the stream on a v7 file and the flat byte on a
#      v6 one -- and SAYS WHICH, in a note line read back from what was
#      uploaded, never from the version word alone.
#
#   G5 THE NEIGHBOURS, run with NEIGHBOURS=1. Owners and their standing counts
#      are listed below so a regression names itself.
#
# FIXTURES. A v7 pair and a v6 pair from the LODIV7 lane's own bakes of chunk
# 4.4.-12. Large and untracked; when one is absent the gate SKIPS with the path
# NAMED and does not pass. Re-point with V7DIR=, V6DIR=, RUNG=.
#
# ONE NifSkope at a time, second monitor, never a desktop capture.
#
# USAGE
#   bash tests/spells/lodi_v7.sh
#   EXE=<exe> OUT=<dir> PORT=<n> NEIGHBOURS=1 bash tests/spells/lodi_v7.sh

set -u

. "$(dirname "$0")/_harness.sh" 2>/dev/null || true
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
EXE="${EXE:-$ROOT/release/NifSkope.exe}"
# ---- settings scope (lane FIX1 fix 4, 2026-09-26) ----------------------------
# Every NifSkope WINDOW this spell opens runs in its OWN QSettings scope -- never
# bungo's profile, and never a scope the caller's environment names. A gate that
# inherits the user's settings measures the profile, not the code (lane GATEFIX2,
# native_lighting.sh: his "Vertex Color" unticked in the Lighting shading mode
# turned the .BTR water white). WW_SETTINGS_SCOPE=<scope> moves the whole tree to
# HKCU\Software\NifTools\NifSkope 2.0 <scope> (src/harnesswindow.cpp).
# fresh_scope wipes it before EACH window (a window saves its layout on close, so
# the next would open at another size) and seeds Settings/Version=1: an EMPTY
# scope is a first install, whose settings dialog saves every pane's widget value
# (Background 46,46,46, src/ui/settingspane.cpp). SEED_REG=<file.reg> (keys
# already under the scope) is imported after the seed -- a red control's way to
# render under a chosen profile. The scope is wiped at exit. The -no-gui CLI
# calls are not windows and are left as they were.
SCOPE="${SCOPE:-lodi_v7}"
case "$SCOPE" in
	''|*[!A-Za-z0-9_-]*) echo "REFUSED: SCOPE='$SCOPE' is not a usable settings scope name"; exit 2 ;;
esac
[ ${#SCOPE} -le 40 ] || { echo "REFUSED: SCOPE='$SCOPE' is longer than 40"; exit 2; }
REGKEY="HKCU\\Software\\NifTools\\NifSkope 2.0 $SCOPE"
wipe_scope() { reg delete "$REGKEY" //f > /dev/null 2>&1 || true; }
fresh_scope() {  # wipe + seed, then print the name: WW_SETTINGS_SCOPE="$(fresh_scope)"
	wipe_scope
	reg add "$REGKEY\\Settings" //v Version //t REG_SZ //d 1 //f > /dev/null 2>&1 || true
	# not a Game Manager first install either: version 0 shows an opaque progress dialog on the
	# PRIMARY monitor (src/gamemanager.cpp prog_dialog) before any WW window placement exists
	reg add "$REGKEY" //v "Game Manager Version" //t REG_DWORD //d 2 //f > /dev/null 2>&1 || true
	# ...and the game manager state an empty scope never gets (its Game Folders come out empty):
	# Fallout 4's path and folders read from THIS MACHINE, never from the user's profile
	local gm; gm="$(mktemp)"
	python "$(dirname "$0")/settings_scope_game.py" "$SCOPE" "$(cygpath -w "$gm")" > /dev/null 2>&1 \
		&& reg import "$(cygpath -w "$gm")" > /dev/null 2>&1
	rm -f "$gm"
	if [ -n "${SEED_REG:-}" ]; then reg import "$(winpath "$SEED_REG")" > /dev/null 2>&1 || true; fi
	printf '%s' "$SCOPE"
}
wipe_scope
trap wipe_scope EXIT
RUNG="${RUNG:-$ROOT/release/NifSkope.before_lodiv7.exe}"
PORT="${PORT:-42947}"
PY="${PY:-$(command -v python || echo /c/Windows/py)}"

LANE="$ROOT/scratchpad/lodiv7_20260918"
V7DIR="${V7DIR:-$LANE/v7/nat/FO4CSLOD/Commonwealth}"
V6DIR="${V6DIR:-$LANE/g1_new_v6/nat/FO4CSLOD/Commonwealth}"
V7I="$V7DIR/Commonwealth.lodi"; V7O="$V7DIR/Commonwealth.lodo"
# WW_RENDER_SHOT only arms when a FILE is on the command line -- the hook hangs off
# `completeLoading`, so an exe launched with no scene renders nothing, quits never
# and wedges the one-instance rule. Root MISTAKES 2026-09-18 10:39.
V="$ROOT/scratchpad/viewfix_20260917"
LODL="${LODL:-$V/chunkB/lodl/FO4CSLOD/Commonwealth/Commonwealth.lodl}"
SHEETS="${SHEETS:-$ROOT/scratchpad/slab1_20260918/after/vt/FO4CSLOD/Commonwealth}"
REGION="4,-12,7,-9,0"
V6I="$V6DIR/Commonwealth.lodi"; V6O="$V6DIR/Commonwealth.lodo"
OUT="${OUT:-$ROOT/scratchpad/lodi_v7_gate}"
REF="$ROOT/tests/spells/lodi_v7_refuters.py"
DEC="$ROOT/tests/spells/lodgen_native_decode.py"

# the framing lodl_channels.sh uses, so a picture from this gate and a picture
# from that one are the same camera
CX=24900; CY=-41300; CZ=450; ORTHO=2600; VIEW=8; SIZE=1400x1091

pass=0; fail=0; skip=0
ck () {  # ck <name> <condition-rc> <detail>
	if [ "$2" -eq 0 ]; then echo "  ok   $1 -- $3"; pass=$((pass+1));
	else echo "  FAIL $1 -- $3"; fail=$((fail+1)); fi
}

[ -x "$EXE" ] || { echo "SKIP: no NifSkope.exe at $EXE"; exit 2; }
for f in "$V7I" "$V7O" "$REF" "$DEC" "$LODL"; do
	[ -f "$f" ] || { echo "SKIP: fixture missing -- $f"; exit 2; }
done
if tasklist 2>/dev/null | grep -qi -E "Fallout4"; then
	echo "SKIP: Fallout4 is up -- no exe runs while the game holds the files"; exit 2
fi

if ! type winpath >/dev/null 2>&1; then
	winpath() {
		case "$1" in
			/[a-zA-Z]/*) local d="${1:1:1}"; printf '%s' "${d}:${1:2}" ;;
			*) printf '%s' "$1" ;;
		esac
	}
fi
mkdir -p "$OUT"; rm -f "$OUT"/*.png "$OUT"/*.log

echo "exe   $EXE  ($(stat -c '%y %s B' "$EXE" 2>/dev/null))"
echo "v7    $V7I"
echo "v6    $V6I"
echo "out   $OUT"

# ---- G1 the way back ---------------------------------------------------------
echo "G1 the way back to v6"
if [ -f "$V6I" ] && [ -x "$RUNG" ]; then
	# the lane bakes the pair; this gate compares what it left
	G1A="$LANE/g1_rung/nat/FO4CSLOD/Commonwealth"
	if [ -f "$G1A/Commonwealth.lodi" ]; then
		for ext in lodi lodo; do
			if cmp -s "$G1A/Commonwealth.$ext" "$V6DIR/Commonwealth.$ext"; then
				ck "G1 .$ext byte-identical" 0 "$(stat -c %s "$V6DIR/Commonwealth.$ext") B, sha $(sha1sum "$V6DIR/Commonwealth.$ext" | cut -c1-12)"
			else
				ck "G1 .$ext byte-identical" 1 "the rung's bake and --lodi-v6 differ"
			fi
		done
	else
		echo "  SKIP G1 byte identity -- no rung bake at $G1A"; skip=$((skip+1))
	fi
	# the rung's READER must refuse the v7 file BY NAME
	msg="$("$RUNG" -no-gui lodgen "$(winpath "$V7O")" --native-verify "$(winpath "$V7O")" "$(winpath "$V7I")" 2>&1 | head -2)"
	case "$msg" in
		*"version 7; this reader knows 3, 4, 5 and 6"*)
			ck "G1 the old reader refuses v7 by name" 0 "$(echo "$msg" | tail -1)" ;;
		*)  ck "G1 the old reader refuses v7 by name" 1 "it said: $msg" ;;
	esac
else
	echo "  SKIP G1 -- no v6 fixture or no rung exe at $RUNG"; skip=$((skip+1))
fi

# ---- G2 + G3 the refuters ----------------------------------------------------
echo "G2 the grouping, G3 the sky stream -- refuters and their red controls"
"$PY" "$REF" "$(winpath "$V7O")" "$(winpath "$V7I")" > "$OUT/refuters.log" 2>&1
rc=$?
sed -n '2,$p' "$OUT/refuters.log" | grep -E '^  (ok|FAIL|red|NOT-RED)' | sed 's/^/  /'
ck "G2/G3 every refuter green, every control red" $rc "$(tail -2 "$OUT/refuters.log" | head -1)"

# the independent Python reader must also accept the file outright
"$PY" "$DEC" "$(winpath "$V7O")" "$(winpath "$V7I")" > "$OUT/decode.log" 2>&1
grep -q "RESULT PASS" "$OUT/decode.log"
ck "G3 the Python reader accepts the v7 pair" $? "$(grep -E '^[0-9]+ checks' "$OUT/decode.log" | tail -1)"

# and the two readers must AGREE on a number the C++ prints
cnum="$("$EXE" -no-gui lodgen "$(winpath "$V7O")" --native-verify "$(winpath "$V7O")" "$(winpath "$V7I")" 2>/dev/null \
	| grep -E '^lodi (groups|vertexSkyBytesTotal) ' | awk '{print $2"="$3}' | sort | tr '\n' ' ')"
pnum="$("$PY" "$ROOT/tests/spells/lodi_v7_numbers.py" "$(winpath "$V7I")")"
[ "$(echo "$cnum" | tr -s ' ')" = "$(echo "$pnum " | tr -s ' ')" ]
ck "G3 the C++ and Python readers agree" $? "C++ [$cnum] Python [$pnum]"

# ---- G4 the viewer -----------------------------------------------------------
echo "G4 the viewer"
shot () {  # shot <tag> <channel> <lodi>
	# the .lodl is the SCENE and it is not optional: no file, no render, no exit.
	# `timeout` is not belt-and-braces either -- it is the only thing that keeps a
	# stuck window from wedging every later gate in the tree.
	WW_LODL_OBJECTS="$(winpath "$3")" \
	WW_LODL_SHEETS="$(winpath "$SHEETS")" \
	WW_LODL_REGION="$REGION" WW_LODI_LEVEL=0 WW_LODI_SLOT=0 \
	WW_LODL_CHANNEL="$2" \
	WW_RENDER_SHOT="$(winpath "$OUT/$1.png")" \
	WW_RENDER_SIZE="$SIZE" WW_RENDER_CENTER="$CX,$CY,$CZ" \
	WW_RENDER_ORTHO="$ORTHO" WW_RENDER_VIEW="$VIEW" WW_RENDER_FLAT=1 WW_RENDER_CLEAN=1 \
	WW_WINDOW_AT=1960,40 \
		WW_SETTINGS_SCOPE="$(fresh_scope)" timeout 600 "$EXE" --port "$PORT" "$(winpath "$LODL")" > "$OUT/$1.log" 2>&1
	[ -s "$OUT/$1.png" ] || echo "  (no picture for $1, exit $?)"
	# PRECAUTION, not a fix for anything measured (MISTAKES 2026-09-18 18:3x):
	# nothing this gate starts may outlive its own shot. The one-instance rule
	# means a window left standing makes every later gate meaningless, and the
	# gate should say so here rather than produce blank pictures afterwards.
	# A NifSkope with no `--port` is bungo's window; this gate never starts one
	# without a port, so anything still up after a shot is this gate's own.
	if tasklist 2>/dev/null | grep -qi "NifSkope.exe"; then
		echo "  STOP: a NifSkope is still up after shot '$1' (this gate's port $PORT)."
		echo "        Unwedge it WITHOUT killing it by sending it the scene over its own"
		echo "        port -- a UTF-16LE UDP datagram 'NifSkope::open <win path>' to"
		echo "        127.0.0.1:$PORT (src/main.cpp IPCsocket). Then re-run this gate."
		return 9
	fi
}
note () { grep -o "WW_LODL_CHANNEL=.*" "$OUT/$1.log" | head -1; }

for c in identity placement sky; do shot "v7_$c" "$c" "$V7I"; done
[ -f "$V6I" ] && shot "v6_sky" sky "$V6I"

n="$(note v7_identity)"
case "$n" in
	*"the GROUP (.lodi v7 0x100)"*) ck "G4 identity draws the group" 0 "$n" ;;
	*) ck "G4 identity draws the group" 1 "${n:-no note line}" ;;
esac
n="$(note v7_placement)"
case "$n" in
	*"placement"*) ck "G4 placement draws the unique identity" 0 "$n" ;;
	*) ck "G4 placement draws the unique identity" 1 "${n:-no note line}" ;;
esac
n="$(note v7_sky)"
case "$n" in
	*"PER-VERTEX SKY STREAM"*) ck "G4 sky draws the stream on a v7 file" 0 "$n" ;;
	*) ck "G4 sky draws the stream on a v7 file" 1 "${n:-no note line}" ;;
esac
if [ -f "$V6I" ]; then
	n="$(note v6_sky)"
	case "$n" in
		*"no per-vertex stream"*|*"PLACEMENT BYTE"*) ck "G4 sky falls back on a v6 file AND says so" 0 "$n" ;;
		*) ck "G4 sky falls back on a v6 file AND says so" 1 "${n:-no note line}" ;;
	esac
	# the fallback must not be the same PICTURE as the stream
	if [ -f "$OUT/v7_sky.png" ] && [ -f "$OUT/v6_sky.png" ]; then
		cmp -s "$OUT/v7_sky.png" "$OUT/v6_sky.png"
		[ $? -ne 0 ]
		ck "G4 the stream and the byte are different pictures" $? "a per-vertex channel that renders byte-identically to the flat one is NOT WIRED"
	fi
fi
# identity and placement must differ -- if they do not, the group is not drawn
if [ -f "$OUT/v7_identity.png" ] && [ -f "$OUT/v7_placement.png" ]; then
	cmp -s "$OUT/v7_identity.png" "$OUT/v7_placement.png"
	[ $? -ne 0 ]
	ck "G4 identity and placement are different pictures" $? "the group and the per-placement id are not the same colouring"
fi

# ---- G5 the neighbours -------------------------------------------------------
echo "G5 the neighbours (NEIGHBOURS=1 to run; standing counts are the owners')"
N7="lodgen_native.sh lodl_channels.sh:48/0 native_open.sh:17/0/2 render_shot.sh:82/0 lodl_open.sh:23/0 lodgen_slab.sh:16/0"
if [ "${NEIGHBOURS:-0}" = "1" ]; then
	for spec in $N7; do
		s="${spec%%:*}"
		[ -f "$ROOT/tests/spells/$s" ] || { echo "  SKIP $s -- absent"; skip=$((skip+1)); continue; }
		bash "$ROOT/tests/spells/$s" > "$OUT/$s.log" 2>&1
		echo "  $s rc=$? -- $(tail -1 "$OUT/$s.log")"
	done
else
	for spec in $N7; do echo "  (not run) $spec"; done
	echo "  native_lighting.sh is 14/3 BEFORE this lane and is NOT this lane's to fix"
fi

echo "lodi_v7: $pass ok, $fail failed, $skip skipped"
[ "$fail" -eq 0 ] || exit 1
exit 0
