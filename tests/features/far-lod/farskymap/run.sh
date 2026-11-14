#!/bin/bash
# FARSKYMAP1 gates: G1 the C++ map (farskymap_unit.cpp), G2 the HLSL twin on a headless Vulkan device
# (skymapmatch.cpp + skymap_kernel.hlsl). Each red must FAIL. Usage: run.sh <tmp dir>
H=$(cd "$(dirname "$0")" && pwd)
T=${1:-$H/out}
mkdir -p "$T"
export PATH=/c/msys64/ucrt64/bin:$PATH
rc=0
g1() { # $1 = name, $2 = extra define
	g++ -std=c++17 -O1 -Wall $2 "$H/farskymap_unit.cpp" -o "$T/unit_$1.exe" > "$T/unit_$1.cc.txt" 2>&1 || { echo "G1 $1 FAILED (g++)"; return 2; }
	"$T/unit_$1.exe" "$T" > "$T/unit_$1.txt" 2>&1; local r=$?
	echo "G1 $1 rc=$r $(tail -1 "$T/unit_$1.txt")"
	return $r
}
g1 green "" || rc=1
g1 red_octant -DFARSKYMAP_RED_OCTANT && { echo "G1 red_octant did NOT fail"; rc=1; }
g1 red_layout -DFARSKYMAP_RED_LAYOUT && { echo "G1 red_layout did NOT fail"; rc=1; }
if [ -f "$H/g2.sh" ]; then bash "$H/g2.sh" "$T" || rc=1; fi
echo "FARSKYMAP GATES rc=$rc"
exit $rc
