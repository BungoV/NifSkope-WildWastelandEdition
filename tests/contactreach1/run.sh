#!/bin/bash
# lane FIX1101A (LATE ITEM 14): the near contact march's reach inside the cascades (src/gl/shadowset.cpp kContactReach,
# res/shaders/ww_contact.frag sssReach). green: a wire 85 u off a wall draws the cascades' smooth line (stair < 1 px,
# darkness CV < 0.1) and a wire 6 u off keeps its contact shadow; red (today's whole march) must FAIL the far gate.
#   bash tests/contactreach1/run.sh <out dir>
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"; ROOT="$(cd "$HERE/../.." && pwd)"
O="${1:?out dir}"; mkdir -p "$O"
PY="${PY:-$(command -v python)}"
rc=0
for v in far near; do
	timeout 300 "$PY" -I "$HERE/contactreach_twin.py" "$O/cr1_$v" --root "$ROOT" --gate $v > "$O/cr1_$v.log" 2>&1; r=$?
	grep -E '^(composed|contact|FAIL|CONTACTREACH1)' "$O/cr1_$v.log" | sed "s/^/green $v: /"
	[ $r = 0 ] || rc=1
done
timeout 300 "$PY" -I "$HERE/contactreach_twin.py" "$O/cr1_red" --root "$ROOT" --gate far --red > "$O/cr1_red.log" 2>&1; r=$?
grep -E '^(composed|FAIL|CONTACTREACH1)' "$O/cr1_red.log" | sed "s/^/red far: /"
[ $r != 0 ] || { echo "red: the red PASSED (it must fail)"; rc=1; }
echo "CONTACTREACH1 UNIT RUN $([ $rc = 0 ] && echo PASS || echo FAIL)"
exit $rc
