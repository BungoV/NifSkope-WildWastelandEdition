#!/bin/bash
# lane CLOUDBAND1: Cloud Accumulation's offline gates (no NifSkope). Green must exit 0, each red must exit 1.
# lane CLOUDBAND2: both switches are the DEFAULT; the gates judge the arm cloudvol.cpp draws for the env (the dump's
# "default" line), the red off = the old path (WW_CLOUDVOL_ACCUM=0 / WW_CLOUDVOL_AMBDARK=0), gate Q = FOGGLOW1's dither.
#   bash tests/cloudband/run.sh <scratch dir>        (from the tree root; needs the GPU driver, MSYS2 UCRT64 g++ and a
#   python WITH numpy first on PATH: code1030/unit.sh's MSYS python has none; PY=<python> overrides)
set -u
S=${1:-../cloudband_scratch}
PY="${PY:-python}"
mkdir -p "$S"
fail=0
unset WW_CLOUDVOL_ACCUM WW_CLOUDVOL_AMBDARK WW_CLOUDVOL_RED
"$PY" -I tests/spells/cloudband_check.py "$S/green" --only LMGQF < /dev/null > "$S/green.log" 2>&1
rc=$?; echo "green rc=$rc (want 0)"; [ $rc -eq 0 ] || fail=1
for r in ign noaccum noclamp; do
	"$PY" -I tests/spells/cloudband_check.py "$S/$r" --red $r --only LMGF < /dev/null > "$S/red_$r.log" 2>&1
	rc=$?; echo "red $r rc=$rc (want 1)"; [ $rc -eq 1 ] || fail=1
done
# lane CLOUDBAND2: the old path (the default before CLOUDBAND2) must fail the banding gates
"$PY" -I tests/spells/cloudband_check.py "$S/off" --red off --only LM < /dev/null > "$S/red_off.log" 2>&1
rc=$?; echo "red off rc=$rc (want 1)"; [ $rc -eq 1 ] || fail=1
"$PY" -I tests/spells/cloudband_check.py "$S/bayer" --red bayerdither --only Q < /dev/null > "$S/red_bayerdither.log" 2>&1
rc=$?; echo "red bayerdither rc=$rc (want 1)"; [ $rc -eq 1 ] || fail=1
# item (b): the ambient through the cloud above
"$PY" -I tests/spells/cloudamb_check.py "$S/amb" --only A < /dev/null > "$S/amb.log" 2>&1
rc=$?; echo "amb green rc=$rc (want 0)"; [ $rc -eq 0 ] || fail=1
for r in flatamb off; do
	"$PY" -I tests/spells/cloudamb_check.py "$S/amb" --red $r --only A < /dev/null > "$S/amb_red_$r.log" 2>&1
	rc=$?; echo "amb red $r rc=$rc (want 1)"; [ $rc -eq 1 ] || fail=1
done
# lane CLOUDBAND2: the default rule (cloudvol.cpp reads both through defaultOnSwitch); red = main before CLOUDBAND2
"$PY" -I tests/cloudband/default_check.py src/gl/cloudvol.cpp "$S/amb/cloudband_dump.exe" < /dev/null > "$S/default.log" 2>&1
rc=$?; echo "default rc=$rc (want 0)"; [ $rc -eq 0 ] || fail=1
git show e8fad765:src/gl/cloudvol.cpp > "$S/base_cloudvol.cpp" 2>/dev/null && {
	"$PY" -I tests/cloudband/default_check.py "$S/base_cloudvol.cpp" "$S/amb/cloudband_dump.exe" < /dev/null > "$S/default_red.log" 2>&1
	rc=$?; echo "default red (main e8fad765) rc=$rc (want 1)"; [ $rc -eq 1 ] || fail=1
}
"$PY" -I tests/cloudband/stage_check.py src/wwswitchstages.inc
rc=$?; echo "stage rc=$rc (want 0)"; [ $rc -eq 0 ] || fail=1
git show f46b1c76:src/wwswitchstages.inc > "$S/base_stages.inc" 2>/dev/null && {
	"$PY" -I tests/cloudband/stage_check.py "$S/base_stages.inc"
	rc=$?; echo "stage red (base table) rc=$rc (want 1)"; [ $rc -eq 1 ] || fail=1
}
echo "CLOUDBAND-RUN $([ $fail -eq 0 ] && echo PASS || echo FAIL)"
exit $fail
