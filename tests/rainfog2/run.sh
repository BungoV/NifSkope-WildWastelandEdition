#!/bin/bash
# lane RAINFOG2: the rain fog in the tails, the rate from the precipitation shader, PRECIPFLAG1, SNOWFOG1 -- offline
# (no NifSkope window). Green must exit 0, each red must exit 1.
#   bash tests/rainfog2/run.sh <scratch dir>     (from the tree root; needs the GPU driver, MSYS2 UCRT64 g++ + Qt6Core and
#   a python WITH numpy: PY=<python> overrides). With the built exe and the game's Data folder (DATA=<dir>, default the
#   Steam path) it also diffs `-no-gui weather --precip` against the census; without them that part is skipped.
set -u
S=${1:-../rainfog2_scratch}
PY="${PY:-$(command -v python)}"
BASE=0ec6dcc0
DATA="${DATA:-X:/Programs/Steam/steamapps/common/Fallout 4/Data}"
mkdir -p "$S"
fail=0
unset WW_CLOUD_RAINFOG_TAIL WW_CLOUD_RAINFOG_TAIL_RED WW_CLOUD_SNOWFOG WW_CLOUD_SNOWFOG_RED WW_PRECIP_SPGD WW_PRECIP_SPGD_RED
"$PY" -I tests/rainfog2/rainfog2_check.py "$S/green" --base $BASE < /dev/null > "$S/green.log" 2>&1
rc=$?; echo "green rc=$rc (want 0)"; [ $rc -eq 0 ] || fail=1
# today's rules / today's lookdev_fog.glsl must fail every gate that carries a claim; one gate a run
for g in T K R S; do
	"$PY" -I tests/rainfog2/rainfog2_check.py "$S/today" --base $BASE --red today --only $g < /dev/null > "$S/red_today_$g.log" 2>&1
	rc=$?; echo "red today $g rc=$rc (want 1)"; [ $rc -eq 1 ] || fail=1
done
"$PY" -I tests/rainfog2/rainfog2_check.py "$S/wiring" --base $BASE --red wiring --only W < /dev/null > "$S/red_wiring.log" 2>&1
rc=$?; echo "red wiring rc=$rc (want 1)"; [ $rc -eq 1 ] || fail=1
# the six switches are live (stage 0); red = the base's table (missing: a set would be a relaunch)
N="WW_CLOUD_RAINFOG_TAIL WW_CLOUD_RAINFOG_TAIL_RED WW_CLOUD_SNOWFOG WW_CLOUD_SNOWFOG_RED WW_PRECIP_SPGD WW_PRECIP_SPGD_RED"
"$PY" -I tests/cloudband/stage_check.py src/wwswitchstages.inc $N > "$S/stage.log"
rc=$?; echo "stage rc=$rc (want 0)"; [ $rc -eq 0 ] || fail=1
git show $BASE:src/wwswitchstages.inc > "$S/base_stages.inc" 2>/dev/null && {
	"$PY" -I tests/cloudband/stage_check.py "$S/base_stages.inc" $N > "$S/stage_red.log"
	rc=$?; echo "stage red (base table) rc=$rc (want 1)"; [ $rc -eq 1 ] || fail=1
}
# CLOUD6e's own gates still hold on this tree (its rain law and wiring are untouched)
bash tests/rainfog/run.sh "$S/rainfog" > "$S/rainfog.log" 2>&1
rc=$?; echo "tests/rainfog rc=$rc (want 0)"; [ $rc -eq 0 ] || fail=1
if [ -f release/NifSkope.exe ] && [ -f "$DATA/Fallout4.esm" ]; then
	"$PY" -I tests/rainfog2/exe_census.py release/NifSkope.exe "$DATA" "$S/exe_precip.txt" < /dev/null > "$S/exe_census.log" 2>&1
	rc=$?; echo "exe census rc=$rc (want 0)"; [ $rc -eq 0 ] || fail=1
else
	echo "exe census skipped (no release/NifSkope.exe or no $DATA/Fallout4.esm)"
fi
echo "RAINFOG2-RUN $([ $fail -eq 0 ] && echo PASS || echo FAIL)"
exit $fail
