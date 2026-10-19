#!/usr/bin/env bash
# fargrid1_gates.sh -- the gates of lane FARGRID1, the far air grid (src/gl/cellvolair.cpp, WW_CELL_AIR_FAR / "Far Air").
# Judged by fargrid1_gates.py (pictures + the "cell air:" echo), never by the code under test.
#
#   off     Far Air Off (unset, and pinned 0) == the rung release/NifSkope.before_fargrid1.exe byte for byte
#           (the lookdev fog cube, CommonwealthFoggy, the air on)
#   leak    the cell view (Concord, sun pinned low behind the western ridge), the air's L alone (WW_CELL_AIR_PROBE=1):
#           terrain shadows On vs Off; the far band's L must drop where the ridge shades it. red farnoterr must FAIL
#   seam    sun + ambient only (terms 9), no cascades, no terrain: Far Air High vs Off; the same medium, so L agrees
#           (mean + p95 of the per-pixel step). red farseam (a gap of air at the handoff) must FAIL
#   stable  a still camera, frames 1 and 2 of one launch: zero difference. red farjitter must FAIL
#   perf    1920x1080, 60 timed repaints, Far Air 0..4: the echoed gpums + volmb per preset (budget table)
#
# usage: bash tests/spells/fargrid1_gates.sh [--out DIR] [--only off,leak,seam,stable,perf] [--red farnoterr|farseam|farjitter]
# One NifSkope of ours at a time (our --port); run it under withlock.sh nifskope.

set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
. "$HERE/_harness.sh"

OUT="$ROOT/scratchpad/fargrid1_gates_$(date +%Y%m%d_%H%M%S)"
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
case "$RED" in ''|farnoterr|farseam|farjitter) ;; *) echo "unknown red $RED"; exit 2 ;; esac
DATA="/e/Tools/Fallout 4/DataUnpacked/Data"
GAME="${FARGRID1_GAME:-/x/Programs/Steam/steamapps/common/Fallout 4/Data}"
CUBE="$DATA/Meshes/Architecture/Quarry/QryCube01.nif"
ARM="$ROOT/release"
OLD="${FARGRID1_OLD:-$ROOT/release/NifSkope.before_fargrid1.exe}"
PORT="${FARGRID1_PORT:-43397}"
SCOPE=fargrid1gates
REGKEY="HKCU\\Software\\NifTools\\NifSkope 2.0 $SCOPE"
mkdir -p "$OUT"
OUT="$(cd "$OUT" && pwd)"
case "$OUT" in /[a-z]/*) ;; *) echo "REFUSED: --out must resolve to a drive path ($OUT)"; exit 2 ;; esac
[ "$(head -c 2 "$ARM/NifSkope.exe")" = "MZ" ] || { echo "REFUSED: release/NifSkope.exe is not a finished image"; exit 2; }
[ -f "$OLD" ] || { echo "REFUSED: no rung $OLD"; exit 2; }
[ -f "$GAME/Fallout4.esm" ] || { echo "REFUSED: no Fallout4.esm in $GAME"; exit 2; }
if tasklist //FI "IMAGENAME eq Fallout4.exe" 2>/dev/null | grep -qi fallout4; then echo "REFUSED: Fallout4.exe is running"; exit 2; fi
{ echo "red=${RED:-none}"; sha1sum "$ARM/NifSkope.exe" "$OLD"; echo "game=$GAME"; } > "$OUT/arms.txt"
ESM="$(winpath "$GAME/Fallout4.esm")"

harness_alive() {  # OUR harness only: the one on our port
	powershell -NoProfile -Command "Get-CimInstance Win32_Process -Filter \"Name='NifSkope.exe'\" | Where-Object { \$_.CommandLine -match '--port $PORT' } | ForEach-Object { \$_.ProcessId }" | tr -d '\r'
}
common_env() {
	echo WW_SETTINGS_SCOPE=$SCOPE WW_WINDOW_AT=1960,40 WW_RENDER_SIZE=1600x1000 \
		WW_RENDER_TIME=1.0 WW_RENDER_CLEAN=1 WW_RENDER_FLAT=0 WW_RENDER_SS=0 \
		WW_PBRM_MODE=pbr WW_PBRM_AUTOREPLACE=1 WW_RENDER_PARTICLES=0 \
		WW_RENDER_SHADOWS=0 WW_RENDER_CONTACT=0 WW_RENDER_AO=0 WW_RENDER_SSGI=0 WW_CELL_DOF=0 WW_CELL_MBLUR=0
}
REDPIN=(); [ -n "$RED" ] && REDPIN=( WW_CELL_AIR_RED=$RED )

shot() {  # shot <exe> <tag> [KEY=VALUE ...] -- the lookdev fog cube
	local exe="$1" tag="$2"; shift 2
	local png="$OUT/$tag.png"
	rm -f "$png"
	[ -n "$(harness_alive)" ] && { echo "  REFUSED $tag: our harness NifSkope still running"; return 1; }
	reg delete "$REGKEY" //f > /dev/null 2>&1
	# shellcheck disable=SC2046
	env $(common_env) WW_LIGHTING_MODE=lookdev WW_LOOKDEV=1 WW_LOOKDEV_WEATHER=CommonwealthFoggy WW_LOOKDEV_FOG=1 \
		WW_LOOKDEV_HOUR=8 WW_LOOKDEV_DAY=4 WW_LOOKDEV_GROUND=1 WW_LOOKDEV_CLOUDTIME=0 "WW_LOOKDEV_PLUGINS=$ESM" \
		WW_RENDER_VIEW=8 WW_CELL_AIR=1 WW_RENDER_SHOT="$(winpath "$png")" "$@" \
		timeout 240 "$exe" --port "$PORT" "$(winpath "$CUBE")" > "$OUT/$tag.log" 2>&1
	if [ -s "$png" ]; then echo "  $tag $(stat -c %s "$png") B"; else echo "  $tag NO PICTURE"; fi
	reg delete "$REGKEY" //f > /dev/null 2>&1
}

# the cell view: Concord (-15,17), the TERRSHADOW1 player-eye camera, a clear weather (a foggy one's air is opaque
# inside 2x the handoff, so the far grid stays off); frames 1 and 2 of one still path are kept
CELLCAM="${FARGRID1_CELLCAM:--61400 68000 6074 -71400 67000 5900 70}"
printf 'fps 16\nframes 3\nkey 0.000000 %s\nkey 0.125000 %s\n' "$CELLCAM" "$CELLCAM" > "$OUT/cell.path"
CELL=( "WW_CELL_OPEN=$ESM|Commonwealth|-15,17|1" "WW_CELL_DATAROOT=$(winpath "$DATA")" WW_CELL_LIT=1 WW_CELL_FARLOD=1
	"WW_RENDER_PATH=$(winpath "$OUT/cell.path")" WW_TAA=0 WW_CELL_IS=0 WW_CELL_PARTICLES=0
	WW_LOOKDEV=1 WW_LOOKDEV_WEATHER=${FARGRID1_WEATHER:-CommonwealthClear} "WW_LOOKDEV_PLUGINS=$ESM" WW_LOOKDEV_SKY=1 WW_LOOKDEV_SUN=1
	WW_LOOKDEV_GROUND=0 WW_LOOKDEV_CLOUDS=0 WW_LOOKDEV_MOON=0 WW_LOOKDEV_FOG=1 WW_LOOKDEV_CLOUDTIME=0 WW_LOOKDEV_HOUR=17
	WW_CELL_GRASS_WIND_T=1 WW_CELL_FARLOD_CARDSWAY=0 WW_CELL_FARLOD_SWAYTIME=1
	WW_SUNPATH=1 "WW_SUNPATH_PIN=${FARGRID1_LOWSUN:-6,240}" WW_CELL_AIR=1 WW_CELL_AIR_TIME=1 )
cellshot() {  # cellshot <exe> <tag> [KEY=VALUE ...] -- <tag>.png = frame 1, <tag>_f2.png = frame 2 (the stable gate)
	local exe="$1" tag="$2" try; shift 2
	rm -f "$OUT/$tag.png" "$OUT/${tag}"_[0-9][0-9][0-9].png "$OUT/${tag}"_f[23].png
	[ -n "$(harness_alive)" ] && { echo "  REFUSED $tag: our harness NifSkope still running"; return 1; }
	for try in 1 2; do
		reg delete "$REGKEY" //f > /dev/null 2>&1
		reg add "$REGKEY\Settings" //v Version //t REG_SZ //d 1 //f > /dev/null 2>&1
		reg add "$REGKEY" //v "Game Manager Version" //t REG_DWORD //d 2 //f > /dev/null 2>&1
		python "$HERE/settings_scope_game.py" "$SCOPE" "$(winpath "$OUT/scope_game.reg")" > /dev/null 2>&1 \
			&& reg import "$(winpath "$OUT/scope_game.reg")" > /dev/null 2>&1
		# shellcheck disable=SC2046
		env $(common_env) "${CELL[@]}" WW_RENDER_SHOT="$(winpath "$OUT/$tag.png")" "$@" \
			timeout 900 "$exe" --port "$PORT" "$(winpath "$ROOT/tests/fixtures/empty.wwcell")" > "$OUT/$tag.log" 2>&1
		[ -s "$OUT/${tag}_002.png" ] && mv -f "$OUT/${tag}_002.png" "$OUT/${tag}_f2.png"
		[ -s "$OUT/${tag}_001.png" ] && mv -f "$OUT/${tag}_001.png" "$OUT/$tag.png"
		rm -f "$OUT/${tag}"_[0-9][0-9][0-9].png
		[ -s "$OUT/$tag.png" ] && break
	done
	if [ -s "$OUT/$tag.png" ]; then echo "  $tag $(stat -c %s "$OUT/$tag.png") B"; else echo "  $tag NO PICTURE"; fi
	reg delete "$REGKEY" //f > /dev/null 2>&1
}
want() { [ -z "$ONLY" ] || [[ "$ONLY" == *",$1,"* ]]; }
aimed() { [ -z "$RED" ] || [ "$RED" = "$1" ]; }
TERR=( WW_SHADOW_TERRAIN=4 WW_SHADOW_TERRAIN_SYNC=1 )

if want off && [ -z "$RED" ]; then
	echo "off"
	shot "$OLD" off_old
	shot "$ARM/NifSkope.exe" off_new
	shot "$ARM/NifSkope.exe" off_pinned WW_CELL_AIR_FAR=0
fi
if want leak && aimed farnoterr; then
	echo "leak"
	cellshot "$ARM/NifSkope.exe" leak_noterr WW_CELL_AIR_FAR=3 WW_CELL_AIR_TERMS=9 WW_CELL_AIR_PROBE=1 WW_SHADOW_TERRAIN=0 "${REDPIN[@]}"
	cellshot "$ARM/NifSkope.exe" leak_terr WW_CELL_AIR_FAR=3 WW_CELL_AIR_TERMS=9 WW_CELL_AIR_PROBE=1 "${TERR[@]}" \
		"WW_SHADOW_DUMP=$(winpath "$OUT/leak_terr.dump")" "${REDPIN[@]}"
	cellshot "$ARM/NifSkope.exe" leak_view WW_CELL_AIR_FAR=3 "${TERR[@]}" "${REDPIN[@]}"	# the picture to audit
fi
if want seam && aimed farseam; then
	echo "seam"
	cellshot "$ARM/NifSkope.exe" seam_off WW_CELL_AIR_FAR=0 WW_CELL_AIR_TERMS=9 WW_CELL_AIR_PROBE=1 WW_SHADOW_TERRAIN=0 "${REDPIN[@]}"
	cellshot "$ARM/NifSkope.exe" seam_on WW_CELL_AIR_FAR=3 WW_CELL_AIR_TERMS=9 WW_CELL_AIR_PROBE=1 WW_SHADOW_TERRAIN=0 "${REDPIN[@]}"
fi
if want stable && aimed farjitter; then
	echo "stable"
	[ -z "$RED" ] && cellshot "$ARM/NifSkope.exe" stable_base WW_CELL_AIR_FAR=0 "${TERR[@]}"
	cellshot "$ARM/NifSkope.exe" stable_far WW_CELL_AIR_FAR=3 "${TERR[@]}" "${REDPIN[@]}"
fi
if want perf && [ -z "$RED" ]; then
	echo "perf"
	for t in 0 1 2 3 4; do
		cellshot "$ARM/NifSkope.exe" perf_$t WW_RENDER_SIZE=1920x1080 WW_RENDER_GPUMS=60 WW_CELL_AIR_FAR=$t "${TERR[@]}"
	done
fi

python "$HERE/fargrid1_gates.py" --out "$(winpath "$OUT")" --red "${RED:-none}"
