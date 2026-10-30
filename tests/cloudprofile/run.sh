#!/bin/bash
# lane CLOUD6d: one cloud profile set per weather (WW_CLOUDVOL_PROFILESET, the default), offline gates (no NifSkope).
# Green must exit 0, each red must exit 1.
#   bash tests/cloudprofile/run.sh <scratch dir>     (from the tree root; needs the GPU driver, MSYS2 UCRT64 g++ and a
#   python WITH numpy: PY=<python> overrides; code1030/unit.sh's MSYS python has none)
set -u
S=${1:-../cloudprofile_scratch}
PY="${PY:-python}"
mkdir -p "$S/base"
fail=0
unset WW_CLOUDVOL_PROFILESET WW_CLOUDVOL_RED
"$PY" -I tests/spells/cloudprofile_check.py "$S/green" --only PCVWD < /dev/null > "$S/green.log" 2>&1
rc=$?; echo "green rc=$rc (want 0)"; [ $rc -eq 0 ] || fail=1
"$PY" -I tests/spells/cloudprofile_check.py "$S/typemix" --red typemix --only P < /dev/null > "$S/red_typemix.log" 2>&1
rc=$?; echo "red typemix rc=$rc (want 1)"; [ $rc -eq 1 ] || fail=1
WW_CLOUDVOL_PROFILESET=0 "$PY" -I tests/spells/cloudprofile_check.py "$S/off" --only P < /dev/null > "$S/red_off.log" 2>&1
rc=$?; echo "red WW_CLOUDVOL_PROFILESET=0 rc=$rc (want 1)"; [ $rc -eq 1 ] || fail=1
# the wiring: the base tree (before CLOUD6d) has none
git show 0e7c2c3a:src/gl/cloudmap.cpp > "$S/base/cloudmap.cpp" 2>/dev/null && git show 0e7c2c3a:src/gl/cloudvol.cpp > "$S/base/cloudvol.cpp" 2>/dev/null && {
	"$PY" -I tests/spells/cloudprofile_check.py "$S/w" --only W --src "$S/base" < /dev/null > "$S/red_wiring.log" 2>&1
	rc=$?; echo "red wiring (base 0e7c2c3a) rc=$rc (want 1)"; [ $rc -eq 1 ] || fail=1
}
"$PY" -I tests/cloudband/stage_check.py src/wwswitchstages.inc WW_CLOUDVOL_PROFILESET
rc=$?; echo "stage rc=$rc (want 0)"; [ $rc -eq 0 ] || fail=1
git show 0e7c2c3a:src/wwswitchstages.inc > "$S/base/stages.inc" 2>/dev/null && {
	"$PY" -I tests/cloudband/stage_check.py "$S/base/stages.inc" WW_CLOUDVOL_PROFILESET
	rc=$?; echo "stage red (base table) rc=$rc (want 1)"; [ $rc -eq 1 ] || fail=1
}
echo "CLOUDPROFILE-RUN $([ $fail -eq 0 ] && echo PASS || echo FAIL)"
exit $fail
