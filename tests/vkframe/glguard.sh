#!/bin/bash
# Lane VKFLIGHT4 (2026-11-16): gate of the Vulkan view's GL guard. Usage: bash tests/vkframe/glguard.sh <outdir>
# Needs MSYS2 UCRT64 g++ on PATH. Green: glguard OK + site OK; reds: one zone (must FAIL "2 passes"), a dropped hook.
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
OUT="${1:-/x/ClaudeOverflow/glguard}"
mkdir -p "$OUT"
rc=0
g++ -std=c++17 -O1 -pthread -o "$OUT/glguard.exe" "$ROOT/tests/vkframe/glguard.cpp" || { echo "GLGUARD build FAILED"; exit 2; }
"$OUT/glguard.exe" || rc=1
"$OUT/glguard.exe" red > "$OUT/red.txt"; r=$?
grep -E 'FAIL|GLGUARD' "$OUT/red.txt"
if [ $r -eq 1 ]; then echo "red one-zone FAILS as it must (rc 1)"; else echo "red one-zone did NOT fail (rc $r)"; rc=1; fi
python -I "$ROOT/tests/vkframe/glguardsite.py" "$ROOT" || rc=1
python -I "$ROOT/tests/vkframe/glguardsite.py" "$ROOT" red > "$OUT/sitered.txt"; r=$?
grep -E 'FAIL|GLGUARDSITE' "$OUT/sitered.txt"
if [ $r -eq 1 ]; then echo "red dropped-hook FAILS as it must (rc 1)"; else echo "red dropped-hook did NOT fail (rc $r)"; rc=1; fi
echo "END rc=$rc"
exit $rc
