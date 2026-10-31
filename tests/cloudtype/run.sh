#!/bin/bash
# lane CLOUDTYPE2: the cloud type's storm signal = the weather's thunder sounds (WW_CLOUDVOL_TYPERULE, the default), offline
# gates over the 107 vanilla weathers' class inputs (no GL, no NifSkope). Green must exit 0, each red must exit 1.
#   bash tests/cloudtype/run.sh <scratch dir>     (from the tree root; MSYS2 UCRT64 g++; PY=<python> overrides)
set -u
S=${1:-../cloudtype_scratch}
PY="${PY:-$(command -v python)}"
mkdir -p "$S/base"
fail=0
unset WW_CLOUDVOL_TYPERULE WW_CLOUDVOL_RED
"$PY" -I tests/spells/cloudtype_check.py "$S/green" < /dev/null > "$S/green.log" 2>&1
rc=$?; echo "green rc=$rc (want 0)"; [ $rc -eq 0 ] || fail=1
"$PY" -I tests/spells/cloudtype_check.py "$S/byte" --red thunderbyte --only ARN < /dev/null > "$S/red_thunderbyte.log" 2>&1
rc=$?; echo "red thunderbyte rc=$rc (want 1)"; [ $rc -eq 1 ] || fail=1
WW_CLOUDVOL_TYPERULE=0 "$PY" -I tests/spells/cloudtype_check.py "$S/off" --only ARN < /dev/null > "$S/red_off.log" 2>&1
rc=$?; echo "red WW_CLOUDVOL_TYPERULE=0 rc=$rc (want 1)"; [ $rc -eq 1 ] || fail=1
# the wiring: the base tree (before CLOUDTYPE2) has none
git show 563dcf97:src/gl/cloudmap.cpp > "$S/base/cloudmap.cpp" 2>/dev/null && git show 563dcf97:src/gl/cloudvol.cpp > "$S/base/cloudvol.cpp" 2>/dev/null && {
	"$PY" -I tests/spells/cloudtype_check.py "$S/w" --only W --src "$S/base" < /dev/null > "$S/red_wiring.log" 2>&1
	rc=$?; echo "red wiring (base 563dcf97) rc=$rc (want 1)"; [ $rc -eq 1 ] || fail=1
}
"$PY" -I tests/cloudband/stage_check.py src/wwswitchstages.inc WW_CLOUDVOL_TYPERULE
rc=$?; echo "stage rc=$rc (want 0)"; [ $rc -eq 0 ] || fail=1
git show 563dcf97:src/wwswitchstages.inc > "$S/base/stages.inc" 2>/dev/null && {
	"$PY" -I tests/cloudband/stage_check.py "$S/base/stages.inc" WW_CLOUDVOL_TYPERULE
	rc=$?; echo "stage red (base table) rc=$rc (want 1)"; [ $rc -eq 1 ] || fail=1
}
echo "CLOUDTYPE-RUN $([ $fail -eq 0 ] && echo PASS || echo FAIL)"
exit $fail
