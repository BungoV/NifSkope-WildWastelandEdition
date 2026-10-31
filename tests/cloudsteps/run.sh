#!/bin/bash
# lane CLOUDSTEPS1: Medium 60 / High 80 march steps at main's read cost (WW_CLOUDVOL_MORESTEPS, the default, with the
# CLOUDLIGHT1 column), offline gates (no NifSkope). Green must exit 0, each red must exit 1.
#   bash tests/cloudsteps/run.sh <scratch dir>     (from the tree root; GPU driver, MSYS2 UCRT64 g++, a python WITH numpy:
#   PY=<python> overrides)
set -u
S=${1:-../cloudsteps_scratch}
PY="${PY:-$(command -v python)}"
mkdir -p "$S/base"
fail=0
unset WW_CLOUDVOL_MORESTEPS WW_CLOUDVOL_TOPCOLUMN WW_CLOUDVOL_RED
"$PY" -I tests/spells/cloudsteps_check.py "$S/green" < /dev/null > "$S/green.log" 2>&1
rc=$?; echo "green rc=$rc (want 0)"; [ $rc -eq 0 ] || fail=1
"$PY" -I tests/spells/cloudsteps_check.py "$S/old" --red oldsteps --only E < /dev/null > "$S/red_oldsteps.log" 2>&1
rc=$?; echo "red oldsteps rc=$rc (want 1)"; [ $rc -eq 1 ] || fail=1
git show 563dcf97:src/gl/cloudvol.cpp > "$S/base/cloudvol.cpp" 2>/dev/null && {
	"$PY" -I tests/spells/cloudsteps_check.py "$S/w" --only W --src "$S/base" < /dev/null > "$S/red_wiring.log" 2>&1
	rc=$?; echo "red wiring (base 563dcf97) rc=$rc (want 1)"; [ $rc -eq 1 ] || fail=1
}
"$PY" -I tests/cloudband/stage_check.py src/wwswitchstages.inc WW_CLOUDVOL_MORESTEPS
rc=$?; echo "stage rc=$rc (want 0)"; [ $rc -eq 0 ] || fail=1
git show 563dcf97:src/wwswitchstages.inc > "$S/base/stages.inc" 2>/dev/null && {
	"$PY" -I tests/cloudband/stage_check.py "$S/base/stages.inc" WW_CLOUDVOL_MORESTEPS
	rc=$?; echo "stage red (base table) rc=$rc (want 1)"; [ $rc -eq 1 ] || fail=1
}
echo "CLOUDSTEPS-RUN $([ $fail -eq 0 ] && echo PASS || echo FAIL)"
exit $fail
