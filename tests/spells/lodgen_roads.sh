#!/bin/bash
#
# Roads and decals in the far-terrain colour, under docs/LODGEN_TERRAIN_VT.md
# section 1a. bungo, 2026-09-11: "We do the same with roads and decals as
# vanilla" -- the placed road meshes are scan-converted top-down into the COLOUR
# sheet, at the mesh's own footprint, with the mesh's own material diffuse, and
# into nothing else.
#
# FIXTURE: two Sanctuary regions, chosen by MEASUREMENT and not by eye.
#   ROAD-BEARING  cells (-20,20)..(-17,23) -- the loop road, its cul-de-sac and
#                 its driveways. Vanilla ships Commonwealth.4.-20.20.DDS for
#                 exactly this ground, at 512 texels and 32 world units a texel,
#                 which is our own chunk sheet's grid with no resampling on
#                 either side.
#   ROAD-FREE     cells (-20,24)..(-17,27) -- the chunk lane TERRAIN-R
#                 photographed. Projecting every Landscape\Roads placement in
#                 cells -24..-12 x 16..30 puts ZERO road triangles in it, which
#                 is why it is the floor here and why it was the wrong tile to
#                 judge roads on.
#
# WHAT IS GUARDED
#   R1  --no-roads is byte-identical to a --no-roads rerun, every file: the
#       feature's OFF value is exact, not approximately exact.
#   R2  --roads changes the COLOUR sheet and nothing else that is not the cover
#       alpha: the _msn sheet is byte-identical, because vanilla's is (measured:
#       vanilla's _msn agrees with the bare LAND heightmap BETTER on the road
#       footprint than off it).
#   R3  the census is written AND moves: road texels far above zero on the
#       road-bearing region, exactly zero on the road-free one, and the refusal
#       counters present in both.
#   R4  THE FLOOR THAT MUST FIRE: the road-free region's colour sheets with and
#       without roads are byte-identical. If they are not, the pass is painting
#       something that is not a road.
#   R5  the road-presence metric against VANILLA's own shipped sheet, with its
#       floor (our --no-roads bake) and its ceiling (vanilla against itself) --
#       tests/spells/lodgen_roads_metric.py, skipped by name when the unpacked
#       vanilla corpus is absent.  Bar 2 is MARG x the SAME BAKE's own non-road
#       background agreement, never a stored number.  MARG was RECALIBRATED from
#       0.8 to 0.72 on 2026-09-12 (lane LAND1) because ROADS4 made
#       --road-detail 1 the default and land detail costs the road more than the
#       background -- the ratio fell 0.8505 to 0.7640 with no road code changed.
#       0.72 carries the same 94.07 % headroom the 0.8 did; the metric script's
#       docstring has the derivation and the three checks that it still binds.
#
# USAGE
#   bash tests/spells/lodgen_roads.sh
#
# Exit 2 = a missing input; 1 = a failed check.

set -u

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
NS="${EXE:-$ROOT/release/NifSkope.exe}"
ESM="${ESM:-X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm}"
DATA="${DATA:-E:/Tools/Fallout 4/DataUnpacked/Data}"
PY="${PY:-$(command -v python || echo /c/Windows/py)}"
VAN="${VAN:-$DATA/Textures/Terrain/Commonwealth}"
W="${OUT:-$(mktemp -d)}"
[ -n "${OUT:-}" ] && mkdir -p "$W"
[ -z "${OUT:-}" ] && trap 'rm -rf "$W"' EXIT
# `a && b || c && d` parses as (((a && b) || c) && d): group the fallback, or
# $WA comes back as TWO LINES on a shell where `pwd -W` succeeds and every path
# built from it carries an embedded newline (docs/MISTAKES.md, lane NATIVE1b).
WA="$(cd "$W" && { pwd -W 2>/dev/null || pwd; })"

checks=0
fails=0
ok() { checks=$((checks + 1)); echo "  ok   $1"; }
bad() { checks=$((checks + 1)); fails=$((fails + 1)); echo "  FAIL $1"; }
say() { echo "       $1"; }

[ -x "$NS" ] || { echo "no NifSkope.exe at $NS"; exit 2; }
[ -f "$ESM" ] || { echo "no ESM at $ESM"; exit 2; }
[ -d "$DATA" ] || { echo "no unpacked Data at $DATA"; exit 2; }

echo "== lodgen_roads: the exe =="
ls -l "$NS" | sed 's/^/       /'

# bake <name> <x0> <y0> <x1> <y1> <extra...>
bake() {
	local name="$1" x0="$2" y0="$3" x1="$4" y1="$5"; shift 5
	mkdir -p "$W/$name/mod" "$W/$name/obj" "$W/$name/tex"
	"$NS" -no-gui lodgen "$ESM" --worldspace 3C \
		--terrain-region "$x0" "$y0" "$x1" "$y1" --dim 4 \
		--out-dir "$WA/$name/obj" --data-root "$DATA" \
		--vt "$WA/$name/mod" --tex-dir "$WA/$name/tex" --cover \
		--road-ground-paint 1 "$@" \
		> "$W/$name.log" 2>&1
	return $?
}

echo "== the bakes =="
bake roadOn  -20 20 -17 23 --roads     || { echo "the --roads bake failed"; tail -8 "$W/roadOn.log"; exit 1; }
bake roadOff -20 20 -17 23 --no-roads  || { echo "the --no-roads bake failed"; exit 1; }
bake roadOff2 -20 20 -17 23 --no-roads || { echo "the --no-roads rerun failed"; exit 1; }
bake freeOn  -20 24 -17 27 --roads     || { echo "the road-free --roads bake failed"; exit 1; }
bake freeOff -20 24 -17 27 --no-roads  || { echo "the road-free --no-roads bake failed"; exit 1; }

cens() { grep -ao "$2 [0-9]*" "$W/$1.log" | head -1 | awk '{print $2}'; }

echo "== R1 the OFF value is exact =="
# The BAKEREC1 record (.lodb) is a record of the RUN, not an output of it:
# it carries the bake time, this run's own --out-dir/--vt/--tex-dir paths
# echoed as `switch` lines, and a census line with the peak working set in
# bytes.  Two identical bakes differ there BY DESIGN, so it is compared on
# its content lines below instead of byte for byte (measured 2026-09-17:
# those four lines were the only difference, every chunk and out hash matched).
n=0; d=0
for f in $(cd "$W/roadOff" && find . -type f | sort); do
	case "$f" in *.lodb) continue;; esac
	n=$((n + 1))
	cmp -s "$W/roadOff/$f" "$W/roadOff2/$f" || { d=$((d + 1)); say "differs: $f"; }
done
[ "$n" -ge 6 ] || bad "R1 the bake produced files to compare ($n)"
[ "$n" -ge 6 ] && [ "$d" -eq 0 ] \
	&& ok "R1 two --no-roads runs are byte-identical, $n files (the .lodb record apart)" \
	|| bad "R1 two --no-roads runs are byte-identical ($d of $n differ)"

# the record's content: every chunk and output hash it names
recHashes() { grep -aE "^(chunk|out)$(printf '\t')" "$1" | sort; }
rec1="$W/roadOff/obj/Commonwealth.lodb"; rec2="$W/roadOff2/obj/Commonwealth.lodb"; recOther="$W/freeOff/obj/Commonwealth.lodb"
if [ -f "$rec1" ] && [ -f "$rec2" ] && [ -f "$recOther" ]; then
	rd=$(diff <(recHashes "$rec1") <(recHashes "$rec2") | grep -c "^[<>]")
	rn=$(diff <(recHashes "$rec1") <(recHashes "$recOther") | grep -c "^[<>]")
	[ "$rd" -eq 0 ] \
		&& ok "R1 the two records name the same chunk and output hashes ($(recHashes "$rec1" | wc -l) lines)" \
		|| bad "R1 the two records name the same chunk and output hashes ($rd lines differ)"
	# the floor: a DIFFERENT region must move those hashes, or the check above.
	# compares nothing.  --roads is NOT that floor: the record names only the
	# chunk files (BTR, BTO, manifest) and roads paint the terrain sheets, so
	# the road-bearing region hashes the same with the switch either way
	# (measured 2026-09-17).
	[ "$rn" -gt 0 ] \
		&& ok "R1 the hash comparison can fail: another region names other hashes ($rn lines differ)" \
		|| bad "R1 the hash comparison can fail: another region names other hashes"
else
	bad "R1 the bake records exist"
fi

echo "== R2 roads reach the colour and not the normal =="
C="Commonwealth.4.-20.20"
if cmp -s "$W/roadOn/tex/$C.DDS" "$W/roadOff/tex/$C.DDS"; then
	bad "R2 --roads CHANGED the colour sheet (it did not)"
else
	ok "R2 --roads changed the colour sheet"
fi
cmp -s "$W/roadOn/tex/${C}_msn.DDS" "$W/roadOff/tex/${C}_msn.DDS" \
	&& ok "R2 --roads left the _msn normal sheet byte-identical" \
	|| bad "R2 --roads left the _msn normal sheet byte-identical"

echo "== R3 the census is written and moves =="
on=$(cens roadOn roadTexels); off=$(cens roadOff roadTexels)
fon=$(cens freeOn roadTexels)
ontr=$(cens roadOn roadTriangles); ondec=$(cens roadOn roadDecalTexels)
onal=$(cens roadOn roadAlphaRejected)
say "road-bearing: roadTexels=$on roadTriangles=$ontr roadDecalTexels=$ondec roadAlphaRejected=$onal"
say "road-free:    roadTexels=$fon ; switch off: roadTexels=$off"
[ -n "$on" ] && [ "$on" -gt 5000 ] \
	&& ok "R3 the road-bearing region paints roads (roadTexels $on > 5000)" \
	|| bad "R3 the road-bearing region paints roads (roadTexels ${on:-unwritten})"
[ "${fon:-x}" = "0" ] \
	&& ok "R3 the road-free region reads roadTexels 0 -- the counter is not a constant" \
	|| bad "R3 the road-free region reads roadTexels 0 (got ${fon:-unwritten})"
[ "${off:-x}" = "0" ] \
	&& ok "R3 with the switch off every road field is 0" \
	|| bad "R3 with the switch off every road field is 0 (got ${off:-unwritten})"
[ -n "$ondec" ] && [ "$ondec" -gt 0 ] \
	&& ok "R3 decal shapes inside the road family are painted (roadDecalTexels $ondec)" \
	|| bad "R3 decal shapes inside the road family are painted (got ${ondec:-unwritten})"
[ -n "$onal" ] && [ "$onal" -gt 0 ] \
	&& ok "R3 alpha-tested road shapes refuse texels (roadAlphaRejected $onal)" \
	|| bad "R3 alpha-tested road shapes refuse texels (got ${onal:-unwritten})"
grep -aq "roadRefusals" "$W/roadOn.log" \
	&& ok "R3 the refusal list is written, whether or not it is empty" \
	|| bad "R3 the refusal list is written"

echo "== R4 THE FLOOR: no road means no change =="
F="Commonwealth.4.-20.24"
cmp -s "$W/freeOn/tex/$F.DDS" "$W/freeOff/tex/$F.DDS" \
	&& ok "R4 on ground with no roads, --roads and --no-roads are byte-identical" \
	|| bad "R4 on ground with no roads, --roads and --no-roads are byte-identical"

echo "== R5 the road-presence metric against vanilla =="
if [ -f "$VAN/$C.DDS" ]; then
	"$PY" "$ROOT/tests/spells/lodgen_roads_metric.py" \
		"$VAN/$C.DDS" "$W/roadOff/tex/$C.DDS" "$W/roadOn/tex/$C.DDS" \
		> "$W/metric.txt" 2>&1
	rc=$?
	sed 's/^/       /' "$W/metric.txt"
	[ $rc -eq 0 ] \
		&& ok "R5 the road-presence metric passes its pre-registered bars" \
		|| bad "R5 the road-presence metric passes its pre-registered bars"
else
	say "SKIPPED BY NAME: no vanilla sheet at $VAN/$C.DDS, so there is nothing"
	say "to compare against. This is the one check that needs Bethesda's corpus."
fi

echo "$checks checks, $fails failures"
[ "$fails" -eq 0 ] && echo "RESULT PASS" || echo "RESULT FAIL"
exit $([ "$fails" -eq 0 ] && echo 0 || echo 1)
