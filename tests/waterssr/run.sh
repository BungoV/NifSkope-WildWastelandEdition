#!/bin/bash
# lane FIX1101A (bungo 10-31 RULED: "we need ssr for water"): the water's own SSR part, ON by default outdoors and indoors.
#   verdict   src/gl/cellssrwater.h: the water reflects with the SSR row off, its own row stops it, no water = the old
#             verdict; red -DWATERSSR_RED (WW_CELL_SSR_WATER_RED=1: the old coupling) must FAIL
#   ssr_gate  the real cell_ssrwater.prog + water_refl.glsl offline: the wall's reflected share vs Schlick, the flat ray's
#             bent read vs the full-normal ray, the fade at the screen edge; reds --red off (no water SSR, today under
#             WW_CELL_SSR=0) and --red old (WW_CELL_SSR_WATER_RED=1's march) must FAIL
# PY= the python with numpy (code1030 unit.sh puts MSYS2 first on PATH). Green: every arm as wanted, rc 0.
cd "$(dirname "$0")/../.." || exit 2
PY="${PY:-$(command -v python)}"
OUT="${TMPDIR:-/tmp}/waterssr_$$"
mkdir -p "$OUT"
export PATH=/c/msys64/ucrt64/bin:$PATH
ok=1
g++ -std=c++17 -O1 -I src/gl tests/waterssr/verdict_test.cpp -o "$OUT/v.exe" || ok=0
g++ -std=c++17 -O1 -DWATERSSR_RED -I src/gl tests/waterssr/verdict_test.cpp -o "$OUT/vr.exe" || ok=0
v=$(timeout 30 "$OUT/v.exe" | tail -1)
printf '%-34s %s (want PASS)\n' "verdict green" "$v"
case "$v" in *"VERDICT PASS"*) ;; *) ok=0 ;; esac
v=$(timeout 30 "$OUT/vr.exe" | tail -1)
printf '%-34s %s (want FAIL)\n' "verdict red (old coupling)" "$v"
case "$v" in *"VERDICT FAIL"*) ;; *) ok=0 ;; esac
arm() {	# name, want (PASS|FAIL), args...
	local n=$1 w=$2
	shift 2
	local v
	v=$(timeout 300 "$PY" -I "$@" < /dev/null 2>&1 | grep '^VERDICT')
	printf '%-34s %s (want %s)\n' "$n" "$v" "$w"
	case "$v" in "VERDICT $w"*) ;; *) ok=0 ;; esac
}
arm "ssr_gate green" PASS tests/waterssr/ssr_gate.py
arm "ssr_gate red off" FAIL tests/waterssr/ssr_gate.py --red off
arm "ssr_gate red old" FAIL tests/waterssr/ssr_gate.py --red old
rm -rf "$OUT"
if [ $ok = 1 ]; then echo "WATERSSR RESULT PASS"; exit 0; fi
echo "WATERSSR RESULT FAIL"
exit 1
