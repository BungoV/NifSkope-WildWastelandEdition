#!/usr/bin/env bash
# seed_bisect.sh <drop-prefix>... -- run native_lighting.sh under a COPY of bungo's profile
# (never his own key) minus GLView/Display/Contributions minus the given prefixes, and print
# one verdict line: TERRAIN-OK when gates (b)(d)(e)(f) all pass, else TERRAIN-RED.
W=/e/Projects/NifskopeWWE-gatefix2
G=$W/scratchpad/gatefix2_20260925
tag=$(echo "$*" | tr ' /' '__' | cut -c1-80)
python $G/prof_scope.py ${SRC:-$G/prof_export.reg} $G/seed_bis.reg nativelighting GLView/Display/Contributions "$@" > /dev/null
cd $W && SEED_REG=$G/seed_bis.reg PORT=43611 bash tests/spells/native_lighting.sh > $G/bis_$tag.log 2>&1
if grep -qE "FAIL gate \((b|d|e|f)\)" $G/bis_$tag.log; then v=TERRAIN-RED; else v=TERRAIN-OK; fi
echo "$v drop: $* ($(grep -oE '^[0-9]+ checks, [0-9]+ failures' $G/bis_$tag.log))"
