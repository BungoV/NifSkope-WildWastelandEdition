#!/bin/bash
# Lane VKWATER4 (2026-11-15, plan row G5): the ultra ocean's build through the shipped lightRun (pass LightOcean,
# ocean_vk.hlsl) against GL's own ocean_spectrum / ocean_fft / ocean_final / ocean_mip programs (tests/vkframe/oceanview.cpp
# has the bars, floors and reds). bash tests/vkframe/oceanview.sh <out dir>. Needs MSYS2 UCRT64 g++ and python.
# Not while NifSkope or the game runs (a window of ours): the script refuses then.
# Last line: `OCEANVIEW GATE OK` or `OCEANVIEW GATE FAILED`.
set -u
OUT="${1:?out dir}"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
PY="${PY:-$(command -v python)}"	# before the UCRT64 PATH
export PATH="/c/msys64/ucrt64/bin:$PATH"
if powershell -NoProfile -Command "if (Get-Process NifSkope,Fallout4 -ErrorAction SilentlyContinue) { 'busy' }" | grep -q busy; then echo "OCEANVIEW GATE FAILED (NifSkope or the game runs: no window of ours now)"; exit 1; fi
mkdir -p "$OUT"
for p in ocean_spectrum ocean_fft ocean_final ocean_mip; do
	"$PY" -I "$ROOT/tests/vkframe/waterdraw_prep.py" "$OUT/$p" "$p" || { echo "OCEANVIEW GATE FAILED (prep $p)"; exit 1; }
done
g++ -O2 -std=c++17 -pthread -I"$ROOT/src" "$ROOT/tests/vkframe/oceanview.cpp" -o "$OUT/oceanview.exe" -lopengl32 -lgdi32 -luser32 \
	|| { echo "OCEANVIEW GATE FAILED (g++)"; exit 1; }
"$OUT/oceanview.exe" "$OUT" 2>&1 | tee "$OUT/oceanview.txt"
tail -1 "$OUT/oceanview.txt" | grep -q "OCEANVIEW GATE OK"
