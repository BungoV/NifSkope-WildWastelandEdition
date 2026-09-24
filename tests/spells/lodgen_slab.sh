#!/bin/bash
#
# THE SLAB LATTICE -- `--terrain-object-ao`'s object term, under the law added
# by lane SLAB1 on 2026-09-18 (docs/LODGEN_TERRAIN_VT.md 2.5h).
#
# THE DEFECT IT GUARDS. The object term marched one number per 128-unit square,
# the object's MAXIMUM Z, and treated it as a wall standing on the ground. An
# elevated highway deck 1,000 units up therefore shaded the road under it as
# though the deck were a solid block reaching down to the tarmac: measured on
# chunk 4.4.-12, the mask sheet's B under the deck was 57.3 of 255 against 234.5
# on open ground beside it. The lattice now carries a MINIMUM plane as well, a
# square whose whole span stands above the sample is read as a CEILING, and a
# ceiling blocks from its nearest escape UP TO THE ZENITH instead of from the
# horizon up. `--no-terrain-object-ao-slab` is the old reading, bit for bit.
#
# WHAT IS BEING GUARDED, in the order the file checks it:
#   * the sub-toggle is INERT while the master is off -- with
#     `--terrain-object-ao` absent, adding `--no-terrain-object-ao-slab` moves
#     not one byte of any of the ten output files, and the comparison is shown
#     going RED on one flipped bit;
#   * the census word `objAoSlabSquares` exists, is WRITTEN as a zero rather
#     than omitted, and MOVES when the term is on;
#   * the mask sheet moves the right way and only there: the under-deck
#     rectangle RISES, an open-ground rectangle with no object over any of its
#     squares does not move at all, and a rectangle whose every marched square
#     is a WALL -- where the new law is algebraically the old one -- stays
#     inside the measured BC1 noise floor;
#   * the way back is a NUMBER as well as a switch: `--no-terrain-object-ao-slab`
#     reproduces the pre-lane whole-chunk mean to three decimals;
#   * and the law itself is pinned on THREE SYNTHETIC FIELDS through the shipped
#     function, in process, with the old reading refused by the same bars
#     (`WW_OBJAO_SLAB_TEST`, src/lodgen.cpp `lodgenObjectSlabSelfTest`).
#
# EVERY PINNED NUMBER IN THIS FILE was measured on chunk 4.4.-12 of the shipped
# Commonwealth with exe 2026-09-18 07:31:05 and is quoted in
# scratchpad/slab1_20260918/lane_slab1_report.md sections 4 and 5. The bars are
# loose around them on purpose -- B is BC1, so it carries 5 bits and a texel is
# one of 32 steps -- and each bar's own floor is named beside it.
#
# THE MASK IS INDEPENDENT OF THE MSN CACHE: the lane's bakes used
# `--msn-cache`, this spell does not, and the whole-chunk B mean is 158.619
# either way (measured 2026-09-18 07:34). So the pinned numbers hold without a
# path into somebody's mod folder.
#
# USAGE
#   bash tests/spells/lodgen_slab.sh
#
# Exit 2 = a missing input or an unpinned corpus; 1 = a failed check.

set -u

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
NS="${EXE:-$ROOT/release/NifSkope.exe}"
ESM="${ESM:-X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm}"
DATA="${DATA:-E:/Tools/Fallout 4/DataUnpacked/Data}"
PY="${PY:-$(command -v python || echo /c/Windows/py)}"
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

echo "== preflight =="
STALE=0
for s in src/lodgen.cpp src/lodgen.h src/nifcli.cpp; do
	if [ -f "$ROOT/$s" ] && [ ! "$NS" -nt "$ROOT/$s" ]; then
		say "exe is NOT newer than $s"
		STALE=1
	fi
done
say "exe: $(ls -l --time-style=+%Y-%m-%d\ %H:%M:%S "$NS" | awk '{print $6, $7}')"
[ $STALE -eq 0 ] && ok "the exe is newer than every source this answer depends on" \
	|| bad "the exe is newer than every source this answer depends on"

# The bars below are properties of ONE plugin set: the deck at x 19712..20992 is
# a Commonwealth placement. A different load order makes every number here
# meaningless, so the corpus is pinned exactly as lodgen_terrain_vt.sh pins it.
"$NS" -no-gui lodgen "$ESM" --worldspace 3C --corpus-hash > "$W/corpus.txt" 2>&1
VHGT="$(grep -a '^corpus vhgtCorpusHash ' "$W/corpus.txt" | awk '{print $3}')"
if [ "$VHGT" != "0xD8337D022F637F22" ]; then
	echo "  corpus hash is $VHGT, not the Commonwealth's; every floor below is a"
	echo "  property of ONE plugin set. Refusing."
	exit 2
fi
ok "the corpus is the shipped Commonwealth"

# ---------------------------------------------------------------------------
# The four bakes. Chunk 4.4.-12 (cells x 4..7, y -12..-9), dim 4, the finest
# pyramid level only, content 512 -- eight world units a texel.
# ---------------------------------------------------------------------------
bake() { # bake <name> [extra switches...]
	local name="$1"; shift
	mkdir -p "$W/$name/vt/tex"
	local t0 t1
	t0="$(date +%s)"
	WW_OBJAO_SLAB_TEST=1 "$NS" -no-gui lodgen "$ESM" \
		--worldspace 3C \
		--terrain-region 4 -12 7 -9 \
		--dim 4 \
		--data-root "$DATA" \
		--out-dir "$W/$name/vt" \
		--tex-dir "$W/$name/vt/tex" \
		--vt "$W/$name/vt" \
		--vt-finest 1 \
		--vt-content 512 \
		--road-detail 1 \
		"$@" > "$W/$name/bake.log" 2>&1
	local rc=$?
	t1="$(date +%s)"
	say "$name: rc $rc, $((t1 - t0))s"
	return $rc
}

echo "== the bakes =="
bake off                                                        || { echo "bake off failed";    exit 2; }
bake off_noslab --no-terrain-object-ao-slab                      || { echo "bake off_noslab failed"; exit 2; }
bake on         --terrain-object-ao                              || { echo "bake on failed";     exit 2; }
bake oldlaw     --terrain-object-ao --no-terrain-object-ao-slab   || { echo "bake oldlaw failed"; exit 2; }

OUTS="vt/Commonwealth.4.4.-12.BTO
vt/Commonwealth.4.4.-12.BTO.manifest.txt
vt/Commonwealth.4.4.-12.BTR
vt/FO4CSLOD/Commonwealth/Commonwealth.VT.1.lodt
vt/FO4CSLOD/Commonwealth/Commonwealth.VT.2.lodt
vt/FO4CSLOD/Commonwealth/Commonwealth.VT.4.lodt
vt/FO4CSLOD/Commonwealth/Commonwealth.VT.lodm
vt/tex/Commonwealth.4.4.-12.DDS
vt/tex/Commonwealth.4.4.-12_data.DDS
vt/tex/Commonwealth.4.4.-12_msn.DDS"

echo "== (a) the sub-toggle is inert while the master is off =="
# Not `--terrain-object-ao-slab 1` against the default: that would only prove the
# parser reads the same value twice. The pair is the DEFAULT against its
# OPPOSITE, with the master off, and the answer must be the same ten files.
same=0
total=0
for f in $OUTS; do
	total=$((total + 1))
	if cmp -s "$W/off/$f" "$W/off_noslab/$f"; then
		same=$((same + 1))
	else
		say "DIFFERS: $f"
	fi
done
say "$same of $total output files byte-identical"
[ "$same" -eq "$total" ] && ok "with --terrain-object-ao absent, --no-terrain-object-ao-slab moves not one byte of any output file" \
	|| bad "with --terrain-object-ao absent, --no-terrain-object-ao-slab moves not one byte of any output file"

# THE FLOOR FOR THAT COMPARISON. A cmp that cannot go red is not a gate: one bit
# of one byte inside the first sheet's payload, length unchanged.
cp "$W/off/vt/FO4CSLOD/Commonwealth/Commonwealth.VT.1.lodt" "$W/flip.lodt"
"$PY" - "$W/flip.lodt" <<'PYEOF'
import sys
p = sys.argv[1]
b = bytearray(open(p, 'rb').read())
b[4256] ^= 0x01
open(p, 'wb').write(bytes(b))
PYEOF
if cmp -s "$W/off/vt/FO4CSLOD/Commonwealth/Commonwealth.VT.1.lodt" "$W/flip.lodt"; then
	bad "REFUTER the same comparison goes RED on one flipped bit at offset 4256"
else
	ok "REFUTER the same comparison goes RED on one flipped bit at offset 4256"
fi

echo "== (b) the census word (docs/LODGEN_CENSUS.md) =="
cenline() { grep -a -o 'terrainObjectAo .*objAoRefusals' "$W/$1/bake.log" | head -1; }
word() { cenline "$1" | tr ' ' '\n' | grep -A1 -x "$2" | tail -1; }
for r in off on oldlaw; do
	say "$r: objAoSlab $(word "$r" objAoSlab) objAoSquares $(word "$r" objAoSquares) objAoSlabSquares $(word "$r" objAoSlabSquares)"
done
# A zero is WRITTEN and not omitted: a reader that has to tell "off" from "old
# build" needs the field present in both.
if [ "$(word off objAoSlabSquares)" = "0" ] && [ -n "$(word off objAoSlab)" ]; then
	ok "with the term off the census WRITES objAoSlabSquares 0 rather than omitting the field"
else
	bad "with the term off the census WRITES objAoSlabSquares 0 rather than omitting the field"
fi
# 13,678 of 24,729 occupied squares, reproduced independently out of the
# --dump-object-ao lattice and --dump-land in the lane report section 5.2. The
# bar is the exact number: this chunk's geometry is pinned by the corpus hash.
if [ "$(word on objAoSquares)" = "24729" ] && [ "$(word on objAoSlabSquares)" = "13678" ]; then
	ok "the census word MOVES with the term: 24729 occupied squares, 13678 of them slabs"
else
	bad "the census word MOVES with the term: 24729 occupied squares, 13678 of them slabs"
fi
# It is a property of the LATTICE, not of the march, so the old law reports it too.
if [ "$(word oldlaw objAoSlab)" = "0" ] && [ "$(word oldlaw objAoSlabSquares)" = "13678" ]; then
	ok "objAoSlab follows the switch (0 under --no-terrain-object-ao-slab) while objAoSlabSquares stays a property of the lattice"
else
	bad "objAoSlab follows the switch (0 under --no-terrain-object-ao-slab) while objAoSlabSquares stays a property of the lattice"
fi
if grep -qa 'objAoSlabSquares' "$W/on/vt/Commonwealth.lodb"; then
	ok "the bake RECORD carries objAoSlabSquares, so a .lodb says which law wrote the sheet"
else
	bad "the bake RECORD carries objAoSlabSquares, so a .lodb says which law wrote the sheet"
fi

echo "== (c) the mask sheet: what moved, what did not =="
# THE RECTANGLES, all in WORLD units, all chosen from geometry before any mask
# was opened (lane report 5.1 and 5.5):
#   a-deck   the elevated highway -- 100 lattice squares all holding max Z 2415.9
#   c-open   open ground, no object over ANY of its squares -> the control
#   e-wall   every square the march visits is a WALL under the march's own test,
#            so the new law is ALGEBRAICALLY the old one there -> the refuter
RECTS="a-deck=19712,-41856,20992,-40576 c-open=23936,-34816,24192,-34560 e-wall=24192,-34944,24704,-34432"
SHEET=vt/FO4CSLOD/Commonwealth/Commonwealth.VT.1.lodt
"$PY" "$ROOT/tests/spells/lodgen_slab_mask.py" "$W/oldlaw/$SHEET" $RECTS > "$W/m_old.txt" 2>&1
"$PY" "$ROOT/tests/spells/lodgen_slab_mask.py" "$W/on/$SHEET"     $RECTS > "$W/m_new.txt" 2>&1
sed 's/^/       /' "$W/m_new.txt" | tail -4
meanof() { # meanof <file> <name|WHOLE>
	if [ "$2" = "WHOLE" ]; then
		sed -n 's/.*WHOLE CHUNK.* mean \([0-9.]*\) .*/\1/p' "$1" | head -1
	else
		sed -n "s/^  $2 .* mean \([0-9.]*\) .*/\1/p" "$1" | head -1
	fi
}
cmpf() { "$PY" -c "import sys;a,b,op,c=sys.argv[1:5];print(1 if eval('(%s-%s) %s %s'%(b,a,op,c)) else 0)" "$@"; }
mv_report() { # mv_report <name>  -> prints "old -> new (move)"
	local o n
	o="$(meanof "$W/m_old.txt" "$1")"; n="$(meanof "$W/m_new.txt" "$1")"
	say "$1: $o -> $n  (move $("$PY" -c "print('%+.3f' % (float('$n')-float('$o')))"))"
}
for r in WHOLE a-deck c-open e-wall; do mv_report "$r"; done

# The signal. Measured +26.632 under the deck and +23.930 over the whole chunk;
# the bar is +20, which a build that ignored the flag (move 0) refuses and which
# the e-wall and c-open bars below cannot reach.
OD="$(meanof "$W/m_old.txt" a-deck)"; ND="$(meanof "$W/m_new.txt" a-deck)"
[ "$(cmpf "$OD" "$ND" '>=' 20)" = "1" ] \
	&& ok "under-deck mask B RISES by at least 20 of 255 (measured +26.632, bar +20)" \
	|| bad "under-deck mask B RISES by at least 20 of 255 (measured +26.632, bar +20)"
OW="$(meanof "$W/m_old.txt" WHOLE)"; NW="$(meanof "$W/m_new.txt" WHOLE)"
[ "$(cmpf "$OW" "$NW" '>=' 20)" = "1" ] \
	&& ok "the whole chunk's mask B rises by at least 20 of 255 (measured +23.930)" \
	|| bad "the whole chunk's mask B rises by at least 20 of 255 (measured +23.930)"

# The controls. c-open has no object over any of its squares, so the object term
# returns exactly 1.0 there by an early return and the byte cannot move: measured
# 0.000, bar 1.0 to leave BC1 one step of room.
OC="$(meanof "$W/m_old.txt" c-open)"; NC="$(meanof "$W/m_new.txt" c-open)"
[ "$("$PY" -c "print(1 if abs(float('$NC')-float('$OC')) <= 1.0 else 0)")" = "1" ] \
	&& ok "the open-ground control does not move (measured 0.000, bar 1.0)" \
	|| bad "the open-ground control does not move (measured 0.000, bar 1.0)"
# e-wall is the one that would catch a ceiling branch leaking into the wall
# branch -- ground beside occluders that REACH the ground must not brighten.
# 2.0 is not a guess: it is the lane's MEASURED BC1 re-encode floor (a provably
# unchanged texel still moves up to 2.488 when its 4x4 block's neighbours
# change), and the wall population moved less than that unchanged population.
OE="$(meanof "$W/m_old.txt" e-wall)"; NE="$(meanof "$W/m_new.txt" e-wall)"
[ "$("$PY" -c "print(1 if abs(float('$NE')-float('$OE')) <= 2.0 else 0)")" = "1" ] \
	&& ok "the all-wall refuter stays inside the BC1 noise floor (measured +0.060, bar 2.0)" \
	|| bad "the all-wall refuter stays inside the BC1 noise floor (measured +0.060, bar 2.0)"

# THE WAY BACK, AS A NUMBER. 134.689 is the whole-chunk mean the rung wrote
# (release/NifSkope.exe of 2026-09-18 05:18:11) and the same ten files came out
# byte-identical under --no-terrain-object-ao-slab (lane report 4.2). Pinning the
# mean here keeps that true on a machine with no rung exe left.
[ "$("$PY" -c "print(1 if abs(float('$OW')-134.689) <= 0.002 else 0)")" = "1" ] \
	&& ok "--no-terrain-object-ao-slab reproduces the pre-lane whole-chunk mean 134.689" \
	|| bad "--no-terrain-object-ao-slab reproduces the pre-lane whole-chunk mean 134.689 (got $OW)"

echo "== (d) the law itself, on three synthetic fields, in process =="
# WW_OBJAO_SLAB_TEST builds a PLATE (a ceiling at H over everything), a WALL (a
# span from -4096 up to H, reaching the ground) and an EDGE (the plate over a
# half plane) by hand, and reads all three through the SHIPPED
# `lodgenObjectSkyVis` -- not a second copy of the formula. Every bar is also
# evaluated with slab = false, and the plate bars must FAIL there.
sed -n 's/^slab:/       slab:/p' "$W/on/bake.log"
SLABN="$(grep -a -c '^slab:   ok' "$W/on/bake.log")"
if grep -qa '^slab: self-test 12 checks, 0 failures, RESULT PASS' "$W/on/bake.log"; then
	ok "the in-process law control passes: 12 checks, 0 failures (plate 0.525468 > 0.5, wall identical to 0, edge 0.703417 between)"
else
	bad "the in-process law control passes: 12 checks, 0 failures"
fi
if grep -qa 'REFUTER PLATE H=1000 under the OLD law against the same bar' "$W/on/bake.log"; then
	ok "that control carries its own refuter: the OLD reading of the same plate is 0.290780 and the same bar REFUSES it"
else
	bad "that control carries its own refuter: the OLD reading of the same plate is 0.290780 and the same bar REFUSES it"
fi
if grep -qa 'slab=false reproduces the old formula exactly on all three fields' "$W/on/bake.log"; then
	ok "slab = false is the old formula to the LAST BIT on all three fields (float equality, not a tolerance)"
else
	bad "slab = false is the old formula to the LAST BIT on all three fields (float equality, not a tolerance)"
fi
say "in-process ok lines: $SLABN"

echo
echo "$checks checks, $fails failures"
# COUNT FLOOR: measured, never predicted (ww-test-harness-add 5c). The first
# green run, 2026-09-18 07:37 on exe 07:31:05, read 16 checks -- and the number
# I had written here by counting the blocks by eye was 14, which is exactly the
# mistake that rule exists for. The floor is the measured 16.
if [ "$checks" -lt 16 ]; then
	echo "RESULT FAIL -- only $checks checks ran, floor 16: the suite stopped early"
	exit 1
fi
[ $fails -eq 0 ] && echo "RESULT PASS" || echo "RESULT FAIL"
[ $fails -eq 0 ]
