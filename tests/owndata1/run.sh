#!/bin/bash
# lane FIX1101A (LATE ITEM 28b), moved by lane WEATHERRECX1: our moon maps ship in the MO2 mod "Fusion Shaders - Assets"
# (src/gamemanager.cpp init_archives after NifSkope's own Data, src/recx.h wwAssetsModRoot). green: the mod's maps;
# red: an empty folder must FAIL. The live proof is the cell view's sky notes: "moon:globe" with the mod found,
# "moon:refused(...)" with WW_ASSETS_MOD pointing at an empty folder and no mod folder in the resource list.
#   bash tests/owndata1/run.sh <out dir> [assets mod dir]
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
O="${1:?out dir}"; mkdir -p "$O"; MOD="${2:-E:/Projects/Fallout 4 Mods/mods/Fusion Shaders - Assets}"
PY="${PY:-$(command -v python)}"
rc=0
"$PY" -I "$HERE/check.py" "$MOD" > "$O/owndata1.log" 2>&1 || rc=1
sed 's/^/green: /' "$O/owndata1.log"
mkdir -p "$O/red_mod"
"$PY" -I "$HERE/check.py" "$O/red_mod" > "$O/owndata1_red.log" 2>&1 && { echo "red: the red PASSED (it must fail)"; rc=1; }
grep -E '^(FAIL|OWNDATA1)' "$O/owndata1_red.log" | sed 's/^/red: /'
echo "OWNDATA1 UNIT RUN $([ $rc = 0 ] && echo PASS || echo FAIL)"
exit $rc
