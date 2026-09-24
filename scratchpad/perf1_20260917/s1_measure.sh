#!/usr/bin/env bash
# PERF1 step 1 -- MEASURE FIRST, on the exe at launch, before any source change.
#
#   bash s1_measure.sh <tag> <chunkthreads> <threads> <regionname> <x0> <y0> <x1> <y1>
#
# Bakes the ruled FO4CS pipeline (--native, --cover --roads --road-detail 1)
# into scratchpad/perf1_20260917/work/s1/<tag>/ and prints wall clock, the four
# stage times, the peak working set and the census bound-by word.
set -u
R="$(cd "$(dirname "$0")/../.." && pwd)"
D="$R/scratchpad/perf1_20260917"
EXE="${EXE:-$R/release/NifSkope.exe}"
DATA="${DATA:-E:/Tools/Fallout 4/DataUnpacked/Data}"
VANILLA="${VANILLA:-X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm}"

tag="$1"; ct="$2"; th="$3"; rn="$4"; x0="$5"; y0="$6"; x1="$7"; y1="$8"

if tasklist 2>/dev/null | grep -qi "Fallout4.exe"; then
	echo "REFUSED: Fallout4.exe is up"; exit 3
fi
[ -x "$EXE" ] || { echo "no exe at $EXE"; exit 2; }

dir="$D/work/s1/$tag"
rm -rf "$dir"; mkdir -p "$dir/obj" "$dir/tex" "$dir/nat"
da="$(cd "$dir" && { pwd -W 2>/dev/null || pwd; })"

t0=$(date +%s%N)
"$EXE" -no-gui lodgen "$VANILLA" --worldspace 3C \
	--terrain-region "$x0" "$y0" "$x1" "$y1" --dim 4 \
	--out-dir "$da/obj" --tex-dir "$da/tex" --data-root "$DATA" \
	--native "$da/nat" \
	--cover --roads --road-detail 1 \
	--threads "$th" --chunk-threads "$ct" \
	> "$dir/bake.log" 2>&1
rc=$?
t1=$(date +%s%N)
ms=$(( (t1-t0)/1000000 ))

echo "TAG=$tag REGION=$rn($x0 $y0 $x1 $y1) CHUNKTHREADS=$ct THREADS=$th rc=$rc WALL_MS=$ms"
grep -E "^stage times:" "$dir/bake.log" | tail -1 | sed 's/^/    /'
grep -oE "peak working set: [^,]*" "$dir/bake.log" | tail -1 | sed 's/^/    /'
grep -oE "bound by [a-z]+" "$dir/bake.log" | tail -1 | sed 's/^/    /'
grep -E "^bake census:" "$dir/bake.log" | tail -1 | sed 's/^/    census: /'
echo "TAG=$tag WALL_MS=$ms rc=$rc" >> "$D/s1_wall.txt"
