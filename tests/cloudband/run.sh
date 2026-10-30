#!/bin/bash
# lane CLOUDBAND1: Cloud Accumulation's offline gates (no NifSkope). Green must exit 0, each red must exit 1.
#   bash tests/cloudband/run.sh <scratch dir>        (from the tree root; needs the GPU driver and MSYS2 UCRT64 g++)
set -u
S=${1:-../cloudband_scratch}
mkdir -p "$S"
fail=0
python -I tests/spells/cloudband_check.py "$S/green" --only LMGF < /dev/null > "$S/green.log" 2>&1
rc=$?; echo "green rc=$rc (want 0)"; [ $rc -eq 0 ] || fail=1
for r in ign noaccum noclamp; do
	python -I tests/spells/cloudband_check.py "$S/$r" --red $r --only LMGF < /dev/null > "$S/red_$r.log" 2>&1
	rc=$?; echo "red $r rc=$rc (want 1)"; [ $rc -eq 1 ] || fail=1
done
python -I tests/cloudband/stage_check.py src/wwswitchstages.inc
rc=$?; echo "stage rc=$rc (want 0)"; [ $rc -eq 0 ] || fail=1
git show f46b1c76:src/wwswitchstages.inc > "$S/base_stages.inc" 2>/dev/null && {
	python -I tests/cloudband/stage_check.py "$S/base_stages.inc"
	rc=$?; echo "stage red (base table) rc=$rc (want 1)"; [ $rc -eq 1 ] || fail=1
}
echo "CLOUDBAND-RUN $([ $fail -eq 0 ] && echo PASS || echo FAIL)"
exit $fail
