#!/bin/bash
# FARSKYMAP1 G2: the HLSL math on a headless Vulkan device against the C++ twin; red -D FARSKY_RED must FAIL.
# Usage: g2.sh <tmp dir>. DXC -O0 as tests/hlslmatch/run.sh (its optimizer reorders float math).
H=$(cd "$(dirname "$0")" && pwd)
T=${1:?tmp dir}
DXC="${DXC:-/e/Tools/dxc/bin/x64/dxc.exe}"
W() { cygpath -m "$1" 2>/dev/null || echo "$1"; }
export PATH=/c/msys64/ucrt64/bin:$PATH
g++ -O1 -std=c++17 "$H/skymapmatch.cpp" -o "$T/skymapmatch.exe" > "$T/skymapmatch.cc.txt" 2>&1 || { cat "$T/skymapmatch.cc.txt" | head -20; echo "G2 FAILED (g++)"; exit 1; }
"$DXC" -spirv -fspv-target-env=vulkan1.2 -T cs_6_0 -E main -O0 -Fo "$(W "$T/skymap.spv")" "$(W "$H/skymap_kernel.hlsl")" || { echo "G2 FAILED (dxc)"; exit 1; }
"$DXC" -spirv -fspv-target-env=vulkan1.2 -T cs_6_0 -E main -O0 -D FARSKY_RED -Fo "$(W "$T/skymap_red.spv")" "$(W "$H/skymap_kernel.hlsl")" || { echo "G2 FAILED (dxc red)"; exit 1; }
"$DXC" -spirv -fspv-target-env=vulkan1.2 -T cs_6_0 -E main -O0 -D FARSKY_RED_NEAR -Fo "$(W "$T/skymap_rednear.spv")" "$(W "$H/skymap_kernel.hlsl")" || { echo "G2 FAILED (dxc red near)"; exit 1; }
"$T/skymapmatch.exe" "$(W "$T/skymap.spv")" > "$T/g2_green.txt"; g=$?
"$T/skymapmatch.exe" "$(W "$T/skymap_red.spv")" > "$T/g2_red.txt"; r=$?
"$T/skymapmatch.exe" "$(W "$T/skymap_rednear.spv")" > "$T/g2_rednear.txt"; rn=$?
echo "G2 green rc=$g $(tail -1 "$T/g2_green.txt")"
echo "G2 red (FARSKY_RED) rc=$r $(tail -1 "$T/g2_red.txt")"
echo "G2 red (FARSKY_RED_NEAR) rc=$rn $(tail -1 "$T/g2_rednear.txt")"
# each red must fail as a mismatch (rc 1), never as a setup error (rc 3)
[ $g -eq 0 ] && [ $r -eq 1 ] && [ $rn -eq 1 ] && { echo "G2 OK"; exit 0; }
echo "G2 FAILED"; exit 1
