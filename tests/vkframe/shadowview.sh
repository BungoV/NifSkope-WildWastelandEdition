#!/bin/bash
# Lane VKSHADOW1 (2026-11-13, row G6): the sun cascades' queued casters on the present device (vkpresent.cpp csmRun) against
# GL's cascade program drawn the way sunshadow.cpp draws it, into D16 2D arrays, every texel compared.
# bash tests/vkframe/shadowview.sh <out dir> [map]. Needs MSYS2 UCRT64 g++.
# Not while NifSkope or the game runs (a window of ours): the script refuses then.
# Last line: `SHADOWVIEW GATE OK` or `SHADOWVIEW GATE FAILED`.
set -u
OUT="${1:?out dir}"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
export PATH="/c/msys64/ucrt64/bin:$PATH"
if tasklist | grep -qiE '^(NifSkope|Fallout4)'; then echo "SHADOWVIEW GATE FAILED (NifSkope or the game runs: no window of ours now)"; exit 1; fi
mkdir -p "$OUT"
g++ -O2 -std=c++17 -pthread -I"$ROOT/src" "$ROOT/tests/vkframe/shadowview.cpp" -o "$OUT/shadowview.exe" -lopengl32 -lgdi32 -luser32 \
	|| { echo "SHADOWVIEW GATE FAILED (g++)"; exit 1; }
"$OUT/shadowview.exe" "$ROOT" "${2:-1024}" 2>&1 | tee "$OUT/shadowview.txt"
tail -1 "$OUT/shadowview.txt" | grep -q "SHADOWVIEW GATE OK" || exit 1
# Lane VKSHADOW4: the mesh pool's reds, each its own process (the pool lives as long as the process). Red 1 (key only) must
# fail a frame1 layer past 1 unit with redSkips >= 1, red 2 (grow without copying again) a frame2 layer with repacks >= 1.
for R in 1 2; do
	WW_VKCSM_POOL_RED=$R "$OUT/shadowview.exe" "$ROOT" "${2:-1024}" > "$OUT/shadowview_pool$R.txt" 2>&1
	fails=$(grep -cE "^SHADOWVIEW mesh frame$R [a-z]+/[0-9] FAIL \([0-9]+ of [0-9]+ covered texels differ, [1-9][0-9]* past 1 unit" "$OUT/shadowview_pool$R.txt")
	if [ "$R" = 1 ]; then trig=$(grep -cE "^SHADOWVIEW mesh pool frame1 .*redSkips=[1-9]" "$OUT/shadowview_pool$R.txt")
	else trig=$(grep -cE "^SHADOWVIEW mesh pool frame2 .*repacks=[1-9]" "$OUT/shadowview_pool$R.txt"); fi
	if [ "$fails" -gt 0 ] && [ "$trig" -gt 0 ]; then
		echo "SHADOWVIEW RED FAIL pool$R ($fails frame$R layers past 1 unit, trigger lines $trig; must FAIL)"
	else
		echo "SHADOWVIEW RED TESTS NOTHING pool$R ($fails frame$R layers past 1 unit, trigger lines $trig)"
		echo "SHADOWVIEW GATE FAILED"
		exit 1
	fi
done
# Lane VKSHADOW6: the remap pipeline built GL_ALWAYS (it overwrites GL's casters): a remap frame1 layer must fail past 1 unit
# with remapRed >= 1 (the remap draws ran on the red pipeline)
WW_VKCSM_REMAP_RED=1 "$OUT/shadowview.exe" "$ROOT" "${2:-1024}" > "$OUT/shadowview_remapred.txt" 2>&1
fails=$(grep -cE "^SHADOWVIEW remap frame1 [a-z]+/[0-9] FAIL \([0-9]+ of [0-9]+ covered texels differ, [1-9][0-9]* past 1 unit" "$OUT/shadowview_remapred.txt")
trig=$(grep -cE "^SHADOWVIEW remap stat: .*remapRed=[1-9]" "$OUT/shadowview_remapred.txt")
if [ "$fails" -gt 0 ] && [ "$trig" -gt 0 ]; then
	echo "SHADOWVIEW RED FAIL remap always ($fails frame1 layers past 1 unit, trigger lines $trig; must FAIL)"
else
	echo "SHADOWVIEW RED TESTS NOTHING remap always ($fails frame1 layers past 1 unit, trigger lines $trig)"
	echo "SHADOWVIEW GATE FAILED"
	exit 1
fi
echo "SHADOWVIEW GATE OK"
