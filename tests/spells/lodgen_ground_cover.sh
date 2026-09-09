#!/bin/bash
#
# Ground cover in the far terrain, under docs/LODGEN_TERRAIN_VT.md §2 and §3:
# the LTEX -> GNAM -> GRAS chain composited against the cell's splat paint,
# gated by terrain slope, written into the ALPHA of <ws>.<dim>.<x>.<y>_data.DDS
# (which turns BC3 and gains a 'WWCV' stamp only where a chunk has grass), and
# a matching grass tint mixed into the far albedo after the VCLR multiply.
#
# WHAT IS BEING GUARDED, in order of how badly it would hurt:
#
#   1. With the feature OFF, not one byte moves. Two --no-cover bakes of the
#      same chunk are byte-identical, each file is the size it has always been,
#      and the data sheet is still DXT1. When a frozen baseline exists
#      (tests/spells/lodgen_ground_cover.sha256) the three hashes must match it
#      as well; without one the check FAILS and says so, because "no byte
#      moved" measured against a bake of the build under test is not a check.
#   2. With the feature ON over ground that has no grass, still not one byte -
#      AND the census line must prove the cover pass actually RAN, or the cmp
#      passed because the code never executed.
#   3. The value itself, against an INDEPENDENT model: a Python reader that
#      parses the ESM's paint and grass records itself (lodgen_cover_model.py)
#      rather than asking the CLI, because a round trip that shares one table
#      with the code it tests proves nothing.
#   4. Cover is not something already shipped: |r| against AO, wetness, shore,
#      the model-space normal's Z and the slope angle itself.
#
# USAGE
#   bash tests/spells/lodgen_ground_cover.sh
#   ESM=... DATA=... EXE=... bash tests/spells/lodgen_ground_cover.sh
#
# Exit 2 = a missing input or an unpinned corpus; 1 = a failed check.

set -u

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
NS="${EXE:-$ROOT/release/NifSkope.exe}"
ESM="${ESM:-X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm}"
DATA="${DATA:-E:/Tools/Fallout 4/DataUnpacked/Data}"
PY="${PY:-$(command -v python || echo /c/Windows/py)}"
CACHE="${CACHE:-${TMPDIR:-/tmp}/ww_lodgen_cover_cache}"
W="$(mktemp -d)"
trap 'rm -rf "$W"' EXIT

checks=0
fails=0
ok() { checks=$((checks + 1)); echo "  ok   $1"; }
bad() { checks=$((checks + 1)); fails=$((fails + 1)); echo "  FAIL $1"; }
say() { echo "       $1"; }

[ -x "$NS" ] || { echo "no NifSkope.exe at $NS"; exit 2; }
[ -f "$ESM" ] || { echo "no ESM at $ESM"; exit 2; }
[ -d "$DATA" ] || { echo "no unpacked Data at $DATA"; exit 2; }
mkdir -p "$CACHE"

echo "== C0 preflight =="
# The exe must be newer than every source the answer depends on. A pyramid or a
# cover plane built by a stale exe from fresh sources is docs/MISTAKES.md's
# oldest entry in new clothes.
STALE=0
for s in src/lodgen.cpp src/lodgen.h src/esmdata.cpp src/esmdata.h src/nifcli.cpp \
		src/io/lodvfile.cpp src/io/lodmfile.cpp src/lodgenmanager.cpp; do
	if [ -f "$ROOT/$s" ] && [ ! "$NS" -nt "$ROOT/$s" ]; then
		say "exe is NOT newer than $s"
		STALE=1
	fi
done
say "exe: $(ls -l --time-style=+%Y-%m-%d\ %H:%M:%S "$NS" | awk '{print $6, $7}')"
[ $STALE -eq 0 ] && ok "C0 the exe is newer than every source this answer depends on" \
	|| bad "C0 the exe is newer than every source this answer depends on"

say "ESM: $(stat -c %s "$ESM") bytes"
"$NS" -no-gui lodgen "$ESM" --worldspace 3C --corpus-hash > "$W/corpus.txt" 2>&1
grep -a "^corpus " "$W/corpus.txt" | sed 's/^/       /'
VHGT="$(grep -a '^corpus vhgtCorpusHash ' "$W/corpus.txt" | awk '{print $3}')"
if [ "$VHGT" != "0xD8337D022F637F22" ]; then
	echo "  corpus hash is $VHGT, not the Commonwealth's 0xD8337D022F637F22:"
	echo "  every floor below is a property of ONE plugin set. Refusing."
	exit 2
fi
ok "C0 the corpus is the shipped Commonwealth (VHGT hash $VHGT)"

# the independent walk, cached: it is slow and it does not change
if [ ! -s "$CACHE/census.json" ]; then
	"$PY" "$ROOT/tests/spells/lodgen_cover_model.py" census "$ESM" > "$CACHE/census.json" \
		|| { echo "the python census failed"; exit 2; }
fi
say "census (python): $(head -c 200 "$CACHE/census.json")"

echo "== bakes =="
# (-20,24) Sanctuary at dim 4: 98.9% of the Commonwealth's grass is an
# ALPHA-layer phenomenon, and this chunk's paint is alpha layers.
bake() {   # bake <outname> <x0> <y0> <x1> <y1> <extra...>
	local name="$1"; shift
	local x0="$1" y0="$2" x1="$3" y1="$4"; shift 4
	mkdir -p "$W/$name/obj" "$W/$name/tex"
	"$NS" -no-gui lodgen "$ESM" --worldspace 3C \
		--terrain-region "$x0" "$y0" "$x1" "$y1" --dim 4 \
		--out-dir "$W/$name/obj" --tex-dir "$W/$name/tex" --data-root "$DATA" \
		"$@" > "$W/$name.log" 2>&1
	return $?
}

bake plain -20 24 -17 27 --no-cover || { echo "the --no-cover bake failed"; tail -5 "$W/plain.log"; exit 1; }
bake plain2 -20 24 -17 27 --no-cover || { echo "the second --no-cover bake failed"; exit 1; }
bake cover -20 24 -17 27 --cover --dump-cover "$W/cover.raw" \
	|| { echo "the --cover bake failed"; tail -5 "$W/cover.log"; exit 1; }
bake tint0 -20 24 -17 27 --cover --grass-tint 0 || { echo "the --grass-tint 0 bake failed"; exit 1; }
bake ocean0 0 0 3 3 --no-cover || { echo "the (0,0) --no-cover bake failed"; exit 1; }
bake ocean1 0 0 3 3 --cover || { echo "the (0,0) --cover bake failed"; exit 1; }

STEM="Commonwealth.4.-20.24"
OSTEM="Commonwealth.4.0.0"

echo "== C1 the feature off moves nothing =="
same=1
for f in "$STEM.DDS" "$STEM""_msn.DDS" "$STEM""_data.DDS"; do
	cmp -s "$W/plain/tex/$f" "$W/plain2/tex/$f" || same=0
done
[ $same -eq 1 ] && ok "C1 two --no-cover bakes are byte-identical" \
	|| bad "C1 two --no-cover bakes are byte-identical"

sizes_ok=1
for f in "$STEM.DDS" "$STEM""_msn.DDS" "$STEM""_data.DDS"; do
	sz="$(stat -c %s "$W/plain/tex/$f")"
	say "$f: $sz bytes"
	[ "$sz" = "174888" ] || sizes_ok=0
done
[ $sizes_ok -eq 1 ] && ok "C1 every --no-cover sheet is 174,888 bytes" \
	|| bad "C1 every --no-cover sheet is 174,888 bytes"

BASE="$ROOT/tests/spells/lodgen_ground_cover.sha256"
for f in "$STEM.DDS" "$STEM""_msn.DDS" "$STEM""_data.DDS"; do
	printf '%s  %s\n' "$(sha256sum "$W/plain/tex/$f" | awk '{print $1}')" "$f"
done > "$W/measured.sha256"
sed 's/^/       /' "$W/measured.sha256"
if [ -s "$BASE" ]; then
	if diff -q <(grep -v '^#' "$BASE") "$W/measured.sha256" >/dev/null; then
		ok "C1 the --no-cover bake matches the frozen baseline"
	else
		bad "C1 the --no-cover bake matches the frozen baseline"
		diff <(grep -v '^#' "$BASE") "$W/measured.sha256" | sed 's/^/       /'
	fi
else
	bad "C1 a frozen baseline exists to compare against"
	say "no $BASE. Record the three hashes above ONCE, from a build that"
	say "lodgen_identity.sh has passed, or this check is a bake compared with itself."
fi

differs=0
for f in "$STEM.DDS" "$STEM""_data.DDS"; do
	cmp -s "$W/plain/tex/$f" "$W/cover/tex/$f" || differs=1
done
[ $differs -eq 1 ] && ok "C1 the --cover bake does NOT match it (the comparison can fail)" \
	|| bad "C1 the --cover bake does NOT match it (the comparison can fail)"

# --grass-tint 0 writes the plane and leaves the albedo alone: the setting for
# an FO4CS-only user who wants the data and no stock-engine colour change
if cmp -s "$W/plain/tex/$STEM.DDS" "$W/tint0/tex/$STEM.DDS"; then
	ok "C1 --grass-tint 0 keeps the albedo byte-identical"
else
	bad "C1 --grass-tint 0 keeps the albedo byte-identical"
fi
T0="$("$PY" -c "import sys;print(open(sys.argv[1],'rb').read()[84:88].decode('latin-1'))" "$W/tint0/tex/$STEM""_data.DDS")"
[ "$T0" = "DXT5" ] && ok "C1 and still writes the cover plane (DXT5)" \
	|| bad "C1 and still writes the cover plane (fourCC $T0)"

echo "== C2 the feature on over grass-free ground moves nothing =="
clean=1
for f in "$OSTEM.DDS" "$OSTEM""_msn.DDS" "$OSTEM""_data.DDS"; do
	cmp -s "$W/ocean0/tex/$f" "$W/ocean1/tex/$f" || clean=0
done
CENSUS="$(grep -a '^cover ' "$W/ocean1.log" | head -1)"
say "census: ${CENSUS:-<absent>}"
kw() { echo "$CENSUS" | tr ' ' '\n' | grep "^$1=" | cut -d= -f2- ; }
if [ -z "$CENSUS" ]; then
	bad "C2 the cover pass ran at all (its census line is present)"
else
	ok "C2 the cover pass ran at all (its census line is present)"
	[ "$(kw texels)" = "262144" ] && ok "C2 the census counted the whole 512x512" \
		|| bad "C2 the census counted the whole 512x512 (texels=$(kw texels))"
	[ "$(kw coverMax)" = "0" ] && ok "C2 this ground has no cover (coverMax=0)" \
		|| bad "C2 this ground has no cover (coverMax=$(kw coverMax))"
	[ "$(kw ltexResolves)" != "0" ] && ok "C2 and the LTEX set it resolved is not empty" \
		|| bad "C2 and the LTEX set it resolved is not empty"
fi
[ $clean -eq 1 ] && ok "C2 all three grass-free sheets are byte-identical with --cover" \
	|| bad "C2 all three grass-free sheets are byte-identical with --cover"
FOURCC="$("$PY" -c "import sys;print(open(sys.argv[1],'rb').read()[84:88].decode('latin-1'))" "$W/ocean1/tex/$OSTEM""_data.DDS")"
[ "$FOURCC" = "DXT1" ] && ok "C2 a grass-free chunk stays as cheap as it has always been (DXT1)" \
	|| bad "C2 a grass-free chunk stays DXT1 (fourCC $FOURCC)"

echo "== C12 the renormalising texels =="
if [ ! -s "$CACHE/renorm.json" ]; then
	"$PY" "$ROOT/tests/spells/lodgen_cover_model.py" renorm "$ESM" > "$CACHE/renorm.json" 2>/dev/null
fi
say "worldspace scan: $(cat "$CACHE/renorm.json" 2>/dev/null || echo '<not run>')"
RCELL="$("$PY" -c "
import json,sys
try: d=json.load(open(sys.argv[1]))
except Exception: print(''); raise SystemExit
print('%d %d %d' % (d['cell'][0], d['cell'][1], d['points']) if d.get('cell') else '')
" "$CACHE/renorm.json" 2>/dev/null)"
if [ -n "$RCELL" ]; then
	set -- $RCELL
	RX=$(( $1 - ($1 % 4 + 4) % 4 )); RY=$(( $2 - ($2 % 4 + 4) % 4 )); RN=$3
	say "the paint sums past 1 on $RN grid points in cell ($1,$2); baking chunk ($RX,$RY)"
	bake renormA "$RX" "$RY" $((RX + 3)) $((RY + 3)) --no-cover
	bake renormB "$RX" "$RY" $((RX + 3)) $((RY + 3)) --cover --grass-tint 0
	RSTEM="Commonwealth.4.$RX.$RY"
	if cmp -s "$W/renormA/tex/$RSTEM.DDS" "$W/renormB/tex/$RSTEM.DDS"; then
		ok "C12 the cover-side renormalisation does not reach the colour composite"
	else
		bad "C12 the cover-side renormalisation does not reach the colour composite"
	fi
	RF="$("$PY" -c "import sys;print(open(sys.argv[1],'rb').read()[84:88].decode('latin-1'))" "$W/renormB/tex/$RSTEM""_data.DDS")"
	[ "$RF" = "DXT5" ] && ok "C12 and that fixture really does carry cover (DXT5)" \
		|| bad "C12 and that fixture really does carry cover (fourCC $RF)"
else
	bad "C12 a fixture containing an A>1 texel was found"
	say "the worldspace scan found none, so nothing here can expose a renormalisation leak"
fi

echo "== C13 the census accuses itself =="
CENSUS="$(grep -a '^cover ' "$W/cover.log" | head -1)"
say "${CENSUS:-<absent>}"
if [ -z "$CENSUS" ]; then
	bad "C13 the cover census line is present under --cover"
else
	ok "C13 the cover census line is present under --cover"
	err0=1
	for k in danglingLtex danglingGnam clipPainted clipBase; do
		v="$(kw $k)"
		say "$k=$v"
		[ "$v" = "0" ] || err0=0
	done
	[ $err0 -eq 1 ] && ok "C13 every ERROR counter is zero on vanilla" \
		|| bad "C13 every ERROR counter is zero on vanilla"
	case "$(kw ltexNoGnam)" in
		*/*) ok "C13 ltexNoGnam is printed with its denominator, not gated" ;;
		*) bad "C13 ltexNoGnam is printed with its denominator, not gated" ;;
	esac
	case "$(kw grasNoTint)" in
		*/*) ok "C13 grasNoTint is printed with its denominator, not gated" ;;
		*) bad "C13 grasNoTint is printed with its denominator, not gated" ;;
	esac
	# C15: COUNTS, not wall clock. A per-texel plugin or texture lookup fails
	# here deterministically instead of hiding inside a 24-second parse.
	QUADS="$(kw quadrants)"
	say "grasReads=$(kw grasReads) nifReads=$(kw nifReads) texLoads=$(kw texLoads) ltexResolves=$(kw ltexResolves)/$((QUADS * 8))"
	if [ "$(kw grasReads)" -le 153 ] && [ "$(kw nifReads)" -le 71 ] \
		&& [ "$(kw texLoads)" -le 64 ] && [ "$(kw ltexResolves)" -le $((QUADS * 8)) ]; then
		ok "C15 the resolves are per quadrant and per form, not per texel"
	else
		bad "C15 the resolves are per quadrant and per form, not per texel"
	fi
	# C14, the half the C++ can state about itself; the python half is in the checker
	say "corpus (bake): ltexTotal=$(kw ltexTotal) grasTotal=$(kw grasTotal) gnamLinks=$(kw gnamLinks) ltexWithGnam=$(kw ltexWithGnam) grasData=$(kw grasDataMin)..$(kw grasDataMax)"
	PYC="$CACHE/census.json"
	for k in ltexTotal grasTotal gnamLinks ltexWithGnam grasDataMin grasDataMax; do
		pv="$("$PY" -c "import json,sys;print(json.load(open(sys.argv[1]))[sys.argv[2]])" "$PYC" "$k")"
		if [ "$(kw $k)" = "$pv" ]; then
			ok "C14 $k agrees across two independent readers ($pv)"
		else
			bad "C14 $k: the bake says $(kw $k), the python walk says $pv"
		fi
	done
fi

echo "== the model, and the measurements on the files =="
MODEL="$CACHE/model.-20.24.json"
[ -s "$MODEL" ] || "$PY" "$ROOT/tests/spells/lodgen_cover_model.py" cover "$ESM" -20 24 4 96 "$MODEL" \
	|| { echo "the python model failed"; exit 2; }
"$PY" -c "
import json,sys
d=json.load(open(sys.argv[1]))
print('       fixture: %d of 16 cells carry LAND; LTEX %s; NULL layers %d of %d; dominant base %s'
      % (d['cells'], ','.join(d['ltexEdids']), d['nullLayers'], d['alphaLayers'], d['dominantBase']))
print('       dangling LTEX in the fixture: %s' % (','.join(d['danglingLtex']) or 'none'))
sys.exit(0 if d['cells']==16 and len(d['ltexEdids'])>=4 and d['alphaLayers']>0 else 1)
" "$MODEL" && ok "C0 the fixture's premises hold (16/16 cells, a real alpha-layer paint)" \
	|| bad "C0 the fixture's premises hold (16/16 cells, a real alpha-layer paint)"

MODEL2="$CACHE/model.0.0.json"
[ -s "$MODEL2" ] || "$PY" "$ROOT/tests/spells/lodgen_cover_model.py" cover "$ESM" 0 0 4 96 "$MODEL2" >/dev/null 2>&1

"$PY" "$ROOT/tests/spells/lodgen_cover_check.py" \
	"$W/cover/tex/$STEM.DDS" "$W/plain/tex/$STEM.DDS" \
	"$W/cover/tex/$STEM""_data.DDS" "$W/plain/tex/$STEM""_data.DDS" \
	"$W/cover/tex/$STEM""_msn.DDS" "$W/cover.raw" \
	"$MODEL" "$MODEL2" "$CACHE/census.json"
RC=$?
checks=$((checks + 1))
if [ $RC -eq 0 ]; then
	echo "  ok   C3..C17 the measurements on the files"
else
	fails=$((fails + 1))
	echo "  FAIL C3..C17 the measurements on the files"
fi

echo
echo "$checks checks, $fails failures"
[ $fails -eq 0 ] && echo "RESULT PASS" || echo "RESULT FAIL"
[ $fails -eq 0 ]
