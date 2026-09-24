#!/usr/bin/env bash
# pbr_wx1_gates.sh -- the gates of lane PBRWX1, the weather preview in the Scene
# window (Sky dome, Sun disc + light, Clouds, Moon, Game Day), judged by
# pbr_wx1_gates.py against its OWN plugin decoder, textbook CIELab, the spec
# formulas and a DXT5 decode -- never the code under test.
#
#   sky      one `weather --sky` CLI run (hours 12 7 19.5 9 16 1, the disc-fade and
#            colour-extension edges, 22 6.3; days 4 17 32; cloud time 100 s): the
#            judge's cli / skycol / sundir / edges / cloud / moon checks
#   skypx    the dome's zenith pixel at 12:00, 07:00, 19:30 (Sky on, nothing else;
#            looking straight up, the model behind the camera)
#   probe    the cloud texel probe on layer 14 (WW_LOOKDEV_CLOUDPROBE): t=0 at uv,
#            t=0 at uv + the judge's 100 s offset, t=100 at uv, t=470.37 at uv
#   lit      the lookdev light at 16:00 with Sun on (the arc) and off (W1's light)
#   off      every part OFF = release/before_pbrwx1 byte for byte: 3 framings
#            (view 8 noon; straight up, FOV 120, at 12:00 and 01:00), each with no
#            pins and with every WW_LOOKDEV_{SKY,SUN,CLOUDS,MOON}=0
#   persist  a scope seeded with every part ON and Game Day 17 comes back that way
#   live     the in-app weather leg (WW_SCENE_TEST_LOOKDEV=1 WW_SCENE_TEST_WEATHER=1)
#   zero     = bash tests/spells/pbr_shade_ab.sh --old release/before_pbrwx1 (run apart)
#
# usage: bash tests/spells/pbr_wx1_gates.sh [--out DIR] [--only sky,skypx,...] [--red NAME]
#   CLI reds (WW_LOOKDEV_RED): exegmst rgbblend nonightbranch sunfade2h sunalpha1
#     colorext05 phasestuck moonalpha1 nam1ignore speedswap cloudalphaone hourstuck
#     cloudgametime todorder
#   stage reds (WW_LOOKDEV_RED): skyscale skygamma skyswap (skypx), skyleak sunleak
#     cloudleak moonleak (off)
#   WW_R2A_RED: nolive nosave (live)
# Every red must end FAIL. One NifSkope of ours at a time (our --port), second
# monitor; bungo's window (no --port) and other lanes' harnesses are never touched.

set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
. "$HERE/_harness.sh"

OUT="$ROOT/scratchpad/pbr_wx1_gates_$(date +%Y%m%d_%H%M%S)"
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
GAME="${PBR_WX1_GAME:-/x/Programs/Steam/steamapps/common/Fallout 4/Data}"
PBRDATA="$ROOT/tests/fixtures/pbr_r2a_data"
ARM="$ROOT/release"
OLD="${PBR_WX1_OLD:-$ROOT/release/before_pbrwx1}"
PORT="${PBR_WX1_PORT:-43247}"
SCOPE=pbrwx1gates
REGKEY="HKCU\\Software\\NifTools\\NifSkope 2.0 $SCOPE"
mkdir -p "$OUT"
OUT="$(cd "$OUT" && pwd)"
[ "$(head -c 2 "$ARM/NifSkope.exe")" = "MZ" ] || { echo "REFUSED: release/NifSkope.exe is not a finished image"; exit 2; }
[ -f "$OLD/NifSkope.exe" ] || { echo "REFUSED: no rung $OLD/NifSkope.exe"; exit 2; }
[ -f "$GAME/Fallout4.esm" ] || { echo "REFUSED: no Fallout4.esm in $GAME"; exit 2; }
python "$HERE/pbr_r2a_fixtures.py" > /dev/null || exit 2

REDPIN=""
case "$RED" in
	'') ;;
	exegmst|rgbblend|nonightbranch|sunfade2h|sunalpha1|colorext05|phasestuck|moonalpha1|nam1ignore|speedswap|\
	cloudalphaone|hourstuck|cloudgametime|todorder|skyscale|skygamma|skyswap|skyleak|sunleak|cloudleak|moonleak)
		REDPIN="WW_LOOKDEV_RED=$RED" ;;
	nolive|nosave) REDPIN="WW_R2A_RED=$RED" ;;
	*) echo "unknown red $RED"; exit 2 ;;
esac
{ echo "red=${RED:-none}"; sha1sum "$ARM/NifSkope.exe" "$OLD/NifSkope.exe"; echo "game=$GAME"; } > "$OUT/arms.txt"
printf '%s\n' "$(winpath "$GAME")" > "$OUT/game.txt"

harness_alive() {  # OUR harness only: the one on our port
	powershell -NoProfile -Command "Get-CimInstance Win32_Process -Filter \"Name='NifSkope.exe'\" | Where-Object { \$_.CommandLine -match '--port $PORT' } | ForEach-Object { \$_.ProcessId }" | tr -d '\r'
}

common_env() {
	echo WW_SETTINGS_SCOPE=$SCOPE WW_WINDOW_AT=1960,40 WW_RENDER_SIZE=1280x859 \
		WW_RENDER_TIME=1.0 WW_RENDER_CLEAN=1 WW_RENDER_FLAT=0 WW_RENDER_REFRACTION=1 WW_RENDER_SS=0 \
		WW_PBRM_MODE=pbr WW_PBRM_AUTOREPLACE=1 WW_RENDER_PARTICLES=1 \
		WW_RENDER_SHADOWS=0 WW_RENDER_CONTACT=0 WW_RENDER_AO=0 WW_RENDER_SSGI=0
}
LOOK="WW_LIGHTING_MODE=lookdev WW_LOOKDEV=1 WW_VIEW_TRANSFORM=standard WW_EXPOSURE_EV=0"

wx() {  # wx <tag> [KEY=VALUE ...] -- <weather args>: one CLI run
	local tag="$1"; shift
	local envs=()
	while [ $# -gt 0 ] && [ "$1" != "--" ]; do envs+=("$1"); shift; done
	shift
	env "${envs[@]}" WW_LOOKDEV_DATA="$(winpath "$GAME")" \
		"$ARM/NifSkope.exe" -no-gui weather --data "$(winpath "$GAME")" "$@" > "$OUT/$tag.txt" 2>&1
	echo "rc=$?" >> "$OUT/$tag.txt"
	echo "  $tag $(grep -cE '^(skyclock|cloud|moon) ' "$OUT/$tag.txt") lines"
}

shot() {  # shot <exe dir> <tag> [KEY=VALUE ...] -- one picture + census; KEEPREG=1 keeps the scope
	local arm="$1" tag="$2"; shift 2
	local nif="$PBRDATA/Meshes/WWPbrTest02/studio.nif"
	local png="$OUT/$tag.png"
	rm -f "$png" "$OUT/$tag.prog.txt" "$OUT/$tag.pbrm.txt"
	local alive; alive="$(harness_alive)"
	if [ -n "$alive" ]; then echo "  REFUSED $tag: our harness NifSkope still running (pid $alive)"; return 1; fi
	[ "${KEEPREG:-0}" = 1 ] || reg delete "$REGKEY" //f > /dev/null 2>&1
	# shellcheck disable=SC2046
	env $(common_env) $LOOK \
		WW_LODGEN_RESOURCES="$(winpath "$DATA");$(winpath "$PBRDATA")" \
		WW_LOOKDEV_DATA="$(winpath "$GAME")" \
		WW_RENDER_SHOT="$(winpath "$png")" \
		WW_PROGRAM_CENSUS="$(winpath "$OUT/$tag.prog.txt")" \
		WW_PBRM_CENSUS="$(winpath "$OUT/$tag.pbrm.txt")" \
		"$@" \
		timeout 240 "$arm/NifSkope.exe" --port "$PORT" "$(winpath "$nif")" > "$OUT/$tag.log" 2>&1
	local rc=$?
	if [ -s "$png" ]; then echo "  $tag rc=$rc $(stat -c %s "$png") B"; else echo "  $tag rc=$rc NO PICTURE"; fi
	reg delete "$REGKEY" //f > /dev/null 2>&1
}

harness() {  # harness <tag> [KEY=VALUE ...]
	local tag="$1"; shift
	local nif="$PBRDATA/Meshes/WWPbrTest02/studio.nif"
	rm -f "$OUT/$tag.harness.log"
	local alive; alive="$(harness_alive)"
	if [ -n "$alive" ]; then echo "  REFUSED $tag: our harness NifSkope still running (pid $alive)"; return 1; fi
	reg delete "$REGKEY" //f > /dev/null 2>&1
	# shellcheck disable=SC2046
	env $(common_env) \
		WW_LODGEN_RESOURCES="$(winpath "$DATA");$(winpath "$PBRDATA")" \
		WW_LOOKDEV_DATA="$(winpath "$GAME")" \
		WW_RENDER_VIEW=8 \
		WW_SCENE_TEST=1 WW_SCENE_TEST_LOG="$(winpath "$OUT/$tag.harness.log")" \
		"$@" \
		timeout 300 "$ARM/NifSkope.exe" --port "$PORT" "$(winpath "$nif")" > "$OUT/$tag.log" 2>&1
	local rc=$?
	echo "  $tag rc=$rc $(tail -1 "$OUT/$tag.harness.log" 2>/dev/null | tr -d '\r')"
	reg delete "$REGKEY" //f > /dev/null 2>&1
}

want() { [ -z "$ONLY" ] || [[ "$ONLY" == *",$1,"* ]]; }
aimed() { local r; [ -z "$RED" ] && return 0; for r in "$@"; do [ "$RED" = "$r" ] && return 0; done; return 1; }
UP="WW_RENDER_VIEW=2 WW_RENDER_CENTER=0,0,100000 WW_RENDER_DIST=10"
NOPART="WW_LOOKDEV_SKY=0 WW_LOOKDEV_SUN=0 WW_LOOKDEV_CLOUDS=0 WW_LOOKDEV_MOON=0"

# shellcheck disable=SC2086
if want sky && aimed exegmst rgbblend nonightbranch sunfade2h sunalpha1 colorext05 phasestuck moonalpha1 \
		nam1ignore speedswap cloudalphaone hourstuck cloudgametime todorder; then
	echo "sky"
	wx sky $REDPIN -- --plugins Fallout4.esm --weather CommonwealthClear --sky \
		--hour 12,7,19.5,9,16,1,6.9,6.95,7.0,7.1,18.95,19.1,2.99,3.01,22.99,23.01,22,6.3 --day 4,17,32 --cloudtime 100
fi
# shellcheck disable=SC2086
if want skypx && aimed rgbblend skyscale skygamma skyswap hourstuck; then
	echo "skypx"
	for h in 12 7 19.5; do
		shot "$ARM" skypx_$h $UP WW_RENDER_FOV=60 WW_LOOKDEV_HOUR=$h WW_LOOKDEV_SKY=1 WW_LOOKDEV_SUN=0 \
			WW_LOOKDEV_CLOUDS=0 WW_LOOKDEV_MOON=0 $REDPIN
	done
fi
# shellcheck disable=SC2086
if want probe && aimed skyscale speedswap cloudgametime cloudalphaone; then
	echo "probe"
	P="$(python "$HERE/pbr_wx1_gates.py" --pick 14 --out "$OUT" | tr -d '\r')"
	echo "  pick: $P"
	echo "$P" > "$OUT/probe.txt"
	read -r L U V <<< "$P"
	OFF="$(python "$HERE/pbr_wx1_gates.py" --offset "$L" 100 --out "$OUT" | tr -d '\r')"
	read -r DU DV <<< "$OFF"
	US="$(python -c "print('%.8f' % ((($U + $DU)) % 1.0))")"
	VS="$(python -c "print('%.8f' % ((($V + $DV)) % 1.0))")"
	echo "  offset after 100 s: $DU $DV -> $US $VS"
	PB="$UP WW_RENDER_FOV=60 WW_LOOKDEV_HOUR=12 WW_LOOKDEV_SKY=0 WW_LOOKDEV_SUN=0 WW_LOOKDEV_CLOUDS=1 WW_LOOKDEV_MOON=0"
	shot "$ARM" probe_t0 $PB WW_LOOKDEV_CLOUDTIME=0 WW_LOOKDEV_CLOUDPROBE=$L,$U,$V $REDPIN
	shot "$ARM" probe_t0_shift $PB WW_LOOKDEV_CLOUDTIME=0 WW_LOOKDEV_CLOUDPROBE=$L,$US,$VS $REDPIN
	shot "$ARM" probe_t100 $PB WW_LOOKDEV_CLOUDTIME=100 WW_LOOKDEV_CLOUDPROBE=$L,$U,$V $REDPIN
	shot "$ARM" probe_t470 $PB WW_LOOKDEV_CLOUDTIME=470.37 WW_LOOKDEV_CLOUDPROBE=$L,$U,$V $REDPIN
fi
# shellcheck disable=SC2086
if want lit && aimed exegmst nonightbranch hourstuck; then
	echo "lit"
	shot "$ARM" lit WW_RENDER_VIEW=8 WW_LOOKDEV_HOUR=16 WW_LOOKDEV_SUN=1 $REDPIN
	shot "$ARM" litoff WW_RENDER_VIEW=8 WW_LOOKDEV_HOUR=16 $NOPART $REDPIN
fi
# shellcheck disable=SC2086
if want off && aimed skyleak sunleak cloudleak moonleak; then
	echo "off"
	for f in v8 up12 up1; do
		case $f in
			v8) FR="WW_RENDER_VIEW=8 WW_LOOKDEV_HOUR=12" ;;
			up12) FR="$UP WW_RENDER_FOV=120 WW_LOOKDEV_HOUR=12" ;;
			up1) FR="$UP WW_RENDER_FOV=120 WW_LOOKDEV_HOUR=1" ;;
		esac
		shot "$OLD" off_${f}_old $FR
		shot "$ARM" off_${f}_new $FR $REDPIN
		shot "$ARM" off_${f}_pinned_new $FR $NOPART $REDPIN
	done
fi
# shellcheck disable=SC2086
if want persist && [ -z "$RED" ]; then
	echo "persist"
	reg delete "$REGKEY" //f > /dev/null 2>&1
	for k in Sky Sun Clouds Moon; do
		reg add "$REGKEY\\Settings\\Render\\Scene" //v "Lookdev $k" //t REG_SZ //d true //f > /dev/null
	done
	reg add "$REGKEY\\Settings\\Render\\Scene" //v "Lookdev Game Day" //t REG_SZ //d 17 //f > /dev/null
	KEEPREG=1 shot "$ARM" persist WW_RENDER_VIEW=8 WW_LOOKDEV_HOUR=22
fi
# shellcheck disable=SC2086
if want live && aimed nolive nosave; then
	echo "live"
	harness live WW_SCENE_TEST_LOOKDEV=1 WW_SCENE_TEST_WEATHER=1 $REDPIN
fi

python "$HERE/pbr_wx1_gates.py" --out "$OUT" --red "${RED:-none}"
