#!/bin/bash
# The far card pick unit, CPU vs GPU (lane FARCARDPICK1, 2026-11-02): builds the CPU oracle from the shipped
# sources, then runs the synthetic sweep, its two reds (which must FAIL) and, when given, a WW_FARCARD_PICKDUMP.
#   bash tests/farcardpick/farcardpick.sh <work dir> [dump file]
# Exit 0 = green PASS, both reds FAIL (and the dump PASS when given).
set -u
T="$(cd "$(dirname "$0")/../.." && pwd)"
W="$1"; DUMP="${2:-}"
mkdir -p "$W"
export PATH="/c/msys64/ucrt64/bin:$PATH"
g++ -std=c++20 -O3 -march=haswell -I"$T/src" -I"$T/src/gl" "$T/tests/farcardpick/pick_oracle.cpp" \
    "$T/src/impostoroct.cpp" -o "$W/pick_oracle.exe" || { echo "ORACLE BUILD FAIL"; exit 2; }
PY="python -I $T/tests/farcardpick/farcardpick.py $T $W/pick_oracle.exe"
rc=0
echo "== green (synthetic)"; timeout 300 $PY "$W/green" < /dev/null || rc=1
for red in shuffle convention; do
    echo "== red $red (must FAIL)"
    timeout 300 $PY "$W/red_$red" --red $red < /dev/null > "$W/red_$red.txt"; r=$?
    tail -3 "$W/red_$red.txt"
    [ $r -eq 1 ] || { echo "RED $red did not fail (rc $r)"; rc=1; }
done
if [ -n "$DUMP" ]; then
    echo "== dump $DUMP"; timeout 300 $PY "$W/dump" --rows "$DUMP" < /dev/null || rc=1
fi
echo "UNIT RC $rc"
exit $rc
