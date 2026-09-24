#!/bin/bash
# Lane BUILD8: every harness the change reaches, ONE AT A TIME (one NifSkope
# instance ever). Each writes its own log and echoes its summary line.
#
#   src/hkxanim.cpp     -> HKX1's decode gates, HKX2's in-app playback gates,
#                          hkxanim_play.sh (the rig), HKX5's writer gates
#   src/nifcli.cpp      -> the CLI, measured in roundtrip.sh
#   the render hook     -> render_shot.sh, because the picture proof used it
set -u
ROOT=/e/Projects/NifskopeWildWastelandEdition
cd "$ROOT" || exit 9
OUT=$ROOT/scratchpad/build8_20260910/logs
mkdir -p "$OUT"

run () { local label="$1" secs="$2"; shift 2
	echo "### $label start $(date +%H:%M:%S)"
	timeout "$secs" "$@" > "$OUT/$label.log" 2>&1
	echo "### $label rc=$?  $(date +%H:%M:%S)"
	grep -E "checks, [0-9]+ failures|^PASS|^FAIL|^RESULT|gates: |not as registered|[0-9]+/[0-9]+$" \
		"$OUT/$label.log" | tail -5
	echo
}

run hkxanim_gates    900 python tests/spells/hkxanim_gates.py
run hkxanim_mutate   900 python tests/spells/hkxanim_mutate.py
run hkxanim_synth    900 python tests/spells/hkxanim_synthetic.py
run hkxanim_play     900 bash  tests/spells/hkxanim_play.sh
run render_shot      900 bash  tests/spells/render_shot.sh
echo "GATES DONE"
