#!/bin/bash
# Lane VKFAR2 (2026-11-14, the froxel item; plan rows L5, P4b, L8): the air's integrate on the present device (vkfog.inc
# fogIntegRun, volair_integ_cs.hlsl) against GL's own cell_volair stage 3 / stage 2 under both switch sets, 5 volumes
# (fogview.cpp), then the independent numpy rebuild of every dumped volume from its dumped inject (fogview_check.py), then
# the temporal history twin (tests/features/air/airlampinj/run.sh). bash tests/vkframe/fogview.sh <out dir>.
# Needs MSYS2 UCRT64 g++ and python with numpy. Not while NifSkope or the game runs (a window of ours): the script refuses then.
# Last line: `FOGVIEW GATE OK` or `FOGVIEW GATE FAILED`.
set -u
OUT="${1:?out dir}"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
PY="${PY:-$(command -v python)}"	# before the UCRT64 PATH
export PATH="/c/msys64/ucrt64/bin:$PATH"
if powershell -NoProfile -Command "if (Get-Process NifSkope,Fallout4 -ErrorAction SilentlyContinue) { 'busy' }" | grep -q busy; then echo "FOGVIEW GATE FAILED (NifSkope or the game runs: no window of ours now)"; exit 1; fi
mkdir -p "$OUT"
"$PY" -I "$ROOT/tests/vkframe/fogview_prep.py" "$OUT" || { echo "FOGVIEW GATE FAILED (prep)"; exit 1; }
g++ -O2 -std=c++17 -pthread -I"$ROOT/src" "$ROOT/tests/vkframe/fogview.cpp" -o "$OUT/fogview.exe" -lopengl32 -lgdi32 -luser32 \
	|| { echo "FOGVIEW GATE FAILED (g++)"; exit 1; }
"$OUT/fogview.exe" "$OUT" 2>&1 | tee "$OUT/fogview.txt"
tail -1 "$OUT/fogview.txt" | grep -q "FOGVIEW GATE OK" || { echo "FOGVIEW GATE FAILED (the GL / Vulkan match)"; exit 1; }
"$PY" -I "$ROOT/tests/vkframe/fogview_check.py" "$OUT" 2>&1 | tee "$OUT/fogview_check.txt"
tail -1 "$OUT/fogview_check.txt" | grep -q "FOGVIEW CHECK OK" || { echo "FOGVIEW GATE FAILED (the rebuild)"; exit 1; }
( cd "$ROOT" && bash tests/features/air/airlampinj/run.sh "$OUT/airlampinj" ) 2>&1 | tee "$OUT/airlampinj.txt"
grep -q "AIRLAMPINJ GATE OK" "$OUT/airlampinj.txt" || { echo "FOGVIEW GATE FAILED (the temporal history twin)"; exit 1; }
echo "FOGVIEW GATE OK"
