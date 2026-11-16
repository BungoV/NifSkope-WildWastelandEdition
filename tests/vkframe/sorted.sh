#!/bin/bash
# Lane VKFLIGHT6 (2026-11-16): gate of the sorted pass on Vulkan. Usage: bash tests/vkframe/sorted.sh <outdir>
# Needs MSYS2 UCRT64 g++ on PATH. Green: SORTEDVIEW OK + SORTEDSITE OK; reds: the stale refusal, the GL_BLEND refusal back.
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
OUT="${1:-/x/ClaudeOverflow/sorted}"
mkdir -p "$OUT"
rc=0
g++ -std=c++17 -O1 -I"$ROOT/src" -o "$OUT/sortedview.exe" "$ROOT/tests/vkframe/sortedview.cpp" || { echo "SORTEDVIEW build FAILED"; exit 2; }
"$OUT/sortedview.exe" || rc=1
"$OUT/sortedview.exe" red > "$OUT/red.txt"; r=$?
grep -E 'FAIL|SORTEDVIEW' "$OUT/red.txt"
if [ $r -eq 1 ]; then echo "red stale-why FAILS as it must (rc 1)"; else echo "red stale-why did NOT fail (rc $r)"; rc=1; fi
python -I "$ROOT/tests/vkframe/sortedsite.py" "$ROOT" || rc=1
python -I "$ROOT/tests/vkframe/sortedsite.py" "$ROOT" red > "$OUT/sitered.txt"; r=$?
grep -E 'FAIL|SORTEDSITE' "$OUT/sitered.txt"
if [ $r -eq 1 ]; then echo "red blend-refusal FAILS as it must (rc 1)"; else echo "red blend-refusal did NOT fail (rc $r)"; rc=1; fi
echo "END rc=$rc"
exit $rc
