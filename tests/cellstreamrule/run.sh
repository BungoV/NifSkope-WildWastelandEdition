#!/bin/bash
# Lane AIRPERF1 round 4: unit test of src/cellstreamrule.h + src/features/gi/rttracesetrule.h, green then one build per
# red; each red must fail its own check and no other. usage: bash tests/cellstreamrule/run.sh <outdir>
T=$(cd "$(dirname "$0")/../.." && pwd)
O=${1:?outdir}
mkdir -p "$O"
export PATH=/c/msys64/ucrt64/bin:$PATH
S="$T/tests/cellstreamrule/cellstreamrule_test.cpp"
g++ -std=c++17 -O2 -I"$T/src" "$S" -o "$O/green.exe" || { echo "CELLSTREAMRULE BUILD FAIL"; exit 2; }
"$O/green.exe" > "$O/green.txt"; g=$?
grep -c '^ok' "$O/green.txt" | sed 's/^/green ok lines: /'; grep '^FAIL\|^flight\|^box' "$O/green.txt"; tail -1 "$O/green.txt"
bad=0
# red macro : the check it must fail
for pair in "CELLSTREAMRULE_RED_SPECFIXED:u1 open" "CELLSTREAMRULE_RED_NOPAUSE:u1 hold" "CELLSTREAMRULE_RED_PARKED:u1 follow" \
	"RTTRACESETRULE_RED_CONSTKEY:u2 key" "TRACESET_RED_PARKED:u2 publish" "TRACESET_RED_FIRSTBOX:u3 box" "TRACESET_RED_NOFADE:u4 fade" \
	"RTTRACESETRULE_RED_NOCAP:u5 farcap" "RTTRACESETRULE_RED_ALLROWS:u5 rows" "RTTRACESETRULE_RED_BYCELLONLY:u5 skip" \
	"RTTRACESETRULE_RED_NOWARM:u6 warm" "CELLSTREAMRULE_RED_NEVEROWED:u7 owed" "CELLSTREAMRULE_RED_ROWNOSAVE:u7 row" \
	"RTTRACESETRULE_RED_FRAMEREL:u8 frame"; do
	m=${pair%%:*}; want=${pair#*:}
	g++ -std=c++17 -O2 -D$m -I"$T/src" "$S" -o "$O/$m.exe" || { echo "CELLSTREAMRULE RED BUILD FAIL $m"; exit 2; }
	"$O/$m.exe" > "$O/$m.txt"; r=$?
	if [ $r -ne 0 ] && grep -q "^FAIL $want" "$O/$m.txt"; then
		echo "red $m: rc $r, fails: $(grep '^FAIL' "$O/$m.txt" | cut -d: -f1 | cut -c6- | tr '\n' ';')"
	else
		echo "red $m: DID NOT FAIL '$want' (rc $r)"; bad=1
	fi
done
if [ $g -eq 0 ] && [ $bad -eq 0 ]; then echo "CELLSTREAMRULE UNIT PASS (green rc 0, 14 reds each fail their check)"; exit 0; fi
echo "CELLSTREAMRULE UNIT FAIL (green rc $g, reds bad $bad)"; exit 1
