#!/bin/bash
# lane FIX1101A area T (LATE ITEM 26): the cell view's tone curve is Fusion Shaders' Physical operator (gate.py: the real
# cell_tonemap.glsl + cellImageSpace + cellCurveW/Inv through the driver vs physical.py, a numpy port of FO4CS
# res/Post/TonemapCS.hlsl; max display error < 1/255 over a grey ramp 0..64, Macbeth-ish patches and saturated ramps).
# Green must PASS; both reds must FAIL: --red vanilla (WW_TONEMAP_RED=vanilla, the game's filmic curve) and the base
# c0a30fab's own shader files (the curve before this lane).
# PY= the python with numpy (code1030 unit.sh puts MSYS2 first on PATH).
cd "$(dirname "$0")/../../../.." || exit 2
PY="${PY:-$(command -v python)}"
T=tests/features/exposure/tonemap1101a/gate.py
ok=1
g=$(timeout 300 "$PY" -I "$T" < /dev/null 2>&1 | grep '^VERDICT')
echo "green       $g"
case "$g" in "VERDICT PASS"*) ;; *) ok=0 ;; esac
r=$(timeout 300 "$PY" -I "$T" --red vanilla < /dev/null 2>&1 | grep '^VERDICT')
echo "red vanilla $r"
case "$r" in "VERDICT FAIL"*) ;; *) ok=0 ;; esac
D=$(mktemp -d)
if git archive c0a30fab res/shaders | tar -x -C "$D"; then
	m=$(timeout 300 "$PY" -I "$T" --shaders "$D/res/shaders" < /dev/null 2>&1 | grep '^VERDICT')
	echo "red base    $m"
	case "$m" in "VERDICT FAIL"*) ;; *) ok=0 ;; esac
else
	echo "red base    (c0a30fab not in this clone: skipped)"
fi
rm -rf "$D"
if [ $ok = 1 ]; then echo "TONEMAP1101A RESULT PASS"; exit 0; fi
echo "TONEMAP1101A RESULT FAIL"; exit 1
