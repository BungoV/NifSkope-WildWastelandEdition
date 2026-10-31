#!/usr/bin/env bash
# lane REDRETIRE2 (2026-10-31): FLYFIX1's free-camera rules (src/wwflyfix.h) after its env red left NifSkope.exe.
# Two g++ builds (MSYS2 UCRT64): green = the shipping header (the twins are a constant false), red = -DWW_REDTWIN
# (src/wwredtwin.h), where the old way lives on as REDTWIN=FLYFIX=tick|refocus.
# PASS = green passes, the red build with no twin passes, every twin fails, and the green build ignores both the
# twin and the retired env name. Prints "FLYFIX UNIT PASS" only then.
#   bash tests/flyfix/run.sh [outdir]      (default notes/fix1030c/redretire2/unit)
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
OUT="${1:-/e/Projects/Claude/notes/fix1030c/redretire2/unit}"
UCRT="${UCRT:-/c/msys64/ucrt64}"
export PATH="$UCRT/bin:$PATH"
CXX="${CXX:-g++}"
mkdir -p "$OUT"
for v in green red; do
	def=""
	[ "$v" = red ] && def="-DWW_REDTWIN"
	if ! "$CXX" -std=c++17 -O1 -w $def -I"$ROOT/src" -I"$UCRT/include/qt6" -I"$UCRT/include/qt6/QtCore" \
		-o "$OUT/$v.exe" "$HERE/flyfixtest.cpp" -lQt6Core > "$OUT/$v.build.txt" 2>&1; then
		cat "$OUT/$v.build.txt"
		echo "FLYFIX UNIT FAIL (build $v)"
		exit 2
	fi
done
fails=0
run() {	# name want(0 = pass, 1 = must fail) exe env...
	local n=$1 want=$2 exe=$3
	shift 3
	timeout 120 env "$@" "$exe" > "$OUT/$n.txt" 2>&1 < /dev/null
	local rc=$?
	local ok=BAD
	if [ "$want" = 0 ]; then [ $rc = 0 ] && ok=OK; else [ $rc != 0 ] && [ $rc != 124 ] && ok=OK; fi
	echo "== $n rc=$rc (want $( [ $want = 0 ] && echo 0 || echo fail )) $ok: $(grep -E '^[0-9]+ checks' "$OUT/$n.txt" | tail -1)"
	[ $ok = OK ] || fails=$((fails + 1))
}
run green 0 "$OUT/green.exe"
run green_on_red_build 0 "$OUT/red.exe"	# the red build with no twin picked is the green
for r in tick refocus; do
	run twin_$r 1 "$OUT/red.exe" REDTWIN=FLYFIX=$r
	run twin_${r}_green_build 0 "$OUT/green.exe" REDTWIN=FLYFIX=$r WW_FLYFIX_RED=1	# retired: the shipping build ignores both
done
if [ "$fails" = 0 ]; then echo "FLYFIX UNIT PASS"; exit 0; fi
echo "FLYFIX UNIT FAIL ($fails)"; exit 1
