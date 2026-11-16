#!/bin/bash
# Lane VKFLIGHT5 (2026-11-16): gate of the Vulkan view's opaque wiring. Usage: bash tests/vkframe/opaque.sh <outdir>
# Needs MSYS2 UCRT64 g++ on PATH. Green: OPAQUEVIEW OK + OPAQUESITE OK; reds: the old switch rule, a dropped hook.
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
OUT="${1:-/x/ClaudeOverflow/opaque}"
mkdir -p "$OUT"
rc=0
g++ -std=c++17 -O1 -I"$ROOT/src" -o "$OUT/opaqueview.exe" "$ROOT/tests/vkframe/opaqueview.cpp" || { echo "OPAQUEVIEW build FAILED"; exit 2; }
"$OUT/opaqueview.exe" || rc=1
"$OUT/opaqueview.exe" red > "$OUT/red.txt"; r=$?
grep -E 'FAIL|OPAQUEVIEW' "$OUT/red.txt"
if [ $r -eq 1 ]; then echo "red old-rule FAILS as it must (rc 1)"; else echo "red old-rule did NOT fail (rc $r)"; rc=1; fi
python -I "$ROOT/tests/vkframe/opaquesite.py" "$ROOT" || rc=1
python -I "$ROOT/tests/vkframe/opaquesite.py" "$ROOT" red > "$OUT/sitered.txt"; r=$?
grep -E 'FAIL|OPAQUESITE' "$OUT/sitered.txt"
if [ $r -eq 1 ]; then echo "red dropped-hook FAILS as it must (rc 1)"; else echo "red dropped-hook did NOT fail (rc $r)"; rc=1; fi
echo "END rc=$rc"
exit $rc
