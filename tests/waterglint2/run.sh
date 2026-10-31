#!/bin/bash
# lane FIX1031B (WATERGLINT2): the near sun path keeps its shown brightness with the glitter on, and breaks into specks
# (gate.py: the real water_refl.glsl + water_glitter.glsl + fo4_water.frag's glare lines through the driver, at the
# cameras of bungo's 10-31 river and sea frames). Green must PASS; both reds must FAIL: --red 128
# (WW_CELL_WATER_GLINT_RED=sparse, WATERGLINT1's count and order) and main 5422be6c's own shader files.
# PY= the python with numpy (code1030 unit.sh puts MSYS2 first on PATH).
cd "$(dirname "$0")/../.." || exit 2
PY="${PY:-$(command -v python)}"
T=tests/waterglint2/gate.py
ok=1
g=$(timeout 600 "$PY" -I "$T" < /dev/null 2>&1 | grep '^VERDICT')
echo "green      $g"
case "$g" in "VERDICT PASS"*) ;; *) ok=0 ;; esac
r=$(timeout 600 "$PY" -I "$T" --red 128 < /dev/null 2>&1 | grep '^VERDICT')
echo "red sparse $r"
case "$r" in "VERDICT FAIL"*) ;; *) ok=0 ;; esac
D=$(mktemp -d)
if git archive 5422be6c res/shaders | tar -x -C "$D"; then
	m=$(timeout 600 "$PY" -I "$T" --shaders "$D/res/shaders" < /dev/null 2>&1 | grep '^VERDICT')
	echo "red main   $m"
	case "$m" in "VERDICT FAIL"*) ;; *) ok=0 ;; esac
else
	echo "red main   (main 5422be6c not in this clone: skipped)"
fi
rm -rf "$D"
if [ $ok = 1 ]; then echo "WATERGLINT2 RESULT PASS"; exit 0; fi
echo "WATERGLINT2 RESULT FAIL"; exit 1
