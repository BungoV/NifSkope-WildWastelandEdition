#!/bin/bash
#
# THE PROBES' SKY FROM THE SKY THE VIEW DRAWS (lane SKYPIC1, 2026-10-11; src/probesky.h, PRTP_PLAN 2as, deck C9).
#
# SKY1 gave each probe the weather's six ambient colours, weighed by what of the sky each octant sees. With
# WW_CELL_SKYPIC=1 (ships OFF) the view captures the sky it draws (the Lookdev dome, the stars, the cloud layers;
# not the sun, the moon or the glare: those are the sun term) as light into six float faces, and each probe's sky
# on an axis becomes sum over octants of (visibility x tint x that octant's drawn light x cosine x solid angle).
#
# Per cell (an exterior: Commonwealth -17,23, FARLOD2's; an interior that shows the sky: ConcordMuseum01), one
# bake, then relights of it (WW_CELL_GI_FROM), Lookdev CommonwealthClear hour 12 day 4, the cloud clock pinned:
#   off   the new exe, switch unset (dump_off/)  } every .bin the rung dumps byte for byte, the picture the same
#   rung  the exe from before the lane (dump_rung/)
#   on    WW_CELL_SKYPIC=1 (dump_on/): cell_skypic_check.py F Q S rebuild the table and the probes' sky
#   flat  WW_CELL_SKYPIC_FLAT=0.3,0.45,0.6 with the switch on (the capture is that one colour, drawn and integrated)
#         and off (SKY1's six colours all that colour): K, their gi_sky.bin agree (the known-answer control)
#   reds  WW_CELL_SKYPIC_RED=flipz (the picture read upside down), nocos (no cosine): each must FAIL Q and S
#   zen   (exterior) the view tilted to the zenith, 0 EV, view Standard, no clouds: D, the drawn pixel at the
#         centre is the capture's +Z centre sRGB-encoded (and not its -Z centre)
# Then the pictures he judges (PHASE pics): the full sky rows on, noon and dusk, before (off) and after (on).
#
# USAGE  bash tests/spells/cell_skypic.sh    (EXE= RUNG= absolute; OUT=; PHASE="gate pics"; CELLS="ext museum")

set -u
. "$(dirname "$0")/_harness.sh"

REPO="$(cd "$(dirname "$0")/../.." && pwd)"
EXE="${EXE:-$REPO/release/NifSkope.exe}"
RUNG="${RUNG:-$REPO/release/NifSkope.before_skypic1.exe}"
LAUNCH="${LAUNCH:-/e/Projects/Claude/notes/finalfix/launch.sh}"
SCOPE="${SCOPE:-cell_skypic}"
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
OUT="${OUT:-$REPO/scratchpad/skypic1/gate}"
PORT="${PORT:-14761}"
SPEC="$REPO/tests/fixtures/empty.wwcell"
SIZE="${SIZE:-960x600}"
PHASE="${PHASE:-gate pics}"
CELLS="${CELLS:-ext museum}"
DUSK="${DUSK:-19}"
CHK="python $(dirname "$0")/cell_skypic_check.py"
declare -A OPEN=( [ext]="Commonwealth|-17,23|1" [museum]="interior|ConcordMuseum01" )
declare -A CAM=( [ext]="-67928,95613,7250/5/1800" [museum]="-1647,1520,-78/3/598" )
declare -A TILT=( [ext]="-15" [museum]="0" )
mkdir -p "$OUT"; OUT="$(cd "$OUT" && pwd)"; LOG="$OUT/cell_skypic.log"; : > "$LOG"
say() { echo "$@" | tee -a "$LOG"; }
has() { case " $PHASE " in *" $1 "*) return 0;; esac; return 1; }
fails=0
check() { if [ "$2" = "1" ]; then say "  PASS  $1"; else say "  FAIL  $1"; fails=$((fails+1)); fi; }
say "cell_skypic.sh  $(date '+%Y-%m-%d %H:%M:%S')  exe $(sha1sum "$EXE" | cut -c1-8)  rung $(sha1sum "$RUNG" | cut -c1-8)  phase: $PHASE"
newer=1
for s in src/probesky.cpp src/probesky.h src/gl/lookdevstage.cpp src/gl/lookdevstage.h src/glview.cpp src/probegi.cpp src/cellview.cpp; do
	[ "$REPO/$s" -nt "$EXE" ] && { say "  $s is NEWER than the exe"; newer=0; }
done
check "the exe is newer than every source this gate covers" "$newer"
cmp -s "$REPO/res/shaders/lookdev_output.glsl" "$(dirname "$EXE")/shaders/lookdev_output.glsl"
check "the shader beside the exe is the source's (lookdev_output.glsl)" "$([ $? = 0 ] && echo 1 || echo 0)"

LD=( WW_LOOKDEV=1 WW_LOOKDEV_WEATHER=CommonwealthClear WW_LOOKDEV_HOUR=12 WW_LOOKDEV_DAY=4 WW_LOOKDEV_CLOUDTIME=0
	WW_LOOKDEV_SKY=1 WW_LOOKDEV_CLOUDS=1 WW_LOOKDEV_SUN=0 WW_LOOKDEV_MOON=0 WW_LOOKDEV_GROUND=0 WW_LOOKDEV_FOG=0
	WW_LOOKDEV_SHADOWS=0 WW_LOOKDEV_PLUGINS="$ESM" WW_CELL_IS=0 WW_CELL_GI_PASSES=1 )

shoot() {   # [XE=<exe>] [PITCH=] shoot <cell> <tag> <cam x,y,z/view/dist> <env...>
	local cell="$1" tag="$2" c="$3"; shift 3
	local run="$OUT/$cell" shot="$OUT/$cell/$tag.png" notes="$OUT/$cell/$tag.notes" exe="${XE:-$EXE}" rest
	rest="${c#*/}"
	mkdir -p "$run"
	rm -f "$shot" "$notes"
	env "${LD[@]}" WW_CELL_OPEN="$ESM|${OPEN[$cell]}" "$@" ${PITCH:+WW_RENDER_PITCH=$PITCH} \
		WW_RENDER_CENTER="${c%%/*}" WW_RENDER_VIEW="${rest%%/*}" WW_RENDER_DIST="${rest#*/}" WW_RENDER_FOV=70 \
		WW_CELL_DATAROOT="$DATA" WW_CELL_PROBES="$(winpath "$run/probes.tsv")" WW_CELL_PROBES_HIDE=1 WW_CELL_LIT=1 \
		WW_RENDER_SHOT="$(winpath "$shot")" WW_RENDER_SIZE="$SIZE" WW_RENDER_CLEAN=1 \
		WW_SETTINGS_SCOPE="$(fresh_scope)" bash "$LAUNCH" "$notes" "$exe" --port "$PORT" "$(winpath "$SPEC")" > /dev/null 2>&1
	[ -s "$shot" ] && echo 1 || echo 0
}
pixdiff() {   # pixels over 2 levels, largest level, mean level
	python -c "import sys,numpy as np;from PIL import Image
a,b=(np.asarray(Image.open(f).convert('RGB'),int) for f in sys.argv[1:3]);d=np.abs(a-b).max(2)
print(int((d>2).sum()),int(d.max()),round(float(np.abs(a-b).mean()),3))" "$(winpath "$1")" "$(winpath "$2")" 2>/dev/null
}

for cell in $CELLS; do
	run="$OUT/$cell"; cam="${CAM[$cell]}"
	from=( WW_CELL_GI=1 WW_CELL_GI_FROM="$(winpath "$run/bake")" )
	if has gate; then
		say "== $cell (${OPEN[$cell]}): bake"
		rm -rf "$run"; mkdir -p "$run"
		check "$cell: the bake ran and shot" "$(PITCH=${TILT[$cell]} shoot $cell lit "$cam" WW_CELL_GI=1 \
			WW_CELL_PROBE_SOUP="$(winpath "$run/soup.psp")" WW_CELL_PROBE_BAKE="$(winpath "$run/bake")" \
			WW_CELL_GI_DUMP="$(winpath "$run/dump")")"
		grep -h "  gi: \|gi sky\|interior" "$run/lit.notes" | head -4 | cut -c1-300 | tee -a "$LOG"
		say "== $cell: off against the rung"
		o1="$(PITCH=${TILT[$cell]} shoot $cell off "$cam" "${from[@]}" WW_CELL_GI_DUMP="$(winpath "$run/dump_off")")"
		o2="$(XE="$RUNG" PITCH=${TILT[$cell]} shoot $cell rung "$cam" "${from[@]}" WW_CELL_GI_DUMP="$(winpath "$run/dump_rung")")"
		check "$cell: both relit and shot" "$([ "$o1$o2" = 11 ] && echo 1 || echo 0)"
		same=1; n=0
		for f in "$run"/dump_rung/*.bin; do
			n=$((n+1)); cmp -s "$f" "$run/dump_off/$(basename "$f")" || { say "  dump_off/$(basename "$f") differs"; same=0; }
		done
		check "$cell: switch off: every .bin the rung dumps is byte for byte ($n files)" "$([ "$n" -gt 5 ] && echo "$same" || echo 0)"
		check "$cell: switch off: no gi_skypic file and no 'gi skypic' line" \
			"$([ ! -e "$run/dump_off/gi_skypic.bin" ] && ! grep -q "gi skypic" "$run/off.notes" && echo 1 || echo 0)"
		pd="$(pixdiff "$run/off.png" "$run/rung.png")"
		say "  off.png against rung.png: ${pd:-unreadable} (pixels over 2 levels, largest level, mean level)"
		check "$cell: switch off: the picture is the rung's (largest level <= 1)" \
			"$([ -n "$pd" ] && v="${pd#* }" && [ "${v%% *}" -le 1 ] && echo 1 || echo 0)"
		say "== $cell: on"
		check "$cell: relit with the drawn sky and shot" "$(PITCH=${TILT[$cell]} shoot $cell on "$cam" "${from[@]}" WW_CELL_SKYPIC=1 \
			WW_CELL_GI_DUMP="$(winpath "$run/dump_on")")"
		grep -h "gi skypic" "$run/on.notes" | cut -c1-400 | tee -a "$LOG"
		$CHK check "$(winpath "$run")" dump_on > "$run/check_on.txt" 2>&1
		sed "s/^/  on: /" "$run/check_on.txt" | tee -a "$LOG"
		check "$cell: on: the rebuild matches (F Q S)" "$(grep -q "^skypic PASS" "$run/check_on.txt" && echo 1 || echo 0)"
		check "$cell: on: the probes' sky moved off SKY1's (gi_sky.bin differs from the switch-off dump)" \
			"$(cmp -s "$run/dump_on/gi_sky.bin" "$run/dump_off/gi_sky.bin" && echo 0 || echo 1)"
		$CHK check "$(winpath "$run")" dump_off > "$run/check_off.txt" 2>&1
		check "$cell: the switch-off dump FAILS the rebuild (it has no drawn sky)" "$(grep -q "^skypic FAIL" "$run/check_off.txt" && echo 1 || echo 0)"
		say "== $cell: the known-answer control (one colour)"
		f1="$(PITCH=${TILT[$cell]} shoot $cell flat_on "$cam" "${from[@]}" WW_CELL_SKYPIC=1 WW_CELL_SKYPIC_FLAT=0.3,0.45,0.6 \
			WW_CELL_GI_DUMP="$(winpath "$run/dump_flat_on")")"
		f2="$(PITCH=${TILT[$cell]} shoot $cell flat_off "$cam" "${from[@]}" WW_CELL_SKYPIC_FLAT=0.3,0.45,0.6 \
			WW_CELL_GI_DUMP="$(winpath "$run/dump_flat_off")")"
		check "$cell: both flat runs shot" "$([ "$f1$f2" = 11 ] && echo 1 || echo 0)"
		grep -h "gi skypic:" "$run/flat_on.notes" | cut -c1-300 | tee -a "$LOG"
		$CHK flat "$(winpath "$run")" dump_flat_on dump_flat_off > "$run/check_flat.txt" 2>&1
		sed "s/^/  /" "$run/check_flat.txt" | tee -a "$LOG"
		check "$cell: K: a sky of one colour, drawn and integrated, is SKY1's flat sky" "$(grep -q "^K PASS" "$run/check_flat.txt" && echo 1 || echo 0)"
		$CHK flat "$(winpath "$run")" dump_on dump_flat_off > "$run/check_flat_red.txt" 2>&1
		check "$cell: K's own red: the real sky against the flat one FAILS K" "$(grep -q "^K FAIL" "$run/check_flat_red.txt" && echo 1 || echo 0)"
		say "== $cell: reds"
		for r in flipz nocos; do
			PITCH=${TILT[$cell]} shoot $cell red_$r "$cam" "${from[@]}" WW_CELL_SKYPIC=1 WW_CELL_SKYPIC_RED=$r \
				WW_CELL_GI_DUMP="$(winpath "$run/dump_red_$r")" > /dev/null
			$CHK check "$(winpath "$run")" "dump_red_$r" > "$run/check_red_$r.txt" 2>&1
			sed "s/^/  red $r: /" "$run/check_red_$r.txt" | grep "Q \|S \|skypic" | tee -a "$LOG"
			check "$cell: red $r FAILS the rebuild" "$(grep -q "^skypic FAIL" "$run/check_red_$r.txt" && grep -q "^F PASS" "$run/check_red_$r.txt" && echo 1 || echo 0)"
		done
		if [ "$cell" = ext ]; then
			say "== $cell: the zenith (the capture against the view's own drawing)"
			z="${cam%%/*}"; z="${z%,*},40000"
			PITCH=89 shoot $cell zenith "$z/5/10" "${from[@]}" WW_CELL_SKYPIC=1 WW_LOOKDEV_CLOUDS=0 WW_EXPOSURE_EV=0 \
				WW_VIEW_TRANSFORM=Standard WW_CELL_GI_DUMP="$(winpath "$run/dump_zen")" > /dev/null
			$CHK zenith "$(winpath "$run/zenith.png")" "$(winpath "$run")" dump_zen > "$run/check_zen.txt" 2>&1
			sed "s/^/  /" "$run/check_zen.txt" | tee -a "$LOG"
			check "$cell: D: the capture's zenith is the drawn zenith" "$(grep -q "^D PASS" "$run/check_zen.txt" && echo 1 || echo 0)"
		fi
	fi
	if has pics; then
		say "== $cell: pictures (the full sky rows on)"
		for h in 12 $DUSK; do
			for arm in before after; do
				sw=(); [ $arm = after ] && sw=( WW_CELL_SKYPIC=1 )
				PITCH=${TILT[$cell]} shoot $cell pic_${h}_$arm "$cam" "${from[@]}" "${sw[@]}" WW_LOOKDEV_HOUR=$h \
					WW_LOOKDEV_SUN=1 WW_LOOKDEV_MOON=1 WW_LOOKDEV_SHADOWS=1 > /dev/null
			done
			pd="$(pixdiff "$run/pic_${h}_before.png" "$run/pic_${h}_after.png")"
			say "  hour $h: before against after ${pd:-unreadable} (pixels over 2 levels, largest level, mean level)"
			grep -h "gi skypic:" "$run/pic_${h}_after.notes" | cut -c1-400 | sed "s/^/  /" | tee -a "$LOG"
		done
	fi
done
say "SKYPIC1 GATE $([ $fails = 0 ] && echo PASS || echo "FAIL ($fails)")"
exit $fails
