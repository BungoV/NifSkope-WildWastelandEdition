#!/bin/bash
# lane CLOUD6h: History Age + Lit Clamp offline gates (no NifSkope). Green must exit 0, each red must exit 1.
#   bash tests/cloudage/run.sh <scratch dir>     (from the tree root; needs the GPU driver, MSYS2 UCRT64 g++ and a python
#   WITH numpy: PY=<python> overrides)
# Then CLOUDBAND1/2's and CLOUD6g's gates on the new default: tests/cloudband/run.sh, tests/cloudstale/run.sh.
set -u
S=${1:-../cloudage_scratch}
PY="${PY:-$(command -v python)}"
mkdir -p "$S"
fail=0
unset WW_CLOUDVOL_HISTAGE WW_CLOUDVOL_LITCLAMP WW_CLOUDVOL_MOVECLAMP WW_CLOUDVOL_CHANGECLAMP WW_CLOUDVOL_ACCUM WW_CLOUDVOL_RED
"$PY" -I tests/spells/cloudage_check.py "$S/green" < /dev/null > "$S/green.log" 2>&1
rc=$?; echo "green rc=$rc (want 0)"; [ $rc -eq 0 ] || fail=1
"$PY" -I tests/spells/cloudage_check.py "$S/noage" --red noage --only A < /dev/null > "$S/red_noage.log" 2>&1
rc=$?; echo "red noage rc=$rc (want 1)"; [ $rc -eq 1 ] || fail=1
"$PY" -I tests/spells/cloudage_check.py "$S/litblind" --red litblind --only L < /dev/null > "$S/red_litblind.log" 2>&1
rc=$?; echo "red litblind rc=$rc (want 1)"; [ $rc -eq 1 ] || fail=1
"$PY" -I tests/cloudage/wiring_check.py src/gl/cloudvol.cpp res/shaders/cloud_resolve.frag "$S/green/cloudband_dump.exe" < /dev/null > "$S/wiring.log" 2>&1
rc=$?; echo "wiring rc=$rc (want 0)"; [ $rc -eq 0 ] || fail=1
git show 563dcf97:src/gl/cloudvol.cpp > "$S/base_cloudvol.cpp" 2>/dev/null && git show 563dcf97:res/shaders/cloud_resolve.frag > "$S/base_resolve.frag" 2>/dev/null && {
	"$PY" -I tests/cloudage/wiring_check.py "$S/base_cloudvol.cpp" "$S/base_resolve.frag" < /dev/null > "$S/wiring_red.log" 2>&1
	rc=$?; echo "wiring red (base 563dcf97) rc=$rc (want 1)"; [ $rc -eq 1 ] || fail=1
}
"$PY" -I tests/cloudband/stage_check.py src/wwswitchstages.inc WW_CLOUDVOL_HISTAGE WW_CLOUDVOL_LITCLAMP
rc=$?; echo "stage rc=$rc (want 0)"; [ $rc -eq 0 ] || fail=1
git show 563dcf97:src/wwswitchstages.inc > "$S/base_stages.inc" 2>/dev/null && {
	"$PY" -I tests/cloudband/stage_check.py "$S/base_stages.inc" WW_CLOUDVOL_HISTAGE WW_CLOUDVOL_LITCLAMP
	rc=$?; echo "stage red (base table) rc=$rc (want 1)"; [ $rc -eq 1 ] || fail=1
}
echo "CLOUDAGE-RUN $([ $fail -eq 0 ] && echo PASS || echo FAIL)"
exit $fail
