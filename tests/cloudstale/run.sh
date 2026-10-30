#!/bin/bash
# lane CLOUD6g: Move Clamp + Change Clamp offline gates (no NifSkope). Green must exit 0, each red must exit 1.
#   bash tests/cloudstale/run.sh <scratch dir>     (from the tree root; needs the GPU driver, MSYS2 UCRT64 g++ and a python
#   WITH numpy: PY=<python> overrides; code1030/unit.sh's MSYS python has none)
# Then CLOUDBAND1/2's gates on the new default (cloudband_check's default arms draw the new uniforms): tests/cloudband/run.sh.
set -u
S=${1:-../cloudstale_scratch}
PY="${PY:-python}"
mkdir -p "$S"
fail=0
unset WW_CLOUDVOL_MOVECLAMP WW_CLOUDVOL_CHANGECLAMP WW_CLOUDVOL_ACCUM WW_CLOUDVOL_RED
"$PY" -I tests/spells/cloudstale_check.py "$S/green" < /dev/null > "$S/green.log" 2>&1
rc=$?; echo "green rc=$rc (want 0)"; [ $rc -eq 0 ] || fail=1
for r in staleread looseclamp; do
	"$PY" -I tests/spells/cloudstale_check.py "$S/$r" --red $r --only STR < /dev/null > "$S/red_$r.log" 2>&1
	rc=$?; echo "red $r rc=$rc (want 1)"; [ $rc -eq 1 ] || fail=1
done
"$PY" -I tests/cloudstale/wiring_check.py src/gl/cloudvol.cpp res/shaders/cloud_resolve.frag "$S/green/cloudband_dump.exe" < /dev/null > "$S/wiring.log" 2>&1
rc=$?; echo "wiring rc=$rc (want 0)"; [ $rc -eq 0 ] || fail=1
git show 2f4d613d:src/gl/cloudvol.cpp > "$S/base_cloudvol.cpp" 2>/dev/null && git show 2f4d613d:res/shaders/cloud_resolve.frag > "$S/base_resolve.frag" 2>/dev/null && {
	"$PY" -I tests/cloudstale/wiring_check.py "$S/base_cloudvol.cpp" "$S/base_resolve.frag" < /dev/null > "$S/wiring_red.log" 2>&1
	rc=$?; echo "wiring red (base 2f4d613d) rc=$rc (want 1)"; [ $rc -eq 1 ] || fail=1
}
"$PY" -I tests/cloudband/stage_check.py src/wwswitchstages.inc WW_CLOUDVOL_MOVECLAMP WW_CLOUDVOL_CHANGECLAMP
rc=$?; echo "stage rc=$rc (want 0)"; [ $rc -eq 0 ] || fail=1
git show 2f4d613d:src/wwswitchstages.inc > "$S/base_stages.inc" 2>/dev/null && {
	"$PY" -I tests/cloudband/stage_check.py "$S/base_stages.inc" WW_CLOUDVOL_MOVECLAMP WW_CLOUDVOL_CHANGECLAMP
	rc=$?; echo "stage red (base table) rc=$rc (want 1)"; [ $rc -eq 1 ] || fail=1
}
echo "CLOUDSTALE-RUN $([ $fail -eq 0 ] && echo PASS || echo FAIL)"
exit $fail
