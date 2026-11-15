#!/bin/bash
# Lane VKOBJ5 item 2: the RT-twin pick table gate (tests/vkframe/twinpick/twinpick_test.cpp over src/features/near/celltwinpick.h).
#   bash tests/vkframe/twinpick/run.sh <out dir>        (from the tree root; CXX defaults to g++, DXC as vkframe_spirv.sh)
# First dxc proves the inert defines (a source built with and without them gives the same SPIR-V bytes), then
# green must print "TWINPICK PASS <n>"; reds (each must print its "FAIL <check>:" and exit 1): swap (the RT modules of
# twins 2 and 3 swapped), noinert (no define taken as inert without the proof), anybase (round 7: the old pick, twins 2-6
# take a key with an extra define). Every run is capped by `timeout 30`.
set -u
OUT="${1:?usage: run.sh <out dir>}"
CXX="${CXX:-g++}"
DXC="${DXC:-/e/Tools/dxc/bin/x64/dxc.exe}"
mkdir -p "$OUT"
SRC=tests/vkframe/twinpick/twinpick_test.cpp
PS_SHIFT="-fvk-b-shift 0 0 -fvk-t-shift 64 0 -fvk-s-shift 128 0 -fvk-u-shift 192 0"
RT="-D WW_CELL_RTDDGI=1 -D WW_CELL_RTDDGI_CASCADES=1 -D WW_CELL_RESTIR=1"
p() { if command -v cygpath > /dev/null; then cygpath -am "$1"; else echo "$1"; fi; }
bad=0
# spv <out name> <core source> <defines>: vkframe_spirv.sh's fxc compile
spv() {
	"$DXC" -spirv -fspv-target-env=vulkan1.2 -HV 2021 -T ps_6_0 -E main -D WW_CELL_FOGLEAK=1 -D WW_FRAME_UBO=1 $PS_SHIFT $3 \
		-fspv-preserve-bindings -fvk-use-dx-position-w -Fo "$(p "$OUT/$1.spv")" "$(p "res/shaders/hlsl/core/$2.hlsl")" > "$OUT/$1.log" 2>&1
}
# inert <source> <defines list> <base defines> <base defines + them>: the same bytes or not inert
INERT=()
inert() {
	if spv "$1_a" "$1" "$3" && spv "$1_b" "$1" "$4" && cmp -s "$OUT/$1_a.spv" "$OUT/$1_b.spv"; then
		echo "inert $1: $2 (the same $(stat -c %s "$OUT/$1_a.spv") bytes with and without)"
		return 0
	fi
	echo "NOT INERT $1: $2 (see $OUT/$1_a.log, $1_b.log)"
	return 1
}
if inert pbrm_default_ps WW_CELL_RESTIR,WW_CELL_RTDDGI,WW_CELL_RTDDGI_CASCADES "" "$RT"; then
	INERT+=( --inert pbrm_default_ps:WW_CELL_RESTIR,WW_CELL_RTDDGI,WW_CELL_RTDDGI_CASCADES )
fi
if inert fo4_effectdecalcsm_ps WW_CELL_AIRZONE "" "-D WW_CELL_AIRZONE=1" \
	&& inert fo4_effectdecalcsm_ps "WW_CELL_AIRZONE (RT variant)" "$RT" "$RT -D WW_CELL_AIRZONE=1"; then
	INERT+=( --inert fo4_effectdecalcsm_ps:WW_CELL_AIRZONE )
fi
# lane VKOBJ8: twin 10 (fo4_effectdecal) takes the air-zone keys on the same proof
if inert fo4_effectdecal_ps WW_CELL_AIRZONE "" "-D WW_CELL_AIRZONE=1" \
	&& inert fo4_effectdecal_ps "WW_CELL_AIRZONE (RT variant)" "$RT" "$RT -D WW_CELL_AIRZONE=1"; then
	INERT+=( --inert fo4_effectdecal_ps:WW_CELL_AIRZONE )
fi
"$CXX" -O1 -std=c++17 -Wall -I src "$SRC" -o "$OUT/green.exe" -static || { echo "BUILD FAIL green"; exit 2; }
timeout 30 "$OUT/green.exe" . "${INERT[@]}" > "$OUT/green.txt"; rc=$?
cat "$OUT/green.txt"
if [ $rc -ne 0 ] || ! grep -q '^TWINPICK PASS ' "$OUT/green.txt"; then echo "GREEN FAILED (rc $rc)"; bad=1; fi
red() {	# red <name> <check> <exe> [args]
	local name=$1 check=$2 exe=$3; shift 3
	timeout 30 "$exe" "$@" > "$OUT/red_$name.txt"; local r=$?
	if [ $r -eq 1 ] && grep -q "^FAIL $check:" "$OUT/red_$name.txt"; then
		echo "red $name: fails $check as it must ($(grep -c '^FAIL' "$OUT/red_$name.txt") check(s) failed)"
	else
		echo "RED $name DID NOT FAIL $check (rc $r)"; bad=1
	fi
}
"$CXX" -O1 -std=c++17 -I src -DTWINPICK_RED_SWAP "$SRC" -o "$OUT/red_swap.exe" -static || { echo "BUILD FAIL red swap"; exit 2; }
red swap module "$OUT/red_swap.exe" . "${INERT[@]}"
red noinert module "$OUT/green.exe" .
"$CXX" -O1 -std=c++17 -I src -DCELLTWINPICK_RED_ANYBASE "$SRC" -o "$OUT/red_anybase.exe" -static || { echo "BUILD FAIL red anybase"; exit 2; }
red anybase extra "$OUT/red_anybase.exe" . "${INERT[@]}"
[ $bad -eq 0 ] && echo "TWINPICK GATE OK (green + 3 reds)" || echo "TWINPICK GATE FAILED"
exit $bad
