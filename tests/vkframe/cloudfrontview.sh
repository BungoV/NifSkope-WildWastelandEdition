#!/bin/bash
# Lane VKFAR2 (2026-11-14, row P8): the clouds in front of the scene on the present device (vkcloudfront.inc cloudFrontRun,
# cloud_composite cvFront 2) against GL's own cloud_composite program in cloudvol.cpp frontPass's state, per sample, on the
# same shared 4x MSAA tone frame; 4 seeded jobs + 3 reds.
# bash tests/vkframe/cloudfrontview.sh <out dir>. Needs MSYS2 UCRT64 g++ and python (the job).
# Not while NifSkope or the game runs (a window of ours): the script refuses then.
# Last line: `CLOUDFRONTVIEW GATE OK` or `CLOUDFRONTVIEW GATE FAILED`.
set -u
OUT="${1:?out dir}"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
PY="${PY:-$(command -v python)}"	# before the UCRT64 PATH
export PATH="/c/msys64/ucrt64/bin:$PATH"
if powershell -NoProfile -Command "if (Get-Process NifSkope,Fallout4 -ErrorAction SilentlyContinue) { 'busy' }" | grep -q busy; then echo "CLOUDFRONTVIEW GATE FAILED (NifSkope or the game runs: no window of ours now)"; exit 1; fi
mkdir -p "$OUT"
"$PY" -I "$ROOT/tests/vkframe/cloudfrontview_prep.py" "$OUT" || { echo "CLOUDFRONTVIEW GATE FAILED (job)"; exit 1; }
g++ -O2 -std=c++17 -pthread -I"$ROOT/src" "$ROOT/tests/vkframe/cloudfrontview.cpp" -o "$OUT/cloudfrontview.exe" -lopengl32 -lgdi32 -luser32 \
	|| { echo "CLOUDFRONTVIEW GATE FAILED (g++)"; exit 1; }
"$OUT/cloudfrontview.exe" "$OUT" 2>&1 | tee "$OUT/cloudfrontview.txt"
tail -1 "$OUT/cloudfrontview.txt" | grep -q "CLOUDFRONTVIEW GATE OK"
