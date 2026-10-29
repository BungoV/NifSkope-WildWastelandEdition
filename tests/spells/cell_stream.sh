#!/bin/bash
# lane CELLSTREAM1: the exterior cell view streams a 5x5 of full cells round the camera (src/cellstream.h).
#
#   (a) a Concord walk of 20 crossings: after each step the loaded cells are the 5x5 round the block's centre;
#       the step's worst streaming frame and write slice are in the dump
#   (b) the picture after streaming in == the fixed-block open at the same centre and camera (far LOD off)
#   (c) far LOD on: no hole and no double draw either side of the loaded / unloaded border
#   (d) private bytes after crossing 20 within 5% of the first full load
#   (e) lane STREAMTEX1: the texture pool's census per crossing (cell_stream_check.py; SKIP on an older exe)
#   reds: WW_CELL_STREAM_RED=nounload must fail (d) (nohandover retired 10-28, FRAMESPEED2: confirmed live)
#
# usage: bash tests/spells/cell_stream.sh            every phase
#        PHASE="walk check" bash tests/spells/cell_stream.sh
#        RECHECK=1 bash tests/spells/cell_stream.sh  the checker on the files already written
# Runs NifSkope through withlock (one at a time) with MEMCAP=15.
set -u

. "$(dirname "$0")/_harness.sh"

REPO="$(cd "$(dirname "$0")/../.." && pwd)"
EXE="${EXE:-$REPO/release/NifSkope.exe}"
SCOPE="${SCOPE:-cell_stream}"
REGKEY="HKCU\\Software\\NifTools\\NifSkope 2.0 $SCOPE"
wipe_scope() { reg delete "$REGKEY" //f > /dev/null 2>&1 || true; }
fresh_scope() {
	wipe_scope
	reg add "$REGKEY\\Settings" //v Version //t REG_SZ //d 1 //f > /dev/null 2>&1 || true
	reg add "$REGKEY" //v "Game Manager Version" //t REG_DWORD //d 2 //f > /dev/null 2>&1 || true
	local gm; gm="$(mktemp)"
	python "$(dirname "$0")/settings_scope_game.py" "$SCOPE" "$(cygpath -w "$gm")" > /dev/null 2>&1 \
		&& reg import "$(cygpath -w "$gm")" > /dev/null 2>&1
	rm -f "$gm"
	printf '%s' "$SCOPE"
}
wipe_scope
trap wipe_scope EXIT
ESM="${ESM:-X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm}"
DATA="${DATA:-E:/Tools/Fallout 4/DataUnpacked/Data}"
OUT="${OUT:-$REPO/scratchpad/cell_stream/gate}"
LOG="$OUT/cell_stream.log"
PORT="${PORT:-14871}"
SPEC="$REPO/tests/fixtures/empty.wwcell"
SIZE="${SIZE:-1280x720}"
PHASE="${PHASE:-cams walk reds pictures check}"
RECHECK="${RECHECK:-0}"
[ "$RECHECK" = 1 ] && PHASE="check"
LOCK="${LOCK:-/e/Projects/Claude/notes/lanes_20261001/withlock.sh}"
# the picture camera: Concord's street corner, a player's eye (judged in-bounds by camcheck)
CAM="${CAM:--57800,70325,6290,-58200,70250,6280,70}"
CAMTOOLS="${CAMTOOLS:-/e/Projects/Claude/notes}"
TRIS="${TRIS:-$CAMTOOLS/gishade1/tris_concord.npy}"
NAV="${NAV:-$CAMTOOLS/gishade1/nav/nav_m15_17.npz}"
CAMCHECK="${CAMCHECK:-$CAMTOOLS/neongi1/tools/camcheck.py}"
CX=-15; CY=17

mkdir -p "$OUT"
[ "$RECHECK" = 1 ] || : > "$LOG"
say() { echo "$@" | tee -a "$LOG"; }
has() { case " $PHASE " in *" $1 "*) return 0;; esac; return 1; }
[ -x "$EXE" ] || { echo "no NifSkope.exe at $EXE"; exit 2; }
say "cell_stream.sh  $(date '+%Y-%m-%d %H:%M:%S')   phase: $PHASE   port $PORT"

STILL=( WW_TAA=0 WW_CELL_DOF=0 WW_CELL_MBLUR=0 WW_CELL_IS=0 WW_CELL_PARTICLES=0 WW_CELL_GRASS=0 WW_CELL_NOGRID=1 )
LD=( WW_LOOKDEV=1 WW_LOOKDEV_WEATHER=CommonwealthClear WW_LOOKDEV_HOUR=12 WW_LOOKDEV_SUN=0 WW_LOOKDEV_GROUND=0
	WW_LOOKDEV_SKY=0 WW_LOOKDEV_CLOUDS=0 WW_LOOKDEV_MOON=0 WW_LOOKDEV_FOG=0 WW_LOOKDEV_SHADOWS=0 WW_LOOKDEV_PLUGINS="$ESM" )

# a step: the eye 3000 units over cell (x, y)'s middle, looking east and down
step() { awk -v x="$1" -v y="$2" 'BEGIN { ex = (x + 0.5) * 4096; ey = (y + 0.5) * 4096;
	printf "%.1f %.1f 9000 %.1f %.1f 8400 70\n", ex, ey, ex + 800, ey }'; }
# the walk: the first load, then 20 one-cell crossings that close on the start
#   east 1, west 1, east 3, north 2, west 5, south 4, east 2, north 2
WALK="0,0 1,0 0,0 1,0 2,0 3,0 3,1 3,2 2,2 1,2 0,2 -1,2 -2,2 -2,1 -2,0 -2,-1 -2,-2 -1,-2 0,-2 0,-1 0,0"
writepath() {   # writepath <file> <count or all> [final camera]
	local f="$1" n="$2" k=0
	: > "$f"
	for c in $WALK; do
		[ "$n" != all ] && [ "$k" -ge "$n" ] && break
		step $(( CX + ${c%,*} )) $(( CY + ${c#*,} )) >> "$f"
		k=$((k+1))
	done
	[ -n "${3:-}" ] && echo "$3" | tr ',' ' ' >> "$f"
}

# GDB=1: under gdb with the release heap, every thread's stack on a fault (needs a relink with symbols)
DBG=()
[ "${GDB:-0}" = 1 ] && DBG=( /c/msys64/ucrt64/bin/gdb.exe --batch -ex "set pagination off" -ex "set confirm off"
	-ex "set environment _NO_DEBUG_HEAP=1" -ex run -ex "thread apply all bt 25" --args )
run() {   # run <name> <cells open "x,y|n"> <path file> <env...>: one NifSkope under the lock
	local name="$1" open="$2" path="$3"; shift 3
	local shot="$OUT/$name.png" notes="$OUT/$name.notes" try rc
	rm -f "$shot" "$OUT/$name.stream.txt"
	for try in 1 2; do
		MEMCAP=15 bash "$LOCK" nifskope env "${STILL[@]}" "${LD[@]}" "$@" \
			WW_CELL_OPEN="$ESM|Commonwealth|$open" WW_CELL_DATAROOT="$DATA" WW_LODGEN_RESOURCES="$DATA" WW_CELL_LIT=1 \
			WW_CELL_STREAM_PATH="$(winpath "$path")" WW_CELL_STREAM_DUMP="$(winpath "$OUT/$name.stream.txt")" \
			WW_RENDER_SHOT="$(winpath "$shot")" WW_RENDER_SIZE="$SIZE" WW_RENDER_CLEAN=1 \
			WW_SETTINGS_SCOPE="$(fresh_scope)" timeout "${TIMEOUT:-3600}" "${DBG[@]}" "$EXE" --port "$PORT" "$(winpath "$SPEC")" > "$notes" 2>&1
		rc=$?
		cp -f "$notes" "$notes.try$try"
		say "  $name try $try rc $rc $([ -s "$shot" ] && echo picture || echo NO-PICTURE) $(grep -c '^step' "$OUT/$name.stream.txt" 2>/dev/null) steps"
		[ -s "$shot" ] && break
		sleep 5
	done
}

if has cams; then
	if [ -f "$CAMCHECK" ] && [ -f "$TRIS" ] && [ -f "$NAV" ]; then
		python "$CAMCHECK" "$TRIS" "$NAV" hcorner2 "$(echo "$CAM" | tr ',' ' ')" > "$OUT/camcheck.txt" 2>&1
		sed 's/^/  /' "$OUT/camcheck.txt" | tee -a "$LOG"
	else
		say "  no camcheck files ($CAMCHECK, $TRIS, $NAV)"
	fi
fi

if has walk; then
	say "== walk: Concord $CX,$CY, 20 crossings, far LOD on"
	writepath "$OUT/walk.path" "${WALKN:-all}" "$CAM"
	run walk "$CX,$CY|1" "$OUT/walk.path" WW_CELL_STREAM=1 WW_CELL_FARLOD=1
fi

if has reds; then
	say "== reds"
	writepath "$OUT/red_nounload.path" 5
	run red_nounload "$CX,$CY|1" "$OUT/red_nounload.path" WW_CELL_STREAM=1 WW_CELL_FARLOD=1 WW_CELL_STREAM_RED=nounload
fi

if has pictures; then
	say "== pictures: streamed in from 3 cells west vs the fixed 5x5 open, far LOD off"
	{ for x in -18 -17 -16 -15; do step "$x" "$CY"; done; echo "$CAM" | tr ',' ' '; } > "$OUT/streamed.path"
	run streamed "$((CX-3)),$CY|1" "$OUT/streamed.path" WW_CELL_STREAM=1 WW_CELL_FARLOD=0
	echo "$CAM" | tr ',' ' ' > "$OUT/fixed.path"
	run fixed "$CX,$CY|5" "$OUT/fixed.path" WW_CELL_STREAM=0 WW_CELL_FARLOD=0
fi

if has check; then
	say "== check"
	python "$(dirname "$0")/cell_stream_check.py" "$OUT" 2>&1 | tee -a "$LOG"
	rc=${PIPESTATUS[0]}
	say "cell_stream.sh done rc $rc"
	exit "$rc"
fi
