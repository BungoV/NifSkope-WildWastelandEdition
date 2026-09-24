#!/bin/bash
# AUDIT1 step 6: the fixed exe measured on both sides of every fix.
#
# A fix that only makes bad files refuse is half a fix. Each block below asks
# two questions: does the doctored file now go RED, and does the legitimate file
# the same rule touches still go GREEN. The second question is the one that
# catches a fix that works by refusing everything.
#
#   usage: bash step6_verify.sh <exe>
set -u
ROOT=/e/Projects/NifskopeWildWastelandEdition
PY="${PY:-/c/Users/bungo/AppData/Local/Programs/Python/Python39/python}"
S="$ROOT/scratchpad/audit1_20260916"
NS="${1:-$ROOT/release/NifSkope.exe}"
ESM="X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm"
DATA="E:/Tools/Fallout 4/DataUnpacked/Data"
bad=0

verify () {  # verify <lodo> <lodi>  -> prints rc and the aggregateStride it reports
	"$NS" -no-gui lodgen "$ESM" --worldspace 3C --native-verify "$1" "$2" 2>&1
}

pair () { find "$1" -name '*.lodo' | head -1; }
pairi () { find "$1" -name '*.lodi' | head -1; }

echo "=== exe: $NS"
ls -la "$NS" | awk '{print "    " $5 " B  " $6 " " $7 " " $8}'

echo
echo "=== A. THE LEGITIMATE FILES MUST STILL VERIFY (a fix that refuses everything is not a fix)"
for t in sanctuary_fo4cs coast_fo4cs urban_fo4cs aggreal meshrep; do
	O="$(pair "$S/bake/$t")"; I="$(pairi "$S/bake/$t")"
	[ -n "$O" ] || { echo "    $t  NO PAIR"; continue; }
	out="$(verify "$O" "$I")"; rc=$?
	stride="$(printf '%s\n' "$out" | sed -n 's/.*lodi aggregateStride *//p' | head -1)"
	printf '    %-16s rc=%d  aggregateStride reported: %s\n' "$t" "$rc" "${stride:-<not printed>}"
	[ $rc -eq 0 ] || bad=$(( bad + 1 ))
done

echo
echo "=== B. THE DOCTORED FILES MUST NOW BE REFUSED (C1 and C2)"
T="$(mktemp -d)"
for case in lodi-wrap lodo-wrap agg-views agg-record; do
	rm -rf "$T/c"; mkdir -p "$T/c"
	cp "$(pair "$S/bake/meshrep")" "$T/c/a.lodo"
	cp "$(pairi "$S/bake/meshrep")" "$T/c/a.lodi"
	d="$("$PY" "$ROOT/tests/spells/lodgen_native_doctor.py" "$case" "$T/c/a.lodo" "$T/c/a.lodi" 2>&1)"
	want="$(printf '%s\n' "$d" | sed -n 's/.*expect: *//p' | head -1)"
	out="$(verify "$T/c/a.lodo" "$T/c/a.lodi")"; rc=$?
	if [ $rc -ne 0 ]; then
		hit="$(printf '%s\n' "$out" | grep -ai 'refus' | head -1 | cut -c1-96)"
		printf '    %-12s REFUSED  (want "%s")\n        %s\n' "$case" "$want" "$hit"
	else
		printf '    %-12s STILL ACCEPTED rc=0  (want "%s")\n' "$case" "$want"
		bad=$(( bad + 1 ))
	fi
done
rm -rf "$T"

echo
echo "=== C. C8: a valued switch spelled without its value must refuse before any work"
O="$(mktemp -d)/out"; mkdir -p "$O"
t0=$(date +%s)
out="$("$NS" -no-gui lodgen "$ESM" --worldspace 3C --terrain-region -20 24 -9 35 --dim 4 \
	--data-root "$DATA" --out-dir "$(cygpath -m "$O")" --native "$(cygpath -m "$O")" \
	--incremental 2>&1)"
rc=$?
el=$(( $(date +%s) - t0 ))
line="$(printf '%s\n' "$out" | grep -ai 'error\|needs a value' | head -1)"
n="$(find "$O" -type f 2>/dev/null | wc -l)"
printf '    rc=%d in %ds, %s file(s) written\n    %s\n' "$rc" "$el" "$n" "${line:-<no error line>}"
if [ $rc -eq 0 ] || [ "$n" -ne 0 ]; then
	echo "    STILL SILENT -- it baked instead of refusing"
	bad=$(( bad + 1 ))
fi

echo
echo "=== D. and the SAME switch spelled properly must still be taken"
R="$(mktemp -d)"
cp -r "$S/bake/sanctuary_fo4cs" "$R/prev" >/dev/null 2>&1
cp -r "$S/bake/sanctuary_fo4cs" "$R/now" >/dev/null 2>&1
t0=$(date +%s)
out="$("$NS" -no-gui lodgen "$ESM" --worldspace 3C --terrain-region -20 24 -9 35 --dim 4 \
	--data-root "$DATA" --out-dir "$(cygpath -m "$R/now")" \
	--incremental "$(cygpath -m "$R/prev")" --native "$(cygpath -m "$R/now")" 2>&1)"
rc=$?
el=$(( $(date +%s) - t0 ))
printf '    rc=%d in %ds\n' "$rc" "$el"
printf '%s\n' "$out" | grep -aiE '^(incremental|native cache|native-library-build)' | head -3 | sed 's/^/        /'
[ $rc -eq 0 ] || bad=$(( bad + 1 ))
rm -rf "$R"

echo
echo "STEP6 $bad problem(s)"
