#!/bin/bash
# lane FIX1031B: the river's waves move at the river's speed (gate.py: the real wwWaterOceanFlowed through the driver,
# the ocean stubbed so its result is the distance the flow carried the waves). Green must PASS; both reds must FAIL:
# --red 1024 (WW_CELL_WATER_OCEAN_RED=rawflow, the bake read as u/s) and main 5422be6c's own water_flowwaves.glsl.
# PY= the python with numpy (code1030 unit.sh puts MSYS2 first on PATH).
cd "$(dirname "$0")/../.." || exit 2
PY="${PY:-$(command -v python)}"
T=tests/waterflowspeed/gate.py
ok=1
g=$(timeout 300 "$PY" -I "$T" < /dev/null 2>&1 | grep '^VERDICT')
echo "green       $g"
case "$g" in "VERDICT PASS"*) ;; *) ok=0 ;; esac
r=$(timeout 300 "$PY" -I "$T" --red 1024 < /dev/null 2>&1 | grep '^VERDICT')
echo "red rawflow $r"
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
if [ $ok = 1 ]; then echo "WATERFLOWSPEED RESULT PASS"; exit 0; fi
echo "WATERFLOWSPEED RESULT FAIL"; exit 1
