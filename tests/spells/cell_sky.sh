#!/bin/bash
#
# THE SKY AND THE SUN IN THE BOUNCE ROW, OUTDOORS (lane SKY1, 2026-10-02; src/probesky.h).
#
# Not judged by eye. Per exterior cell (Scene mode Lookdev pinned: weather, hour, the plain sun arc):
#   bake     one window places + bakes the probes, relights the bake with the weather's sky and sun
#            (WW_CELL_GI_DUMP writes every stage's numbers) and shoots lit.png
#   views    per camera (eye height, WW_CELL_GI_FROM = no new bake): probes 2-4 the position and normal,
#            5 the bounce a surface takes / pi, 90 the share of the weather ambient the grid replaced;
#            r_lit / r_keepamb the finished picture with the ambient replaced / left in (rp_* in PBR mode)
#   interior one interior, the exe from before the lane (BEFORE=) against this one: the bounce probe, the lit
#            picture and the relight's dump must be the same byte for byte, in the plain light and in Lookdev
# Then tests/spells/cell_sky_check.py rebuilds the sky and the sun from the .tbk files, the soup and ITS OWN
# read of the plugin's weather and climate records (stages W U S T B C D O R; its header says what each is).
#
# RED CONTROLS (WW_CELL_SKY_RED; each must FAIL its stage, the numbers are printed):
#   --red off         S  no sky in the relight
#   --red novis       S  every direction sees sky (the stored visibility ignored)
#   --red notint      T  the glass tint ignored (on the cell with outdoor glass; SKIP where there is none)
#   --red sunthrough  U  the sun lights surfels through walls and roofs
#   --red keepamb     R  the weather's unshadowed ambient left in beside the grid's sky
#
# INTERIORS THAT SHOW THE SKY (lane SKYINT1, 2026-10-03; phase skyint). An interior whose CELL DATA has bit 7
# (Show Sky) bakes a ray that meets nothing as sky (the Division deck's rule) and its relight takes the weather's
# sky like an exterior's; the sun only with bits 8 + 11 (Use Sky Lighting + Sunlight Shadows; no vanilla cell).
# Per cell of SKYINT (Lookdev, one launch: bake + relight + dump), cell_sky_check.py --cell reads the flags
# ITSELF from the plugin: V the .tbk's sky shares (closed: every one 0; Show Sky: at least 20 probes see sky),
# N the viewer read the same flags, I (closed) no sky file in the dump, and for Show Sky cells W U S T B as
# outdoors (U: the sun kept out). tests/prtp_reference.py re-traces a Show Sky cell's sky shares with its own
# brute-force tracer (worst octant 0.01 of the sphere, the bar set on exteriors); build it once with
#   g++ -O3 -std=c++17 -static -pthread tests/prtp_reference.cpp -o release/prtp_reference.exe   (or REF=)
# Reds (WW_CELL_SKYINT_RED, a bake of their own; each must FAIL stage V):
#   --red noflag      V  the flag ignored: the Museum of Freedom bakes no sky
#   --red all         V  every interior read as Show Sky: Vault111Cryo's misses (mesh cracks) become sky;
#                        its sphere share is the false sky the deck's rule gives a closed cell
#
# USAGE  bash tests/spells/cell_sky.sh [--red off|novis|notint|sunthrough|keepamb|noflag|all]
#        PHASE="bake views skyint interior check" (default all five; a hold on the window may take them in turn)
#        RECHECK=1 judges the files already shot (no launch).
#        CELLS="tag:x,y ..."; VIEWS_<tag>="name=cx,cy,cz/view/dist ..." (look-at point, WW_RENDER_VIEW, distance)

set -u

. "$(dirname "$0")/_harness.sh"

REPO="$(cd "$(dirname "$0")/../.." && pwd)"
EXE="${EXE:-$REPO/release/NifSkope.exe}"
BEFORE="${BEFORE:-$REPO/release/NifSkope.before_sky1.exe}"
SCOPE="${SCOPE:-cell_sky}"
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
RED=""
[ "${1:-}" = "--red" ] && RED="${2:-}"
OUT="${OUT:-$REPO/scratchpad/sky1_20261002/gate}"
LOG="$OUT/cell_sky${RED:+_red_$RED}.log"
PORT="${PORT:-14797}"
SPEC="$REPO/tests/fixtures/empty.wwcell"
SIZE="${SIZE:-960x600}"
WORLD="${WORLD:-Commonwealth}"
WEATHER="${WEATHER:-CommonwealthClear}"
HOUR="${HOUR:-12}"
PHASE="${PHASE:-bake views skyint interior check}"
RECHECK="${RECHECK:-0}"
[ "$RECHECK" = 1 ] && PHASE="check"
# Graygarden: the cell with outdoor glass (its greenhouses), for the notint red; no view, the dump alone
# lane SKYINT1: an empty CELLS (or SKYINT) is none, not the default
CELLS="${CELLS-concord:-15,17 graygarden:-12,4}"
# lane SKYINT1: the Museum of Freedom (Show Sky + Use Sky Lighting), a Show Sky cell of another kind (Show Sky
# alone), a closed vault; its reds take one cell each and no exterior
SKYINT="${SKYINT-museum:ConcordMuseum01 witch:MuseumOfWitchcraft01 cryo:Vault111Cryo}"
REF="${REF:-$REPO/release/prtp_reference.exe}"
case "$RED" in
	noflag) CELLS=""; SKYINT="museum:ConcordMuseum01" ;;
	all) CELLS=""; SKYINT="cryo:Vault111Cryo" ;;
	?*) SKYINT="" ;;
esac
INTERIOR="${INTERIOR:-DmndSolomonsHouse01}"
INTERIOR_CAM="${INTERIOR_CAM:-1450,-20,150}"
# the cameras, eye height (Concord's street is near z 6200: a camera at z 300 is under the ground and shows
# only the background), picked from the bake's sky shares by scratch pick_cams.py, judged on the pictures
: "${VIEWS_concord:=open=-60200,73500,6300/4/600 covered=-57680,71680,6284/4/350}"

mkdir -p "$OUT"
: > "$LOG"
say() { echo "$@" | tee -a "$LOG"; }
fails=0
check() { if [ "$2" = "1" ]; then say "  PASS  $1"; else say "  FAIL  $1"; fails=$((fails+1)); fi; }
has() { case " $PHASE " in *" $1 "*) return 0;; esac; return 1; }
[ -x "$EXE" ] || { echo "no NifSkope.exe at $EXE"; exit 2; }
say "cell_sky.sh  $(date '+%Y-%m-%d %H:%M:%S')${RED:+   RED CONTROL: $RED}   phase: $PHASE"
if [ "$RECHECK" != 1 ]; then
	newer=1
	for s in src/probesky.cpp src/probegi.cpp src/gl/celllights.cpp src/cellview.cpp src/glview.cpp \
		res/shaders/cell_lights.glsl res/shaders/fo4_default.frag res/shaders/pbrm_default.frag; do
		[ "$REPO/$s" -nt "$EXE" ] && { say "  $s is NEWER than the exe"; newer=0; }
	done
	check "the exe is newer than every source this gate covers" "$newer"
	same=1
	for s in cell_lights.glsl fo4_default.frag pbrm_default.frag; do
		cmp -s "$REPO/res/shaders/$s" "$REPO/release/shaders/$s" || { say "  release/shaders/$s is not res/shaders/$s"; same=0; }
	done
	check "the shaders beside the exe are the source's" "$same"
fi

# the weather the view is lit by, pinned: Lookdev, the plain sun arc, no sky dome / fog / ground / tonemap
LD=( WW_LOOKDEV=1 WW_LOOKDEV_WEATHER="$WEATHER" WW_LOOKDEV_HOUR="$HOUR" WW_LOOKDEV_SUN=0 WW_LOOKDEV_GROUND=0
	WW_LOOKDEV_SKY=0 WW_LOOKDEV_CLOUDS=0 WW_LOOKDEV_MOON=0 WW_LOOKDEV_FOG=0 WW_LOOKDEV_SHADOWS=0 WW_LOOKDEV_PLUGINS="$ESM" )
# lane SKYFULL1: the preview rows ship ON; all four stay pinned OFF here so the probe numbers stay byte-stable

shoot() {   # shoot <exe> <WW_CELL_OPEN tail> <probes.tsv> <shot.png> <cx,cy,cz/view/dist or ''> <env...>
	local exe="$1" open="$2" tsv="$3" shot="$4" cam="$5"; shift 5
	local notes="${shot%.png}.notes" camenv=()
	mkdir -p "$(dirname "$shot")"
	rm -f "$shot" "$notes"
	if [ -n "$cam" ]; then
		local c="${cam%%/*}" rest="${cam#*/}"
		camenv=( WW_RENDER_CENTER="$c" WW_RENDER_VIEW="${rest%%/*}" WW_RENDER_DIST="${rest#*/}" WW_RENDER_FOV=70 )
	else
		camenv=( WW_RENDER_VIEW=1 )
	fi
	# lane BOUNCE2: one bounce pinned (this gate's gather twin and its before/after interior are one pass)
	env WW_CELL_IS=0 WW_CELL_GI_PASSES=1 "$@" "${camenv[@]}" \
		WW_CELL_OPEN="$ESM|$open" WW_CELL_DATAROOT="$DATA" \
		WW_CELL_PROBES="$(winpath "$tsv")" WW_CELL_PROBES_HIDE=1 WW_CELL_LIT=1 \
		WW_RENDER_SHOT="$(winpath "$shot")" WW_RENDER_SIZE="$SIZE" WW_RENDER_CLEAN=1 \
		WW_SETTINGS_SCOPE="$(fresh_scope)" timeout 1500 "$exe" --port "$PORT" "$(winpath "$SPEC")" > "$notes" 2>&1
	[ -s "$shot" ] && echo 1 || echo 0
}

for spec in $CELLS; do
	tag="${spec%%:*}"; xy="${spec##*:}"
	say "== $tag ($WORLD $xy, $WEATHER hour $HOUR)"
	run="$OUT/$tag"
	sub="$run${RED:+/red_$RED}"
	open="$WORLD|$xy|1"
	vvar="VIEWS_$tag"; views="${!vvar:-}"
	mkdir -p "$sub"
	from=( WW_CELL_GI=1 WW_CELL_GI_FROM="$(winpath "$run/bake")" )
	ok=1
	if [ -z "$RED" ]; then
		if has bake; then
			rm -rf "$run/bake" "$run/dump" "$run/views"
			[ "$(shoot "$EXE" "$open" "$run/probes.tsv" "$run/lit.png" "" "${LD[@]}" WW_CELL_GI=1 \
				WW_CELL_PROBE_SOUP="$(winpath "$run/soup.psp")" WW_CELL_PROBE_BAKE="$(winpath "$run/bake")" \
				WW_CELL_GI_DUMP="$(winpath "$run/dump")")" = 1 ] || ok=0
			grep -h "  gi" "$run/lit.notes" | head -3 | cut -c1-300 | tee -a "$LOG"
		fi
		if has views; then
			for v in $views; do
				name="${v%%=*}"; cam="${v#*=}"; d="$run/views/$name"
				for p in 2 3 4 5 90; do
					[ "$(shoot "$EXE" "$open" "$run/probes.tsv" "$d/probe$p.png" "$cam" "${LD[@]}" "${from[@]}" WW_CELL_LIT_PROBE=$p)" = 1 ] || ok=0
				done
				[ "$(shoot "$EXE" "$open" "$run/probes.tsv" "$d/r_lit.png" "$cam" "${LD[@]}" "${from[@]}")" = 1 ] || ok=0
				[ "$(shoot "$EXE" "$open" "$run/probes.tsv" "$d/r_keepamb.png" "$cam" "${LD[@]}" "${from[@]}" WW_CELL_SKY_RED=keepamb)" = 1 ] || ok=0
				# lane SKYFULL1: the sheet he sees = the same view under the whole sky (dome, sun, clouds, moon, stars);
				# not measured here (the probe numbers above keep the rows OFF), only checked to be the dome
				if [ -z "$RED" ]; then
					[ "$(shoot "$EXE" "$open" "$run/probes.tsv" "$d/sheet_lit.png" "$cam" "${LD[@]}" "${from[@]}" WW_LOOKDEV_SKY=1 WW_LOOKDEV_SUN=1 WW_LOOKDEV_CLOUDS=1 WW_LOOKDEV_MOON=1 WW_LODGEN_RESOURCES="$DATA")" = 1 ] 						&& grep -q "lookdev sky: sky:dome(" "$d/sheet_lit.notes" || { say "  $name: the sheet did not draw the dome"; ok=0; }
				fi
				if [ "$name" = covered ]; then
					pbr=( WW_PBRM_MODE=pbr WW_PBRM_AUTOREPLACE=1 )
					[ "$(shoot "$EXE" "$open" "$run/probes.tsv" "$d/rp_lit.png" "$cam" "${LD[@]}" "${from[@]}" "${pbr[@]}")" = 1 ] || ok=0
					[ "$(shoot "$EXE" "$open" "$run/probes.tsv" "$d/rp_keepamb.png" "$cam" "${LD[@]}" "${from[@]}" "${pbr[@]}" WW_CELL_SKY_RED=keepamb)" = 1 ] || ok=0
				fi
			done
		fi
		{ has bake || has views; } && check "$tag: the pictures written" "$ok"
	elif [ "$RECHECK" != 1 ]; then
		# a red relights the green run's bake with the defect on: its own dump, and the covered view's pictures
		rm -rf "$sub"; mkdir -p "$sub"
		redenv=( WW_CELL_SKY_RED="$RED" )
		first=( WW_CELL_GI_DUMP="$(winpath "$sub/dump")" )
		shots=0
		for v in $views; do
			name="${v%%=*}"; cam="${v#*=}"; d="$sub/views/$name"
			[ "$name" = covered ] || [ "$RED" = off ] || continue
			if [ "$RED" = keepamb ]; then
				[ "$(shoot "$EXE" "$open" "$run/probes.tsv" "$d/r_lit.png" "$cam" "${LD[@]}" "${from[@]}" "${redenv[@]}" "${first[@]}")" = 1 ] || ok=0
				first=()
				[ "$(shoot "$EXE" "$open" "$run/probes.tsv" "$d/rp_lit.png" "$cam" "${LD[@]}" "${from[@]}" "${redenv[@]}" WW_PBRM_MODE=pbr WW_PBRM_AUTOREPLACE=1)" = 1 ] || ok=0
			fi
			for p in 5 90; do
				[ "$(shoot "$EXE" "$open" "$run/probes.tsv" "$d/probe$p.png" "$cam" "${LD[@]}" "${from[@]}" "${redenv[@]}" "${first[@]}" WW_CELL_LIT_PROBE=$p)" = 1 ] || ok=0
				first=()
			done
			shots=1
		done
		if [ "$shots" = 0 ]; then   # a cell with no view: the dump alone
			[ "$(shoot "$EXE" "$open" "$run/probes.tsv" "$sub/probe5.png" "" "${LD[@]}" "${from[@]}" "${redenv[@]}" "${first[@]}" WW_CELL_LIT_PROBE=5)" = 1 ] || ok=0
		fi
		check "$tag: the red's pictures written" "$ok"
	fi
	if has check; then
		vargs=()
		for v in $views; do c="${v#*=}"; vargs+=( --view "${v%%=*}=${c%%/*}" ); done
		python "$(dirname "$0")/cell_sky_check.py" "$ESM" "$run" --weather "$WEATHER" --hour "$HOUR" \
			${RED:+--red "$RED"} "${vargs[@]}" > "$sub/check.txt" 2>&1
		sed 's/^/  /' "$sub/check.txt" | tee -a "$LOG"
		want=""
		case "$RED" in off|novis) want=S ;; notint) want=T ;; sunthrough) want=U ;; keepamb) want=R ;; esac
		if [ "$want" = T ] && grep -q "^T SKIP" "$sub/check.txt"; then
			say "  skip  $tag: no outdoor glass in this cell, the notint red has nothing to remove"
		elif [ "$want" = R ] && ! grep -q "^R " "$sub/check.txt"; then
			say "  skip  $tag: no view, the keepamb red has no picture"
		elif [ -n "$want" ]; then
			check "$tag: the red control FAILS stage $want" "$(grep -q "^$want[A-Z]* .*FAIL" "$sub/check.txt" && echo 1 || echo 0)"
		else
			check "$tag: every stage matches the independent rebuild" "$(grep -q "^sky PASS" "$sub/check.txt" && echo 1 || echo 0)"
		fi
	fi
done

# ---- lane SKYINT1: interiors judged by their own Show Sky flag (a red bakes its own cell with the defect on)
for spec in $SKYINT; do
	tag="${spec%%:*}"; edid="${spec##*:}"
	run="$OUT/skyint${RED:+_red_$RED}/$tag"
	say "== interior $edid ($tag${RED:+, RED $RED}; $WEATHER hour $HOUR)"
	if has skyint; then
		rm -rf "$run"; mkdir -p "$run"
		[ "$(shoot "$EXE" "interior|$edid" "$run/probes.tsv" "$run/lit.png" "" "${LD[@]}" WW_CELL_GI=1 \
			${RED:+WW_CELL_SKYINT_RED=$RED} \
			WW_CELL_PROBE_SOUP="$(winpath "$run/soup.psp")" WW_CELL_PROBE_BAKE="$(winpath "$run/bake")" \
			WW_CELL_GI_DUMP="$(winpath "$run/dump")")" = 1 ] && ok=1 || ok=0
		grep -h "cell flags\|  bake: [0-9]\|  gi sky\|  gi:" "$run/lit.notes" | head -4 | cut -c1-260 | tee -a "$LOG"
		check "$edid: the picture and the bake written" "$ok"
	fi
	if has check && [ -d "$run/bake" ]; then
		python "$(dirname "$0")/cell_sky_check.py" "$ESM" "$run" --cell "$edid" --weather "$WEATHER" --hour "$HOUR" \
			> "$run/check.txt" 2>&1
		sed 's/^/  /' "$run/check.txt" | tee -a "$LOG"
		if [ -n "$RED" ]; then
			check "$edid: the red control FAILS stage V" "$(grep -q "^V FAIL" "$run/check.txt" && echo 1 || echo 0)"
		else
			check "$edid: every stage matches the plugin's flags and the independent rebuild" \
				"$(grep -q "^sky $edid PASS" "$run/check.txt" && echo 1 || echo 0)"
			if grep -q "^V PASS sky in a Show Sky" "$run/check.txt"; then
				if [ -x "$REF" ]; then
					python "$(dirname "$0")/prtp_reference.py" "$(winpath "$run/soup.psp")" "$(winpath "$run/bake")" \
						--ref "$(winpath "$REF")" > "$run/reference.txt" 2>&1
					sed 's/^/  /' "$run/reference.txt" | tee -a "$LOG"
					check "$edid: the sky shares re-traced by the brute-force reference" \
						"$(grep -q "^reference PASS" "$run/reference.txt" && echo 1 || echo 0)"
				else
					say "  FAIL  no reference tracer at $REF (build line in this header)"; fails=$((fails+1))
				fi
			fi
		fi
	fi
done

# ---- an interior: the lane must not move it by one pixel
if [ -z "$RED" ] && { has interior || has check; }; then
	say "== interior $INTERIOR: before the lane ($(basename "$BEFORE")) against after"
	for mode in plain lookdev; do
		idir="$OUT/interior_$mode"
		menv=(); [ "$mode" = lookdev ] && menv=( "${LD[@]}" )
		if has interior; then
			[ -x "$BEFORE" ] || { say "  no exe from before the lane at $BEFORE"; fails=$((fails+1)); continue; }
			rm -rf "$idir"; mkdir -p "$idir/before" "$idir/after"
			iopen="interior|$INTERIOR"; icam="$INTERIOR_CAM/1/1400"; ok=1
			ifrom=( WW_CELL_GI=1 WW_CELL_GI_FROM="$(winpath "$idir/bake")" )
			# lane ROOMCLAMP1: this exe relights with its rooms clamp off (a gate key, never a user toggle), so the
			# pre-lane relight of the same bake stays byte for byte
			pin=( WW_CELL_ROOMCLAMP_PIN=off )
			[ "$(shoot "$BEFORE" "$iopen" "$idir/probes.tsv" "$idir/before/lit.png" "$icam" "${menv[@]}" WW_CELL_GI=1 \
				WW_CELL_PROBE_SOUP="$(winpath "$idir/soup.psp")" WW_CELL_PROBE_BAKE="$(winpath "$idir/bake")" \
				WW_CELL_GI_DUMP="$(winpath "$idir/before/dump")")" = 1 ] || ok=0
			[ "$(shoot "$BEFORE" "$iopen" "$idir/probes.tsv" "$idir/before/probe5.png" "$icam" "${menv[@]}" "${ifrom[@]}" WW_CELL_LIT_PROBE=5)" = 1 ] || ok=0
			[ "$(shoot "$BEFORE" "$iopen" "$idir/probes.tsv" "$idir/before/lit2.png" "$icam" "${menv[@]}" "${ifrom[@]}" \
				WW_CELL_GI_DUMP="$(winpath "$idir/before/dump")")" = 1 ] || ok=0
			[ "$(shoot "$EXE" "$iopen" "$idir/probes.tsv" "$idir/after/lit.png" "$icam" "${menv[@]}" "${ifrom[@]}" "${pin[@]}" \
				WW_CELL_GI_DUMP="$(winpath "$idir/after/dump")")" = 1 ] || ok=0
			[ "$(shoot "$EXE" "$iopen" "$idir/probes.tsv" "$idir/after/probe5.png" "$icam" "${menv[@]}" "${ifrom[@]}" "${pin[@]}" WW_CELL_LIT_PROBE=5)" = 1 ] || ok=0
			cp -f "$idir/before/lit2.png" "$idir/before/lit.png" 2>/dev/null   # both sides relit from the files
			check "$INTERIOR ($mode): the pictures written" "$ok"
		fi
		if has check && [ -d "$idir/after" ]; then
			python "$(dirname "$0")/cell_sky_check.py" --interior "$idir" "$ESM" "$INTERIOR" > "$idir/check.txt" 2>&1
			sed 's/^/  /' "$idir/check.txt" | tee -a "$LOG"
			check "$INTERIOR ($mode): byte for byte what the exe before the lane gave" "$(grep -q "^sky interior PASS" "$idir/check.txt" && echo 1 || echo 0)"
		fi
	done
fi
say ""
if [ "$fails" = "0" ]; then say "PASS"; else say "FAIL ($fails)"; fi
exit "$fails"
