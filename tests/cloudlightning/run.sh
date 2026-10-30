#!/bin/bash
# lane CLOUD6f: lightning inside the volume clouds, offline gates (no NifSkope). Green must exit 0, each red must exit 1.
#   bash tests/cloudlightning/run.sh <scratch dir>     (from the tree root; needs the GPU driver, MSYS2 UCRT64 g++ and a
#   python WITH numpy: PY=<python> overrides; the S gate reads the Fallout 4 masters (FO4_DATA) and SKIPs without them)
set -u
S=${1:-../cloudlightning_scratch}
PY="${PY:-python}"
BASE=0e7c2c3a
mkdir -p "$S"
fail=0
unset WW_CLOUD_LIGHTNING WW_CLOUD_LIGHTNING_RED
"$PY" -I tests/spells/cloudlightning_check.py "$S/green" < /dev/null > "$S/green.log" 2>&1
rc=$?; echo "green rc=$rc (want 0)"; [ $rc -eq 0 ] || fail=1
# red today = main's composite (no flash): the schedule, the inside light, the field and the flash control must fail
"$PY" -I tests/spells/cloudlightning_check.py "$S/green" --red today --only TLIF < /dev/null > "$S/red_today.log" 2>&1
rc=$?; echo "red today rc=$rc (want 1)"; [ $rc -eq 1 ] || fail=1
# red iso = the field without its first-order radiance term (the RDR2-like isotropic falloff alone)
"$PY" -I tests/spells/cloudlightning_check.py "$S/green" --red iso --only L < /dev/null > "$S/red_iso.log" 2>&1
rc=$?; echo "red iso rc=$rc (want 1)"; [ $rc -eq 1 ] || fail=1
# red thunderbyte = CLOUD5's storm rule (DATA thunder byte > 0): every weather storms
if grep -q "S storms     SKIP" "$S/green.log"; then
	echo "red thunderbyte SKIP (no Fallout 4 masters)"
else
	"$PY" -I tests/spells/cloudlightning_check.py "$S/green" --red thunderbyte --only S < /dev/null > "$S/red_thunderbyte.log" 2>&1
	rc=$?; echo "red thunderbyte rc=$rc (want 1)"; [ $rc -eq 1 ] || fail=1
fi
"$PY" -I tests/cloudlightning/wiring_check.py . < /dev/null > "$S/wiring.log" 2>&1
rc=$?; echo "wiring rc=$rc (want 0)"; [ $rc -eq 0 ] || fail=1
"$PY" -I tests/cloudlightning/wiring_check.py . $BASE < /dev/null > "$S/wiring_red.log" 2>&1
rc=$?; echo "wiring red (main $BASE) rc=$rc (want 1)"; [ $rc -eq 1 ] || fail=1
echo "CLOUDLIGHTNING-RUN $([ $fail -eq 0 ] && echo PASS || echo FAIL)"
exit $fail
