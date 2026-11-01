#!/bin/bash
# lane FIX1101A (LATE ITEM 28b): NifSkope ships our moon maps as its own lowest-priority Fallout 4 data
# (src/gamemanager.cpp init_archives, res/Data/Textures/Sky/Moon). green: the shipped maps + the release copies;
# red: a release dir with no own data (today's layout) must FAIL. The live proof is the cell view's sky notes:
# "moon:globe" with the build, "moon:refused(...)" under WW_OWNDATA_RED=1 with no mod folder in the resource list.
#   bash tests/owndata1/run.sh <out dir> [release dir]
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"; ROOT="$(cd "$HERE/../.." && pwd)"
O="${1:?out dir}"; mkdir -p "$O"; REL="${2:-$ROOT/release}"
PY="${PY:-$(command -v python)}"
rc=0
"$PY" -I "$HERE/check.py" "$ROOT" "$REL" > "$O/owndata1.log" 2>&1 || rc=1
sed 's/^/green: /' "$O/owndata1.log"
mkdir -p "$O/red_release"
"$PY" -I "$HERE/check.py" "$ROOT" "$O/red_release" > "$O/owndata1_red.log" 2>&1 && { echo "red: the red PASSED (it must fail)"; rc=1; }
grep -E '^(FAIL|OWNDATA1)' "$O/owndata1_red.log" | sed 's/^/red: /'
echo "OWNDATA1 UNIT RUN $([ $rc = 0 ] && echo PASS || echo FAIL)"
exit $rc
