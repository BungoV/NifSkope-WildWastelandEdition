#!/bin/bash
# lane FIX1031B: the crest glow can be seen facing a low sun (gate.py: the real water_sss.glsl through the driver at the
# camera of bungo's 10-31 sea frame, the water's light calibrated from that frame). Green must PASS; both reds must
# FAIL: --red 512 (WW_CELL_WATER_OCEAN_RED=dimglow, the gain before) and main 5422be6c's own water_sss.glsl.
# PY= the python with numpy (code1030 unit.sh puts MSYS2 first on PATH).
cd "$(dirname "$0")/../.." || exit 2
PY="${PY:-$(command -v python)}"
T=tests/watercrestglow/gate.py
ok=1
g=$(timeout 300 "$PY" -I "$T" < /dev/null 2>&1 | grep '^VERDICT')
echo "green       $g"
case "$g" in "VERDICT PASS"*) ;; *) ok=0 ;; esac
r=$(timeout 300 "$PY" -I "$T" --red 512 < /dev/null 2>&1 | grep '^VERDICT')
echo "red dimglow $r"
case "$r" in "VERDICT FAIL"*) ;; *) ok=0 ;; esac
D=$(mktemp -d)
if git archive 5422be6c res/shaders | tar -x -C "$D"; then
	m=$(timeout 300 "$PY" -I "$T" --shaders "$D/res/shaders" < /dev/null 2>&1 | grep '^VERDICT')
	echo "red main    $m"
	case "$m" in "VERDICT FAIL"*) ;; *) ok=0 ;; esac
else
	echo "red main    (main 5422be6c not in this clone: skipped)"
fi
rm -rf "$D"
if [ $ok = 1 ]; then echo "WATERCRESTGLOW RESULT PASS"; exit 0; fi
echo "WATERCRESTGLOW RESULT FAIL"; exit 1
