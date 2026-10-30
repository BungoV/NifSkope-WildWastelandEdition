#!/bin/bash
# lane CLOUD6c: the REFUSAL test of edge refinement in the volume clouds' march (RDR2's step refinement, Bauer 2019 slide
# 50). No NifSkope; the real driver through tests/spells/cloudedge_check.py on a scratch copy of res/shaders with
# tests/cloudedge/shaders (the refinement, test only) laid over it.
#   green: --refusal must exit 0 = the refinement helps at its own step count (X, I, L), its off arm is today's shipping
#          march pixel for pixel (R), and the uniform march wins at equal density reads in every tier/view (Q): the
#          reason nothing ships. A lane that makes refinement win at equal cost must change this test on purpose.
#   red:   --red off (the candidates = the old march) must exit 1 (X fails: the edge measure separates).
#   bash tests/cloudedge/run.sh <scratch dir>     (from the tree root; GPU driver, MSYS2 UCRT64 g++ for the helpers'
#   dumps, and a python WITH numpy: PY=<python> overrides, as tests/cloudband/run.sh)
set -u
S=${1:-../cloudedge_scratch}
PY="${PY:-python}"
mkdir -p "$S"
fail=0
"$PY" -I tests/spells/cloudedge_check.py "$S/green" --refusal < /dev/null > "$S/green.log" 2>&1
rc=$?; echo "refusal green rc=$rc (want 0)"; [ $rc -eq 0 ] || fail=1
"$PY" -I tests/spells/cloudedge_check.py "$S/red" --refusal --red off --only XQ < /dev/null > "$S/red_off.log" 2>&1
rc=$?; echo "red off rc=$rc (want 1)"; [ $rc -eq 1 ] || fail=1
grep -h "CLOUDEDGE-CHECK" "$S/green.log" "$S/red_off.log"
echo "CLOUDEDGE-RUN $([ $fail -eq 0 ] && echo PASS || echo FAIL)"
exit $fail
