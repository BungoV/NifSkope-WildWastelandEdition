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
# Lane VKGROUND17: mode 2 (fo4_cellcsmfgp, cached pages) too: fgpagesrtdraw.cpp (generated from fgpagesdraw.cpp) in <out dir>/fgp,
# FGPAGESRTDRAW_SPVSET reuses its build_all set; FARGNDRTDRAW_MODES="1 2" (default) picks the modes. OK only when every key of
# every mode is OK (4 of 4).
mkdir -p "$OUT"
MODES="${FARGNDRTDRAW_MODES:-1 2}"
ok=0
want=0
for m in $MODES; do
	if [ "$m" = 1 ]; then name=fargndrtdraw; dir="$OUT"; set_="${FARGNDRTDRAW_SPVSET:-}"; word="FARGNDRTDRAW GATE OK"
	else name=fgpagesrtdraw; dir="$OUT/fgp"; set_="${FGPAGESRTDRAW_SPVSET:-}"; word="FARGNDRTDRAW2 GATE OK"; fi
	mkdir -p "$dir"
	"$PY" -I "$ROOT/tests/vkframe/${name}_prep.py" "$dir" $set_ || { echo "FARGNDRTDRAW GATE FAILED (job, mode $m)"; exit 1; }
	PATH=/c/msys64/ucrt64/bin:$PATH g++ -O2 -std=c++17 -pthread -I"$ROOT/src" "$ROOT/tests/vkframe/$name.cpp" -o "$dir/$name.exe" \
		-lopengl32 -lgdi32 -luser32 || { echo "FARGNDRTDRAW GATE FAILED (g++, mode $m)"; exit 1; }
	for k in rt dg; do
		want=$((want + 1))
		PATH=/c/msys64/ucrt64/bin:$PATH "$dir/$name.exe" "$dir" "$k" > "$dir/${name}_$k.txt" 2>&1
		tail -1 "$dir/${name}_$k.txt"
		tail -1 "$dir/${name}_$k.txt" | grep -q "$word" && ok=$((ok + 1))
	done
done
if [ "$ok" = "$want" ] && [ "$want" -gt 0 ]; then echo "FARGNDRTDRAW GATE OK ($ok of $want keys)"; else echo "FARGNDRTDRAW GATE FAILED ($ok of $want keys OK)"; exit 1; fi
