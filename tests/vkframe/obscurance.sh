#!/bin/bash
# Lane VKFLIGHT6 (2026-11-16): gate of the obscurance pass's shared g-buffer. Usage: bash tests/vkframe/obscurance.sh <outdir>
# Green: AOGBUFSITE OK; red: the share call stripped (in memory) must FAIL.
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
OUT="${1:-/x/ClaudeOverflow/obscurance}"
PY="${PY:-$(command -v python)}"
mkdir -p "$OUT"
rc=0
"$PY" -I "$ROOT/tests/vkframe/aogbufsite.py" "$ROOT" || rc=1
"$PY" -I "$ROOT/tests/vkframe/aogbufsite.py" "$ROOT" red > "$OUT/red.txt"; r=$?
grep -E 'FAIL|AOGBUFSITE' "$OUT/red.txt"
if [ $r -eq 1 ]; then echo "red no-share FAILS as it must (rc 1)"; else echo "red no-share did NOT fail (rc $r)"; rc=1; fi
echo "END rc=$rc"
exit $rc
