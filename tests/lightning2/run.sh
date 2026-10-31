#!/bin/bash
# lane LIGHTNING2 (reviewed and finished by FIX1031B): the flash on the 2D layers and the exterior ambient, the DATA
# frequency law (opt-in), offline (no NifSkope). Green must exit 0, each red must exit 1.
#   bash tests/lightning2/run.sh <scratch dir>   (from the tree root; GPU driver, MSYS2 UCRT64 g++, PY=<python> overrides)
set -u
S=${1:-../lightning2_scratch}
PY="${PY:-$(command -v python)}"
BASE=5422be6c
mkdir -p "$S"
fail=0
unset WW_LIGHTNING_FREQ WW_LIGHTNING_FREQ_RED WW_LIGHTNING_SKY WW_LIGHTNING_SKY_RED
"$PY" -I tests/spells/lightning2_check.py "$S/green" < /dev/null > "$S/green.log" 2>&1
rc=$?; echo "green rc=$rc (want 0)"; [ $rc -eq 0 ] || fail=1
for g in Q W; do
	"$PY" -I tests/spells/lightning2_check.py "$S/oldfreq" --red oldfreq --only $g < /dev/null > "$S/red_oldfreq_$g.log" 2>&1
	rc=$?; echo "red oldfreq $g rc=$rc (want 1)"; [ $rc -eq 1 ] || fail=1
done
for g in G K U; do
	"$PY" -I tests/spells/lightning2_check.py "$S/today" --red today --only $g < /dev/null > "$S/red_today_$g.log" 2>&1
	rc=$?; echo "red today $g rc=$rc (want 1)"; [ $rc -eq 1 ] || fail=1
done
"$PY" -I tests/lightning2/wiring_check.py . > "$S/wiring.log" 2>&1
rc=$?; echo "wiring rc=$rc (want 0)"; [ $rc -eq 0 ] || fail=1
"$PY" -I tests/lightning2/wiring_check.py . $BASE > "$S/wiring_red.log" 2>&1
rc=$?; echo "wiring red (base $BASE) rc=$rc (want 1)"; [ $rc -eq 1 ] || fail=1
echo "LIGHTNING2-RUN $([ $fail -eq 0 ] && echo PASS || echo FAIL)"
exit $fail
