#!/bin/bash
# lane CLOUDLIGHT1: the top light without reads (WW_CLOUDVOL_TOPCOLUMN, the default), offline gates (no NifSkope).
# Green must exit 0, each red must exit 1.
#   bash tests/cloudlight/run.sh <scratch dir>     (from the tree root; GPU driver, MSYS2 UCRT64 g++, a python WITH numpy:
#   PY=<python> overrides)
set -u
S=${1:-../cloudlight_scratch}
PY="${PY:-$(command -v python)}"
mkdir -p "$S/base"
fail=0
unset WW_CLOUDVOL_TOPCOLUMN WW_CLOUDVOL_RED
"$PY" -I tests/spells/cloudlight_check.py "$S/green" < /dev/null > "$S/green.log" 2>&1
rc=$?; echo "green rc=$rc (want 0)"; [ $rc -eq 0 ] || fail=1
"$PY" -I tests/spells/cloudlight_check.py "$S/taps" --red toptaps --only N < /dev/null > "$S/red_toptaps.log" 2>&1
rc=$?; echo "red toptaps rc=$rc (want 1)"; [ $rc -eq 1 ] || fail=1
# the Monte Carlo gates on the default (the column): dark core, top, flicker; and the red notop still fails the top
"$PY" -I tests/spells/cloudvol_check.py "$S/mc" --only C,R,D < /dev/null > "$S/mc.log" 2>&1
rc=$?; echo "Monte Carlo C R D rc=$rc (want 0)"; [ $rc -eq 0 ] || fail=1
"$PY" -I tests/spells/cloudvol_check.py "$S/mcred" --red notop --only R < /dev/null > "$S/mc_red_notop.log" 2>&1
rc=$?; echo "Monte Carlo red notop rc=$rc (want 1)"; [ $rc -eq 1 ] || fail=1
"$PY" -I tests/cloudlight/wiring_check.py src/gl/cloudvol.cpp res/shaders/cloud_march_step.glsl tests/spells/cloudvol_check.py < /dev/null > "$S/wiring.log" 2>&1
rc=$?; echo "wiring rc=$rc (want 0)"; [ $rc -eq 0 ] || fail=1
git show 563dcf97:src/gl/cloudvol.cpp > "$S/base/cloudvol.cpp" 2>/dev/null && git show 563dcf97:res/shaders/cloud_march_step.glsl > "$S/base/step.glsl" 2>/dev/null && git show 563dcf97:tests/spells/cloudvol_check.py > "$S/base/cloudvol_check.py" 2>/dev/null && {
	"$PY" -I tests/cloudlight/wiring_check.py "$S/base/cloudvol.cpp" "$S/base/step.glsl" "$S/base/cloudvol_check.py" < /dev/null > "$S/wiring_red.log" 2>&1
	rc=$?; echo "wiring red (base 563dcf97) rc=$rc (want 1)"; [ $rc -eq 1 ] || fail=1
}
"$PY" -I tests/cloudband/stage_check.py src/wwswitchstages.inc WW_CLOUDVOL_TOPCOLUMN
rc=$?; echo "stage rc=$rc (want 0)"; [ $rc -eq 0 ] || fail=1
git show 563dcf97:src/wwswitchstages.inc > "$S/base/stages.inc" 2>/dev/null && {
	"$PY" -I tests/cloudband/stage_check.py "$S/base/stages.inc" WW_CLOUDVOL_TOPCOLUMN
	rc=$?; echo "stage red (base table) rc=$rc (want 1)"; [ $rc -eq 1 ] || fail=1
}
echo "CLOUDLIGHT-RUN $([ $fail -eq 0 ] && echo PASS || echo FAIL)"
exit $fail
