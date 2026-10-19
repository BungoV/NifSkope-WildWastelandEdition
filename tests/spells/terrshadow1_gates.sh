#!/usr/bin/env bash
# terrshadow1_gates.sh -- the gates of lane TERRSHADOW1, the unified sun-shadow set (src/gl/shadowset.h):
# (A) Cascade Quality, (B) Contact Shadows, (C) Terrain Shadows, (D) Sun Path, one composition.
# Judged by terrshadow1_gates.py (pictures + echoes) and terrshadow1_march.py (an independent CPU
# ray march of the dumped heights) -- never by the code under test.
#
# The known answer: the vanilla QryCube01.nif (a 512-unit cube on the origin, read from the unpacked
# Data folder, never copied), on the Lookdev ground at z = -256. WW_SHADOW_TERRAIN_ALL=1 makes the cube
# the terrain the sweep sees; WW_SUNPATH_PIN=<elev>,<az> pins the sun (az 90 = east: the shadow runs -x).
#
#   off      every new row Off = release/NifSkope.before_terrshadow1.exe byte for byte (cube, view 8,
#            Shadows off and on; top-down with Shadows on; the Real sun path's Engine twin)
#   march    the swept map vs an independent CPU march of the same heights (30, 10, 3, 1 degrees)
#   length   the shadow's length on the ground vs 512 / tan(elev): the map at 30/10/3/1 degrees, and the
#            terrain picture (ortho top-down, view 2) at the same elevations
#   handoff  the composed picture (view 4) and the weights (view 5) across the cascades' reach, perspective
#            top-down at a low sun: the luminance step at the cascade -> terrain handoff
#   contact  the contact mask (view 3) under the cube in the cell-less view: refused (the cell view only)
#   cascade  Cascade Quality Low..Ultra: the echoed fit (D, map, splits) and the composed picture
#   sun      Sun Path: the engine arc vs the real path over the day (echo only)
#   perf     GPU ms per producer per preset at 1080p in the cell view (gpums + the echoed timers)
#   cell     the combined low-sun picture in the cell view (west of Concord, sun 10 degrees ESE): every view of the ladder;
#            composed == min of the producers, the step across the handoff, contact vs cascades; Off = before
#
# usage: bash tests/spells/terrshadow1_gates.sh [--out DIR] [--only off,march,...] [--red NAME]
#   reds (WW_SHADOW_RED): terrnodecay terrflip terrseam mulcompose contactflip novogel; (WW_CSM_RED) solidcast
# Every red must end FAIL. One NifSkope of ours at a time (our --port), second monitor; run it under
# withlock.sh nifskope.

set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
. "$HERE/_harness.sh"

OUT="$ROOT/scratchpad/terrshadow1_gates_$(date +%Y%m%d_%H%M%S)"
ONLY=""
RED=""
while [ $# -gt 0 ]; do
	case "$1" in
		--out) OUT="$2"; shift 2 ;;
		--only) ONLY=",$2,"; shift 2 ;;
		--red) RED="$2"; shift 2 ;;
		*) echo "unknown argument $1"; exit 2 ;;
	esac
done
DATA="/e/Tools/Fallout 4/DataUnpacked/Data"
GAME="${TERRSHADOW1_GAME:-/x/Programs/Steam/steamapps/common/Fallout 4/Data}"
CUBE="$DATA/Meshes/Architecture/Quarry/QryCube01.nif"
ARM="$ROOT/release"
OLD="${TERRSHADOW1_OLD:-$ROOT/release/NifSkope.before_terrshadow1.exe}"
PORT="${TERRSHADOW1_PORT:-43391}"
SCOPE=terrshadow1gates
REGKEY="HKCU\\Software\\NifTools\\NifSkope 2.0 $SCOPE"
mkdir -p "$OUT"
OUT="$(cd "$OUT" && pwd)"
case "$OUT" in /[a-z]/*) ;; *) echo "REFUSED: --out must resolve to a drive path ($OUT)"; exit 2 ;; esac
[ "$(head -c 2 "$ARM/NifSkope.exe")" = "MZ" ] || { echo "REFUSED: release/NifSkope.exe is not a finished image"; exit 2; }
[ -f "$OLD" ] || { echo "REFUSED: no rung $OLD"; exit 2; }
[ -f "$GAME/Fallout4.esm" ] || { echo "REFUSED: no Fallout4.esm in $GAME"; exit 2; }
[ -f "$CUBE" ] || { echo "REFUSED: no known-answer caster $CUBE"; exit 2; }
if tasklist //FI "IMAGENAME eq Fallout4.exe" 2>/dev/null | grep -qi fallout4; then echo "REFUSED: Fallout4.exe is running"; exit 2; fi

REDPIN=""
case "$RED" in
	'') ;;
	terrnodecay|terrflip|terrseam|mulcompose|contactflip|novogel) REDPIN="WW_SHADOW_RED=$RED" ;;
	solidcast) REDPIN="WW_CSM_RED=solidcast" ;;	# leaf cards cast solid cards again: the cell judge must FAIL
	*) echo "unknown red $RED"; exit 2 ;;
esac
{ echo "red=${RED:-none}"; sha1sum "$ARM/NifSkope.exe" "$OLD"; echo "game=$GAME"; } > "$OUT/arms.txt"

harness_alive() {  # OUR harness only: the one on our port
	powershell -NoProfile -Command "Get-CimInstance Win32_Process -Filter \"Name='NifSkope.exe'\" | Where-Object { \$_.CommandLine -match '--port $PORT' } | ForEach-Object { \$_.ProcessId }" | tr -d '\r'
}

common_env() {
	echo WW_SETTINGS_SCOPE=$SCOPE WW_WINDOW_AT=1960,40 WW_RENDER_SIZE=1600x1000 \
		WW_RENDER_TIME=1.0 WW_RENDER_CLEAN=1 WW_RENDER_FLAT=0 WW_RENDER_REFRACTION=1 WW_RENDER_SS=0 \
		WW_PBRM_MODE=pbr WW_PBRM_AUTOREPLACE=1 WW_RENDER_PARTICLES=1 \
		WW_RENDER_SHADOWS=0 WW_RENDER_CONTACT=0 WW_RENDER_AO=0 WW_RENDER_SSGI=0 \
		WW_CELL_DOF=0 WW_CELL_MBLUR=0
}
LOOK="WW_LIGHTING_MODE=lookdev WW_LOOKDEV=1 WW_VIEW_TRANSFORM=standard WW_EXPOSURE_EV=0 WW_LOOKDEV_SKY=0 WW_LOOKDEV_SUN=0 WW_LOOKDEV_CLOUDS=0 WW_LOOKDEV_MOON=0"

shot() {  # shot <exe> <tag> <nif> [KEY=VALUE ...] -- one picture + echoes
	local exe="$1" tag="$2" nif="$3"; shift 3
	local png="$OUT/$tag.png"
	rm -f "$png" "$OUT/$tag.csm.txt" "$OUT/$tag.uss.txt"
	local alive; alive="$(harness_alive)"
	if [ -n "$alive" ]; then echo "  REFUSED $tag: our harness NifSkope still running (pid $alive)"; return 1; fi
	reg delete "$REGKEY" //f > /dev/null 2>&1
	# shellcheck disable=SC2046
	env $(common_env) $LOOK \
		WW_CSM_ECHO="$(winpath "$OUT/$tag.csm.txt")" \
		WW_SHADOW_ECHO="$(winpath "$OUT/$tag.uss.txt")" \
		WW_LODGEN_RESOURCES="$(winpath "$DATA")" \
		WW_LOOKDEV_DATA="$(winpath "$GAME")" \
		WW_RENDER_SHOT="$(winpath "$png")" \
		"$@" \
		timeout 240 "$exe" --port "$PORT" "$(winpath "$nif")" > "$OUT/$tag.log" 2>&1
	local rc=$?
	if [ -s "$png" ]; then echo "  $tag rc=$rc $(stat -c %s "$png") B"; else echo "  $tag rc=$rc NO PICTURE"; fi
	reg delete "$REGKEY" //f > /dev/null 2>&1
}

CELLCAM="${TERRSHADOW1_CELLCAM:--61400 68000 6074 -71400 67000 5900 70}"
cellshot() {  # cellshot <exe> <tag> [KEY=VALUE ...] -- the cell view through the camera path; frame 001 is the picture
	local exe="$1" tag="$2"; shift 2
	local png="$OUT/$tag.png" try
	rm -f "$png" "$OUT/$tag.csm.txt" "$OUT/$tag.uss.txt" "$OUT/${tag}"_[0-9][0-9][0-9].png
	for try in 1 2; do
		reg delete "$REGKEY" //f > /dev/null 2>&1
		reg add "$REGKEY\Settings" //v Version //t REG_SZ //d 1 //f > /dev/null 2>&1
		reg add "$REGKEY" //v "Game Manager Version" //t REG_DWORD //d 2 //f > /dev/null 2>&1
		python "$HERE/settings_scope_game.py" "$SCOPE" "$(winpath "$OUT/scope_game.reg")" > /dev/null 2>&1 \
			&& reg import "$(winpath "$OUT/scope_game.reg")" > /dev/null 2>&1
		# shellcheck disable=SC2046
		env $(common_env) WW_LOOKDEV=1 WW_LOOKDEV_CLOUDS=0 WW_LOOKDEV_MOON=0 WW_LOOKDEV_FOG=0 \
			WW_CSM_ECHO="$(winpath "$OUT/$tag.csm.txt")" WW_SHADOW_ECHO="$(winpath "$OUT/$tag.uss.txt")" \
			WW_RENDER_SHOT="$(winpath "$png")" "$@" \
			timeout 900 "$exe" --port "$PORT" "$(winpath "$ROOT/tests/fixtures/empty.wwcell")" > "$OUT/$tag.log" 2>&1
		[ -s "$OUT/${tag}_001.png" ] && mv -f "$OUT/${tag}_001.png" "$png"
		rm -f "$OUT/${tag}"_[0-9][0-9][0-9].png
		[ -s "$png" ] && break
	done
	if [ -s "$png" ]; then echo "  $tag $(stat -c %s "$png") B"; else echo "  $tag NO PICTURE"; fi
	reg delete "$REGKEY" //f > /dev/null 2>&1
}

want() { [ -z "$ONLY" ] || [[ "$ONLY" == *",$1,"* ]]; }
aimed() { local r; [ -z "$RED" ] && return 0; for r in "$@"; do [ "$RED" = "$r" ] && return 0; done; return 1; }
ON="WW_LOOKDEV_SHADOWS=1"
GRD="WW_LOOKDEV_GROUND=1"
V8="WW_RENDER_VIEW=8 $GRD"
TERR="WW_SHADOW_TERRAIN_ALL=1 WW_SHADOW_TERRAIN_SYNC=1 WW_SUNPATH=1"
ELEVS="30 10 3 1"
[ -n "$RED" ] && ELEVS="3"   # a red needs one elevation to fail

# shellcheck disable=SC2086
if want off && [ -z "$RED" ]; then
	echo "off"
	for f in v8 v8on topon; do
		case $f in
			v8) FR="$V8 WW_LOOKDEV_HOUR=12" ;;
			v8on) FR="$V8 WW_LOOKDEV_HOUR=16 $ON" ;;
			topon) FR="WW_RENDER_VIEW=1 WW_RENDER_FOV=30 WW_RENDER_DIST=2000 WW_RENDER_CENTER=-356,-100,-256 $GRD WW_LOOKDEV_HOUR=12 $ON" ;;
		esac
		shot "$OLD" off_${f}_old "$CUBE" $FR
		shot "$ARM/NifSkope.exe" off_${f}_new "$CUBE" $FR
		shot "$ARM/NifSkope.exe" off_${f}_pinned "$CUBE" $FR WW_SHADOW_CASCADE=0 WW_SHADOW_CONTACT=0 WW_SHADOW_TERRAIN=0 WW_SUNPATH=0
	done
fi
# shellcheck disable=SC2086
if want march && aimed terrnodecay terrflip; then
	echo "march"
	for e in $ELEVS; do
		shot "$ARM/NifSkope.exe" march_$e "$CUBE" WW_RENDER_VIEW=1 WW_RENDER_ORTHO=4096 WW_RENDER_CENTER=0,0,-256 $GRD \
			WW_LOOKDEV_GROUND_HALF=40000 $TERR WW_SHADOW_TERRAIN=1 WW_SUNPATH_PIN=$e,90 \
			WW_SHADOW_DUMP="$(winpath "$OUT/march_$e.dump")" $REDPIN
	done
	# off-axis: the sweep interpolates the previous line here (az 120 and 200)
	for az in 120 200; do
		shot "$ARM/NifSkope.exe" march_3_az$az "$CUBE" WW_RENDER_VIEW=1 WW_RENDER_ORTHO=4096 WW_RENDER_CENTER=0,0,-256 $GRD \
			WW_LOOKDEV_GROUND_HALF=40000 $TERR WW_SHADOW_TERRAIN=1 WW_SUNPATH_PIN=3,$az \
			WW_SHADOW_DUMP="$(winpath "$OUT/march_3_az$az.dump")" $REDPIN
	done
fi
# shellcheck disable=SC2086
if want length && aimed terrnodecay terrflip; then
	echo "length"
	# ortho top-down over the shadow's run (-x), wide enough for the 1-degree shadow (29.3 k): the terrain view
	for e in $ELEVS; do
		case $e in 30) H=1200 ;; 10) H=3200 ;; 3) H=10500 ;; 1) H=31000 ;; esac
		shot "$ARM/NifSkope.exe" length_$e "$CUBE" WW_RENDER_VIEW=1 WW_RENDER_ORTHO=$H WW_RENDER_CENTER=-$((H - 600)),0,-256 $GRD \
			WW_LOOKDEV_GROUND_HALF=40000 $TERR WW_SHADOW_TERRAIN=4 WW_SUNPATH_PIN=$e,90 WW_SHADOW_VIEW=2 $REDPIN
	done
fi
# shellcheck disable=SC2086
if want handoff && aimed terrseam mulcompose; then
	echo "handoff"
	# perspective, looking down the shadow's run from above the cube: the cascades' reach (Low: D 4000) falls
	# inside the shadow; the composed factor (view 4) must not step where the cascades hand off to the terrain
	for e in $ELEVS; do
		for v in 1 2 4 5; do
			shot "$ARM/NifSkope.exe" handoff_${e}_v$v "$CUBE" WW_RENDER_VIEW=1 WW_RENDER_FOV=90 WW_RENDER_DIST=3000 \
				WW_RENDER_CENTER=-4000,0,-256 $GRD WW_LOOKDEV_GROUND_HALF=40000 $ON $TERR WW_SHADOW_TERRAIN=4 \
				WW_SHADOW_CASCADE=1 WW_SUNPATH_PIN=$e,90 WW_SHADOW_VIEW=$v $REDPIN
		done
	done
fi
# shellcheck disable=SC2086
if want cascade && aimed novogel; then
	echo "cascade"
	for t in 0 1 2 3 4; do
		shot "$ARM/NifSkope.exe" cascade_$t "$CUBE" $V8 WW_LOOKDEV_HOUR=16 $ON WW_SHADOW_CASCADE=$t WW_SUNPATH=1 WW_SUNPATH_PIN=10,250 \
			WW_SHADOW_VIEW=1 $REDPIN
	done
fi
# shellcheck disable=SC2086
if want sun && [ -z "$RED" ]; then
	echo "sun"
	for h in 5 6 7 8 12 16 17 18 19; do
		shot "$ARM/NifSkope.exe" sun_$h "$CUBE" $V8 WW_LOOKDEV_HOUR=$h WW_LOOKDEV_SUN=1 WW_LOOKDEV_SKY=1 WW_LOOKDEV_DAY=0 $ON WW_SUNPATH=0
	done
fi
# the cell view (west of Concord): a player-eye camera, in-bounds per notes/neongi1/tools/camcheck.py, in the woods looking west with
# the sun 10 degrees up in the ESE behind it, so the leaf cards' cut-outs dapple the ground; sky, sun disc and far LOD on (the far
# sway's clock pinned, a still picture); DOF and motion blur off
cell_setup() {
	ESM="$(winpath "$GAME/Fallout4.esm")"
	printf 'fps 16\nframes 2\nkey 0.000000 %s\nkey 0.062500 %s\n' "$CELLCAM" "$CELLCAM" > "$OUT/cell.path"
	CELL=( "WW_CELL_OPEN=$ESM|Commonwealth|-15,17|1" "WW_CELL_DATAROOT=$(winpath "$DATA")" WW_CELL_LIT=1 WW_CELL_FARLOD=1
		"WW_RENDER_PATH=$(winpath "$OUT/cell.path")" WW_RENDER_SIZE=1600x1000 WW_TAA=0 WW_CELL_IS=0 WW_CELL_PARTICLES=0
		WW_LOOKDEV_WEATHER=CommonwealthClear "WW_LOOKDEV_PLUGINS=$ESM" WW_LOOKDEV_SKY=1 WW_LOOKDEV_SUN=1 WW_LOOKDEV_GROUND=0
		$ON WW_LOOKDEV_HOUR=17 WW_CELL_GRASS_WIND_T=1 WW_CELL_FARLOD_CARDSWAY=0 WW_CELL_FARLOD_SWAYTIME=1 )
	LOW=( WW_SUNPATH=1 "WW_SUNPATH_PIN=${TERRSHADOW1_LOWSUN:-10,120}" )
}

# shellcheck disable=SC2086
if [[ "$ONLY" == *",cellprobe,"* ]]; then
	# a look only (never judged): the composed view at each pinned sun in TERRSHADOW1_PROBE ("elev,az elev,az ...")
	echo "cellprobe"
	cell_setup
	for s in ${TERRSHADOW1_PROBE:-6,240}; do
		cellshot "$ARM/NifSkope.exe" probe_${s/,/_} "${CELL[@]}" WW_SUNPATH=1 WW_SUNPATH_PIN=$s WW_SHADOW_CASCADE=4 WW_SHADOW_CONTACT=4 WW_SHADOW_TERRAIN=4 WW_SHADOW_TERRAIN_SYNC=1 WW_SHADOW_VIEW=${TERRSHADOW1_PROBE_VIEW:-4} "WW_SHADOW_DUMP=$(winpath "$OUT/probe_${s/,/_}.dump")" ${TERRSHADOW1_PROBE_EXTRA:-}
	done
fi

# shellcheck disable=SC2086
if want cell && aimed contactflip terrseam mulcompose; then
	# the combined low-sun picture: the sun pinned 6 degrees up in the east (a morning sun; shadows run across
	# the view; picked from the dumped heights: in the west-south-west the border mountains' horizon is 7.4 degrees, all shadow); contact near, cascades mid,
	# terrain far
	echo "cell"
	cell_setup
	ALL=( WW_SHADOW_CASCADE=4 WW_SHADOW_CONTACT=4 WW_SHADOW_TERRAIN=4 WW_SHADOW_TERRAIN_SYNC=1 )
	if [ -z "$RED" ]; then
		cellshot "$OLD" cell_off_old "${CELL[@]}"
		cellshot "$OLD" cell_off_old2 "${CELL[@]}"	# the noise floor: the cell view is not deterministic run to run
		cellshot "$ARM/NifSkope.exe" cell_off_new "${CELL[@]}"
		cellshot "$ARM/NifSkope.exe" cell_on "${CELL[@]}" "${LOW[@]}" "${ALL[@]}"
	fi
	for v in 1 2 3 4 5; do
		cellshot "$ARM/NifSkope.exe" cell_v$v "${CELL[@]}" "${LOW[@]}" "${ALL[@]}" WW_SHADOW_VIEW=$v $REDPIN
	done
fi

# shellcheck disable=SC2086
if want perf && [ -z "$RED" ]; then
	# GPU ms at 1080p in the cell view, one producer at a time per preset (60 repaints; the producers' own
	# timers in the echo). The terrain runs as in use (no SYNC): its worker thread rasters + sweeps, the
	# render thread uploads a band a frame; judged on the worst frame's CPU, the upload's GPU ms and one sweep.
	echo "perf"
	cell_setup
	PF=( WW_RENDER_SIZE=1920x1080 WW_RENDER_GPUMS=60 )
	cellshot "$ARM/NifSkope.exe" perf_base "${CELL[@]}" "${LOW[@]}" "${PF[@]}"
	for t in 1 2 3 4; do
		for p in CASCADE CONTACT TERRAIN; do
			# the terrain: 900 timed repaints, so the worker's map (Ultra: ~0.8 s) finishes and all of its upload
			# bands land in the frame max
			LONG=""; [ $p = TERRAIN ] && LONG=WW_RENDER_GPUMS=900
			cellshot "$ARM/NifSkope.exe" perf_${p,,}_$t "${CELL[@]}" "${LOW[@]}" "${PF[@]}" $LONG WW_SHADOW_$p=$t
		done
	done
fi

# shellcheck disable=SC2086
python "$HERE/terrshadow1_gates.py" --out "$(winpath "$OUT")" --red "${RED:-none}"
