#!/usr/bin/env bash
# light_angles.sh -- lane LIGHTANGLES1 (2026-09-24): are the viewer's light
# angles remembered across a restart?
#
# THE DEFECT: src/ui/widgets/lightingwidget.cpp saved the two angles of the
# world-fixed light (declination, planar angle; quarter-degree integers) under
# "Settings/Render/Lighting/Declination" / ".../Planar Angle", and the load read
# "Lighting/Declination" / "Lighting/Planar Angle", which nothing writes. The load
# also folded `tmp % 720`, so a saved +-180 degrees came back as 0.
#
# TWO LEGS, both inside ONE scratch settings scope (WW_SETTINGS_SCOPE=lightangles1,
# HKCU\Software\NifTools\NifSkope 2.0 lightangles1), deleted at the start and the
# end. bungo's own settings key is never reached.
#
#  roundtrip  the in-app harness (src/lightanglestest.cpp, WW_LIGHTANGLES_TEST),
#             chained launches: launch k READS the pair launch k-1 SAVED (through
#             the real Save Lighting action), then saves the next pair. The pairs
#             include the range ends +-180 on both angles. Pass = every launch
#             PASS: loaded == saved within one slider step (0.25 degree).
#             Plus one launch reading a PLANTED out-of-range value (1000, -1600
#             quarter-degrees), which must wrap the way rotateLight wraps
#             (-110, -40), not fold.
#             Needs an exe that carries the harness: the rung
#             release/before_lightangles1 does NOT (it predates this file), so
#             its red control is the same harness built against the unchanged
#             lightingwidget.cpp (EXE=<that build>).
#  picture    runs on ANY exe, including the literal rung: plant the keys the
#             save writes (declination 720 = 180 deg, planar 360 = 90 deg,
#             Frontal Light off), photograph the BGSM duct; plant 0, 0 and
#             photograph it twice (the noise bar). Pass = the planted angles
#             move the picture beyond the bar. On the rung the load ignores the
#             key, both pictures are lit from 0, 0, and the leg FAILS.
#
# usage: [EXE=...] bash tests/spells/light_angles.sh [--leg roundtrip|picture|all] [--out DIR]
# One NifSkope instance at a time, second monitor, --port unused; bungo's own
# window (no --port on its command line) is never touched.

set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
. "$HERE/_harness.sh"

EXE="${EXE:-$ROOT/release/NifSkope.exe}"
LEG=all
OUT="$ROOT/scratchpad/light_angles_$(date +%Y%m%d_%H%M%S)"
while [ $# -gt 0 ]; do
	case "$1" in
		--leg) LEG="$2"; shift 2 ;;
		--out) OUT="$2"; shift 2 ;;
		*) echo "unknown argument $1"; exit 2 ;;
	esac
done
mkdir -p "$OUT"
[ "$(head -c 2 "$EXE")" = "MZ" ] || { echo "REFUSED: $EXE is not a finished image"; exit 2; }

SCOPE=lightangles1
REGKEY="HKCU\\Software\\NifTools\\NifSkope 2.0 $SCOPE"
LIGHTKEY="$REGKEY\\Settings\\Render\\Lighting"
PORT="${LIGHTANGLES_PORT:-43251}"
DATA="/e/Tools/Fallout 4/DataUnpacked/Data"
NIF="$DATA/Meshes/SetDressing/ACDucts/ACDuctConnector01.nif"

harness_alive() {
	powershell -NoProfile -Command "Get-CimInstance Win32_Process -Filter \"Name='NifSkope.exe'\" | Where-Object { \$_.CommandLine -match '--port' } | ForEach-Object { \$_.ProcessId }" | tr -d '\r'
}
wipe_scope() { reg delete "$REGKEY" //f > /dev/null 2>&1; }
plant() {  # plant <declination quarter-degrees> <planar quarter-degrees> [frontal true|false]
	reg add "$LIGHTKEY" //v "Declination" //t REG_SZ //d "$1" //f > /dev/null
	reg add "$LIGHTKEY" //v "Planar Angle" //t REG_SZ //d "$2" //f > /dev/null
	[ -n "${3:-}" ] && reg add "$LIGHTKEY" //v "Frontal Light" //t REG_SZ //d "$3" //f > /dev/null
	return 0
}

TOTAL_FAIL=0
echo "exe: $EXE ($(sha1sum "$EXE" | cut -c1-8), $(stat -c %y "$EXE" | cut -c1-19))"

# ---- leg 1: the round trip through the real save and a real restart
launch() {  # launch <tag> <expect|-> <save|->
	local tag="$1" exp="$2" sav="$3" log="$OUT/$1.log"
	local alive; alive="$(harness_alive)"
	[ -n "$alive" ] && { echo "  REFUSED $tag: harness NifSkope still running (pid $alive)"; TOTAL_FAIL=$((TOTAL_FAIL + 1)); return; }
	rm -f "$log"
	local envs=( WW_SETTINGS_SCOPE=$SCOPE WW_LIGHTANGLES_TEST=1 WW_LIGHTANGLES_LOG="$(winpath "$log")" )
	[ "$exp" != "-" ] && envs+=( WW_LIGHTANGLES_EXPECT="$exp" )
	[ "$sav" != "-" ] && envs+=( WW_LIGHTANGLES_SAVE="$sav" )
	env "${envs[@]}" "$EXE" --port "$PORT" > /dev/null 2>&1 &
	local pid=$!
	for _ in $(seq 1 90); do
		[ -f "$log" ] && grep -q '^done$' "$log" 2>/dev/null && break
		kill -0 "$pid" 2>/dev/null || break
		sleep 1
	done
	for _ in $(seq 1 20); do kill -0 "$pid" 2>/dev/null || break; sleep 1; done
	kill "$pid" 2>/dev/null; wait "$pid" 2>/dev/null
	if [ ! -f "$log" ]; then
		echo "  $tag: FAIL (the harness wrote no log)"; TOTAL_FAIL=$((TOTAL_FAIL + 1)); return
	fi
	grep -E "^  (FAIL|ok)|loaded|stored|REFUSED" "$log" | sed "s/^/  $tag |/"
	local verdict; verdict="$(grep -E '^(PASS|FAIL)$' "$log" | tail -1)"
	echo "  $tag: ${verdict:-NO VERDICT} ($(grep -E '^[0-9]+ checks' "$log"))"
	[ "$verdict" = "PASS" ] || TOTAL_FAIL=$((TOTAL_FAIL + 1))
}

if [ "$LEG" = all ] || [ "$LEG" = roundtrip ]; then
	echo "--- leg roundtrip (scope $SCOPE) ---"
	wipe_scope
	launch rt1 -             "37.3,-123.4"
	launch rt2 "37.3,-123.4" "180,-180"
	launch rt3 "180,-180"    "-180,180"
	launch rt4 "-180,180"    "0.3,179.8"
	launch rt5 "0.3,179.8"   -
	plant 1000 -1600
	launch rt6 "-110,-40"    -
	wipe_scope
fi

# ---- leg 2: the picture, on any exe
if [ "$LEG" = all ] || [ "$LEG" = picture ]; then
	echo "--- leg picture (scope $SCOPE) ---"
	ARM="$(dirname "$EXE")"
	shot() {  # shot <tag> <decl> <planar>
		local png="$OUT/$1.png"
		rm -f "$png"
		local alive; alive="$(harness_alive)"
		[ -n "$alive" ] && { echo "  REFUSED $1: harness NifSkope still running (pid $alive)"; return; }
		wipe_scope
		plant "$2" "$3" false
		env WW_SETTINGS_SCOPE=$SCOPE \
			WW_LODGEN_RESOURCES="$(winpath "$DATA")" \
			WW_RENDER_SHOT="$(winpath "$png")" \
			WW_RENDER_SIZE=1280x859 WW_RENDER_VIEW=8 WW_RENDER_TIME=1.0 WW_RENDER_CLEAN=1 \
			WW_RENDER_FLAT=0 WW_RENDER_REFRACTION=1 WW_RENDER_SS=0 \
			WW_PBRM_MODE=legacy WW_PBRM_AUTOREPLACE=1 WW_LIGHTING_MODE=legacy WW_LOOKDEV=0 \
			WW_RENDER_PARTICLES=1 WW_RENDER_SHADOWS=0 WW_RENDER_CONTACT=0 WW_RENDER_AO=0 WW_RENDER_SSGI=0 \
			timeout 180 "$EXE" --port "$PORT" "$(winpath "$NIF")" > "$OUT/$1.run.log" 2>&1
		[ -s "$png" ] && echo "  $1: $(stat -c %s "$png") B" || echo "  $1: NO PICTURE"
	}
	[ -d "$ARM/shaders" ] || echo "  note: no shaders/ beside $EXE"
	shot pic_zero_a 0 0
	shot pic_zero_b 0 0
	shot pic_angles 720 360
	wipe_scope
	python - "$OUT" <<'PYEOF' || TOTAL_FAIL=$((TOTAL_FAIL + 1))
import sys, os
from PIL import Image, ImageChops
d = sys.argv[1]
p = {k: os.path.join(d, k + '.png') for k in ('pic_zero_a', 'pic_zero_b', 'pic_angles')}
miss = [k for k, v in p.items() if not os.path.isfile(v)]
if miss:
    print('  picture: FAIL (no picture: %s)' % ', '.join(miss)); sys.exit(1)
im = {k: Image.open(v).convert('RGB') for k, v in p.items()}
def ndiff(a, b):
    if im[a].size != im[b].size:
        return -1
    return sum(1 for px in ImageChops.difference(im[a], im[b]).getdata() if px != (0, 0, 0))
noise = ndiff('pic_zero_a', 'pic_zero_b')
moved = ndiff('pic_zero_a', 'pic_angles')
BAR = max(1000, 10 * noise)
print('  picture: noise (0,0 vs 0,0) %d px; planted 180,90 vs 0,0 %d px; bar %d px' % (noise, moved, BAR))
ok = noise >= 0 and moved > BAR
print('  picture: %s (the planted angles %s the picture)' % ('PASS' if ok else 'FAIL', 'moved' if ok else 'did NOT move'))
sys.exit(0 if ok else 1)
PYEOF
fi

echo "failed launches/legs: $TOTAL_FAIL"
[ "$TOTAL_FAIL" -eq 0 ] && echo PASS || { echo FAIL; exit 1; }
