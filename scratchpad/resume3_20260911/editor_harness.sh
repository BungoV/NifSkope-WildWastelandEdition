#!/bin/bash
#
# RESUME3: the GUI editor harnesses the MODEL LAYER reaches, driven as a chain.
#
# NIFPARSE1's fixes touch src/model/nifmodel.cpp, src/data/nifvalue.cpp and
# src/xml/nifexpr.cpp -- the layer the whole editor sits on. These five hooks
# are the ones that load a real, big, skinned FO4 mesh and then move data in it.
# Their counts are taken on the RUNG first, so a count that moves afterwards can
# be named rather than argued about.
#
#   bash editor_harness.sh <exe> <tag>
#
# One NifSkope instance at a time: strictly sequential, each run polled for
# "done" in its own log then closed. Window on the SECOND monitor via
# _harness.sh's WW_WINDOW_AT; never raised, never focused.
set -u
ROOT=/e/Projects/NifskopeWildWastelandEdition
EXE="${1:?exe}"
TAG="${2:?tag}"
OUT="$ROOT/scratchpad/resume3_20260911/logs/editor_$TAG"
TORSO='E:\Projects\Fallout 4 Mods\mods\Fo76 Mega Pack\meshes\F76\actors\powerarmor\x01tesla\x01tesla_torso.nif'
mkdir -p "$OUT"

export WW_WINDOW_AT="1960,40"

run_hook () {
	local var="$1" val="$2" logname="$3" label="$4"
	local log="$ROOT/release/$logname"
	rm -f "$log"
	local t0=$(date +%s%3N)
	( export "$var=$val"; "$EXE" "$TORSO" > "$OUT/$label.stdout" 2>&1 ) &
	local pid=$!
	local waited=0
	while [ $waited -lt 180 ]; do
		if [ -f "$log" ] && grep -q "^done" "$log" 2>/dev/null; then break; fi
		if ! kill -0 $pid 2>/dev/null; then break; fi
		sleep 1; waited=$((waited+1))
	done
	local t1=$(date +%s%3N)
	sleep 1
	# the app hangs on GL teardown after a headless grab; close it, never -9 first
	taskkill //IM NifSkope.exe //T //F > /dev/null 2>&1
	taskkill //IM "$(basename "$EXE")" //T //F > /dev/null 2>&1
	sleep 1
	if [ -f "$log" ]; then cp -p "$log" "$OUT/$label.log"; fi
	local verdict="NOLOG"
	if [ -f "$log" ]; then
		if grep -qE "^PASS|: PASS|— PASS" "$log"; then verdict="PASS"; fi
		if grep -qE "^FAIL|CHECK:|FAILED" "$log"; then verdict="FAIL"; fi
	fi
	printf '%-14s %-8s %5s ms  wait=%ss\n' "$label" "$verdict" "$((t1-t0))" "$waited"
	grep -cE "^  ok|PASS" "$OUT/$label.log" 2>/dev/null | sed 's/^/    passlines=/'
}

echo "=== editor harnesses, exe $EXE, tag $TAG, $(date +%H:%M:%S) ==="
ls -l --time-style=full-iso "$EXE"
run_hook WW_JOIN_TEST        2 ww_join_test.log        join
run_hook WW_SEP_TEST         1 ww_sep_test.log         sep
run_hook WW_DUPFREEZE_TEST   1 ww_dupfreeze_test.log   dupfreeze
run_hook WW_COPYPASTE_TEST   1 ww_copypaste_test.log   copypaste
run_hook WW_VERTEXFLAGS_TEST 1 ww_vertexflags_test.log vertexflags
echo "=== end $(date +%H:%M:%S) ==="
