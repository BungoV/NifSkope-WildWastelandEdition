#!/bin/bash
# lane FARTOGGLE4: far objects that go on and off on a MOVING camera with the temporal AA off (bungo's settings:
# 4x MSAA, FOV 60, both TAA rows off). A REFUSAL as a test: selftest.py proves the 4x coverage rule is the owner and
# that no min-footprint rule removes the blink without > 1.25x ink (green = the ideal raster 0 blinks; red = today's
# rule must FAIL "no blink"). census.py repeats it on the real far rings of Concord when the bake + LODPLACE1's drawn
# rows + the c3 move row's camera log are on disk (skipped otherwise; numbers in WW_CHANGES.md).
# PY= the python with numpy (code1030 unit.sh puts MSYS2 first on PATH).
cd "$(dirname "$0")/../.." || exit 2
PY="${PY:-$(command -v python)}"
ok=1
s=$(timeout 120 "$PY" -I tests/farshimmer/selftest.py < /dev/null 2>&1)
echo "$s" | grep -E "^(ok|FAIL)|SELFTEST"
case "$s" in *"FARSHIMMER SELFTEST PASS"*) ;; *) ok=0 ;; esac
BAKE="E:/Projects/Fallout 4 Mods/mods/Fo4CS_Bakes/FO4CSLOD/Commonwealth"
PLACE="E:/Projects/Claude/notes/code1030/lodplace1/walk/still1/place.txt"
CAM="E:/Projects/Claude/notes/merge1030a/hold/run2/c3_rt_move_path.txt"
if [ -f "$BAKE/Commonwealth.lodi" ] && [ -f "$PLACE" ] && [ -f "$CAM" ]; then
	c=$(timeout 600 "$PY" -I tests/farshimmer/census.py --bake "$BAKE" --place "$PLACE" --path "$CAM" < /dev/null 2>&1)
	echo "$c" | grep -E "^(population|CENSUS)"
	case "$c" in *"refusal HOLDS"*) ;; *) ok=0 ;; esac
else
	echo "census skipped: the Concord bake / drawn rows / camera log are not on this machine"
fi
if [ $ok = 1 ]; then echo "FARSHIMMER RESULT PASS"; exit 0; fi
echo "FARSHIMMER RESULT FAIL"; exit 1
