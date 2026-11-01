#!/bin/bash
# lane PERFFIX2 b (item 8): the sun-cascades zone's sub-timers (check.py) + its red (a doctored table: must FAIL).
#   bash tests/suntimersplit/run.sh
HERE="$(cd "$(dirname "$0")" && pwd)"; ROOT="$(cd "$HERE/../.." && pwd)"
PY="${PY:-$(command -v python)}"
rc=0
"$PY" -I "$HERE/check.py" "$ROOT" < /dev/null | tail -n 1 | sed 's/^/green: /'
[ "${PIPESTATUS[0]}" = 0 ] || rc=1
"$PY" -I "$HERE/check.py" "$ROOT" --red < /dev/null > /dev/null && { echo "red: PASSED (it must fail)"; rc=1; } || echo "red: FAIL (as it must)"
echo "SUNTIMERSPLIT UNIT RUN $([ $rc = 0 ] && echo PASS || echo FAIL)"
exit $rc
