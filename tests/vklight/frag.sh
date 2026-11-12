#!/bin/bash
# Lane VKLIGHT1 round 2 (2026-11-12): converted fragment passes (HLSLBULK1's res/shaders/hlsl/<module>/*_ps*.hlsl) on a
# Vulkan graphics pipeline against the tree's GL .frag on the same inputs (tests/vklight/frag_gate.py + frag_jobs.py, a
# hidden GL 4.1 context; tests/vklight/fragmatch.cpp, a headless Vulkan device).
# bash tests/vklight/frag.sh <out dir> [<job> ...]          (jobs: frag_jobs.py names; default all)
# FRAG_TIME=1 also times both sides (only when nvidia-smi reads under 5 % for 20-30 s).
# Needs MSYS2 UCRT64 g++ (Vulkan headers), DXC (DXC= to override), spirv-cross and python with numpy.
# Green: every job EXACT or BOUND (its stated rule); each job's reds must FAIL. Last line `FRAG GATE OK` / `FAILED`.
set -u
OUT="${1:?out dir}"
shift
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
PY="${PY:-$(command -v python)}"	# before the UCRT64 PATH: its python has no numpy
export PATH="/c/msys64/ucrt64/bin:$PATH"
mkdir -p "$OUT"
W() { cygpath -m "$1" 2>/dev/null || echo "$1"; }
g++ -O1 -std=c++17 "$ROOT/tests/vklight/fragmatch.cpp" -o "$OUT/fragmatch.exe" || { echo "FRAG GATE FAILED (g++)"; exit 1; }
"$PY" -I "$ROOT/tests/vklight/frag_gate.py" "$(W "$OUT")" "$(W "$OUT/fragmatch.exe")" "$@" < /dev/null 2>&1
