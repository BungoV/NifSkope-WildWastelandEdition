#!/bin/bash
# Lane VKFAR14 (row G3 leftover): the live far ground on streamed opens. bash fgstream.sh <out dir>
# Runs the default, then the reds (each must FAIL), then checks the app's call sites use the shared rule.
# Last line: `FGSTREAM ALL OK` or `FGSTREAM ALL FAILED`.
set -u
OUT="${1:?out dir}"
ROOT="$(cd "$(dirname "$0")/../../../.." && pwd)"
export PATH="/c/msys64/ucrt64/bin:$PATH"
mkdir -p "$OUT"
g++ -O1 -std=c++17 -I"$ROOT/src" "$ROOT/tests/features/far-ground/fgstream/fgstream.cpp" -o "$OUT/fgstream.exe" \
	|| { echo "FGSTREAM ALL FAILED (g++)"; exit 1; }
bad=0
"$OUT/fgstream.exe" > "$OUT/default.txt" 2>&1; cat "$OUT/default.txt"
tail -1 "$OUT/default.txt" | grep -q "FGSTREAM GATE OK" || bad=1
for r in "WW_CELL_FARGROUND_RED=nostream" "FG_GATE_RED=keepcut"; do
	env "$r" "$OUT/fgstream.exe" > "$OUT/red_${r%%=*}.txt" 2>&1
	if tail -1 "$OUT/red_${r%%=*}.txt" | grep -q "FGSTREAM GATE FAILED"; then
		echo "red $r: FAILED as it must ($(grep -c '^FAIL' "$OUT/red_${r%%=*}.txt") checks)"
	else
		echo "red $r: TESTS NOTHING"; bad=1
	fi
done
# the app's sites: cellfarlod.cpp asks the shared rule and hands the stream to prepare; cellfarground.cpp cuts only by Cut
F="$ROOT/src/features/far-lod/cellfarlod.cpp"; G="$ROOT/src/features/far-ground/cellfarground.cpp"
n1=$(grep -c "CellFarGround::streamPrepares( stream != nullptr" "$F")
n2=$(grep -c "r.cx0, r.cy0, r.cx1, r.cy1, stream != nullptr, &fg" "$F")
n3=$(grep -c "wx >= cx0 && wx <= cx1\|c.wx >= d.cutX0" "$G")
n4=$(grep -c "\.has( " "$G")
echo "sites: streamPrepares $n1 (1), prepare uncut arg $n2 (1), old literal cuts $n3 (0), Cut.has calls $n4 (3)"
[ "$n1" = 1 ] && [ "$n2" = 1 ] && [ "$n3" = 0 ] && [ "$n4" = 3 ] || bad=1
[ $bad = 0 ] && echo "FGSTREAM ALL OK" || echo "FGSTREAM ALL FAILED"
