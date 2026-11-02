#!/bin/bash
# lane BUGS1102D: src/wwswitchstages.inc must equal what tools/wwswitch_survey.py --inc writes for the tree's sources
# (a merge that adds a switch read and skips the survey leaves the live driver without its stage: FRAMEFMT1-SURVEY).
# bash tests/switchsurvey/run.sh <scratch> [red]
# red: the committed table minus one row (WW_FRAMEFMT1_RED, the row a merge once dropped) must FAIL the same compare.
T=$(cd "$(dirname "$0")/../.." && pwd)
O=${1:?scratch dir}
mkdir -p "$O"
PY="${PY:-$(command -v python)}"
"$PY" -I "$T/tools/wwswitch_survey.py" --root "$T" --inc "$O/stages.inc" < /dev/null || exit 2
INC="$T/src/wwswitchstages.inc"
if [ "$2" = red ]; then
	grep -v '"WW_FRAMEFMT1_RED"' "$INC" > "$O/stale.inc"
	[ "$(wc -l < "$O/stale.inc")" -lt "$(wc -l < "$INC")" ] || { echo "RED: no WW_FRAMEFMT1_RED row to drop"; exit 2; }
	INC="$O/stale.inc"
fi
if cmp -s "$INC" "$O/stages.inc"; then
	echo "switchsurvey: table current ($(grep -c '^{' "$INC") switches)"
	exit 0
fi
echo "FAIL switchsurvey: src/wwswitchstages.inc is stale; run python tools/wwswitch_survey.py --inc src/wwswitchstages.inc"
diff "$INC" "$O/stages.inc" | head -20
exit 1
