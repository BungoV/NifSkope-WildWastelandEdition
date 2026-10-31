#!/bin/bash
# lane REDRETIRE1: tests/cellshow green + every red. Two qmake builds (MSYS2 UCRT64): green = the shipping sources,
# red = the same with REDTWIN=1 (-DWW_REDTWIN, src/wwredtwin.h), where OPENCELLFIX2's retired reds live on as twins.
# Usage: bash tests/cellshow/run.sh [outdir]     PASS = green passes and every red fails. Never writes into the tree
# unless no outdir is given (then tests/cellshow/out, untracked).
T=$(cd "$(dirname "$0")/../.." && pwd)
O=${1:-$T/tests/cellshow/out}
mkdir -p "$O/green" "$O/red" "$O/tmp"
export PATH=/c/msys64/ucrt64/bin:$PATH
export TMP=$(cygpath -w "$O/tmp") TEMP=$(cygpath -w "$O/tmp") TMPDIR="$O/tmp"	# QTemporaryDir stays off C:
for b in green red; do
	cp "$T/.qmake.stash" "$O/$b/" 2>/dev/null
	extra=; [ $b = red ] && extra="REDTWIN=1"
	MSYSTEM=UCRT64 CHERE_INVOKING=1 /c/msys64/usr/bin/bash -lc "cd '$O/$b' && /ucrt64/bin/qmake6 '$T/tests/cellshow/cellshowtest.pro' $extra > qmake.log 2>&1 && make -j4 > make.log 2>&1" < /dev/null \
		|| { echo "BUILD FAIL $b"; grep -E "error|undefined" "$O/$b/make.log" | head -20; exit 2; }
done
G=$(ls "$O"/green/release/cellshowtest.exe "$O"/green/cellshowtest.exe 2>/dev/null | head -1)
R=$(ls "$O"/red/release/cellshowtest.exe "$O"/red/cellshowtest.exe 2>/dev/null | head -1)
fails=0
run() {	# name want(0 = pass, 1 = must fail) exe env...
	local n=$1 want=$2 exe=$3
	shift 3
	( cd "$(dirname "$exe")" && timeout 120 env "$@" "$exe" > "$O/$n.txt" 2>&1 < /dev/null )
	local rc=$?
	local ok=BAD
	if [ "$want" = 0 ]; then [ $rc = 0 ] && ok=OK; else [ $rc != 0 ] && [ $rc != 124 ] && ok=OK; fi
	echo "== $n rc=$rc (want $( [ $want = 0 ] && echo 0 || echo fail )) $ok: $(grep -E '^[0-9]+ checks' "$O/$n.txt" | tail -1)"
	[ $ok = OK ] || fails=$((fails + 1))
}
run green 0 "$G"
run green_on_red_build 0 "$R"	# the red build with no twin picked is the green
for r in menu sync groups; do run red_$r 1 "$G" WW_CELLSHOW_RED=$r; done
run red_wwdropdown 1 "$G" WW_DROPDOWN_RED=1
for r in ticks dropdown presets nest livebind foldrows inline arrowframe dropright override builtin presetbar showbutton; do
	run twin_$r 1 "$R" REDTWIN=CELLSHOW=$r
	run twin_${r}_green_build 0 "$G" REDTWIN=CELLSHOW=$r WW_CELLSHOW_RED=$r	# retired: the shipping build ignores both
done
run twin_fit 1 "$R" REDTWIN=DROPDOWN=fit
run twin_fit_green_build 0 "$G" REDTWIN=DROPDOWN=fit WW_DROPDOWN_RED=fit
if [ "$fails" = 0 ]; then echo "CELLSHOW PASS"; else echo "CELLSHOW FAIL ($fails)"; exit 1; fi
