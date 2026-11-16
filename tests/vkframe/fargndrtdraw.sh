#!/bin/bash
# Lane VKGROUND10: the far ring 0's live ground (fo4_cellcsmfgl, shader decals) under the RT GI keys (RT and DG twins) against
# GL's HLSL-built program of the same key, per sample, colour + depth; reds, the all-on arm and the half-rate arm (header of
# fargndrtdraw.cpp). bash tests/vkframe/fargndrtdraw.sh <out dir>. Set FARGNDRTDRAW_SPVSET to reuse a build_all set.
# Needs MSYS2 UCRT64 g++ and python. Not while NifSkope or the game runs. Last line: `FARGNDRTDRAW GATE OK` or `... GATE FAILED`.
set -u
OUT="${1:?out dir}"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
PY="${PY:-$(command -v python)}"	# before the UCRT64 PATH
if powershell -NoProfile -Command "if (Get-Process NifSkope,Fallout4 -ErrorAction SilentlyContinue) { 'busy' }" | grep -q busy; then echo "FARGNDRTDRAW GATE FAILED (NifSkope or the game runs: no window of ours now)"; exit 1; fi
mkdir -p "$OUT"
"$PY" -I "$ROOT/tests/vkframe/fargndrtdraw_prep.py" "$OUT" ${FARGNDRTDRAW_SPVSET:-} || { echo "FARGNDRTDRAW GATE FAILED (job)"; exit 1; }
PATH=/c/msys64/ucrt64/bin:$PATH g++ -O2 -std=c++17 -pthread -I"$ROOT/src" "$ROOT/tests/vkframe/fargndrtdraw.cpp" -o "$OUT/fargndrtdraw.exe" \
	-lopengl32 -lgdi32 -luser32 || { echo "FARGNDRTDRAW GATE FAILED (g++)"; exit 1; }
ok=0
for k in rt dg; do
	PATH=/c/msys64/ucrt64/bin:$PATH "$OUT/fargndrtdraw.exe" "$OUT" "$k" > "$OUT/fargndrtdraw_$k.txt" 2>&1
	tail -1 "$OUT/fargndrtdraw_$k.txt"
	tail -1 "$OUT/fargndrtdraw_$k.txt" | grep -q "FARGNDRTDRAW GATE OK" && ok=$((ok + 1))
done
if [ "$ok" = 2 ]; then echo "FARGNDRTDRAW GATE OK"; else echo "FARGNDRTDRAW GATE FAILED ($ok of 2 keys OK)"; exit 1; fi
