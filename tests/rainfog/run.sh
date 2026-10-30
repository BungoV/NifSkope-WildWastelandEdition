#!/bin/bash
# lane CLOUD6e: the rain fog's offline gates (no NifSkope). Green must exit 0, each red must exit 1.
#   bash tests/rainfog/run.sh <scratch dir>        (from the tree root; needs the GPU driver, MSYS2 UCRT64 g++ and a
#   python WITH numpy first on PATH: code1030/unit.sh's MSYS python has none; PY=<python> overrides)
set -u
S=${1:-../rainfog_scratch}
PY="${PY:-python}"
BASE=0e7c2c3a
mkdir -p "$S"
fail=0
unset WW_CLOUD_RAINFOG WW_CLOUD_RAINFOG_RED
"$PY" -I tests/spells/rainfog_check.py "$S/green" --base $BASE < /dev/null > "$S/green.log" 2>&1
rc=$?; echo "green rc=$rc (want 0)"; [ $rc -eq 0 ] || fail=1
# today's program (no rain fog) must fail every law gate; each one alone, so one pass cannot hide behind another
for g in I P S H; do
	"$PY" -I tests/spells/rainfog_check.py "$S/today" --base $BASE --red today --only $g < /dev/null > "$S/red_today_$g.log" 2>&1
	rc=$?; echo "red today $g rc=$rc (want 1)"; [ $rc -eq 1 ] || fail=1
done
"$PY" -I tests/spells/rainfog_check.py "$S/flag" --base $BASE --red flagrule --only K < /dev/null > "$S/red_flagrule.log" 2>&1
rc=$?; echo "red flagrule rc=$rc (want 1)"; [ $rc -eq 1 ] || fail=1
"$PY" -I tests/spells/rainfog_check.py "$S/wiring" --base $BASE --red wiring --only W < /dev/null > "$S/red_wiring.log" 2>&1
rc=$?; echo "red wiring rc=$rc (want 1)"; [ $rc -eq 1 ] || fail=1
# the switches are live (stage 0); red = the base's table (missing: a set would be a relaunch)
"$PY" -I tests/cloudband/stage_check.py src/wwswitchstages.inc WW_CLOUD_RAINFOG WW_CLOUD_RAINFOG_RED
rc=$?; echo "stage rc=$rc (want 0)"; [ $rc -eq 0 ] || fail=1
git show $BASE:src/wwswitchstages.inc > "$S/base_stages.inc" 2>/dev/null && {
	"$PY" -I tests/cloudband/stage_check.py "$S/base_stages.inc" WW_CLOUD_RAINFOG WW_CLOUD_RAINFOG_RED
	rc=$?; echo "stage red (base table) rc=$rc (want 1)"; [ $rc -eq 1 ] || fail=1
}
echo "RAINFOG-RUN $([ $fail -eq 0 ] && echo PASS || echo FAIL)"
exit $fail
