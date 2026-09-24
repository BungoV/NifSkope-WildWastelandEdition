#!/bin/bash
#
# The FO4CS-native far field, end to end: the `.lodo` object library and the
# `.lodi` instance table (docs/LODGEN_NATIVE_LODO_LODI.md), v3.
#
# Thirteen legs, in the order a failure is cheapest to read:
#   1. the synthetic KNOWN-ANSWER pair, written by the exe, checked by the
#      independent Python decoder against answers the writer printed BEFORE it
#      wrote a byte (`--expect`)
#   2. two writes of that pair are byte-identical
#   3. the refusal set: one mutation per row rule and one per v2 field, every
#      CRC re-signed so the RULE and not the checksum answers
#      (tests/spells/lodgen_native_mutate.py)
#   4. a REAL region bake -- the 9-chunk Sanctuary region, dim 4 -- with
#      `--native` and `--native-mesh-report`
#   5. the same region baked WITHOUT `--native`, and every stock output file
#      compared byte for byte: the emitter may not touch the stock path
#   6. the decoder on the real pair, the ESM leg and the manifest leg
#   7. `--native-verify --native-verify-corpus`: the pair read back with every
#      check on AND the three staleness hashes recomputed from the plugin
#   8. the v2 field gate, one subsection a field, each with its floor
#      (tests/spells/lodgen_native_fields.py)
#   9. a STALENESS floor: the corpus check is shown REFUSING a doctored pair
#  10. v3, the geometry gate on the fixture: bounding spheres, normal cones,
#      the selection law's cut and its partition, the occluder box -- each with
#      a floor that must go red in the same run
#      (tests/spells/lodgen_native_cut.py)
#  11. the same geometry gate on the REAL pair
#  12. the two exact ways back, `--native-no-ladder` and `--native-no-occluders`
#  13. the occluders on a SECOND small region: measured 2026-09-11, the
#      Sanctuary region draws 41 distinct LOD meshes and none is watertight, so
#      its box count is legitimately 0 and leg 11's box gates are vacuous there
#
# It runs `-no-gui` only, so it needs no window and no display; it does not
# write anywhere but its own scratch directory.
#
# USAGE
#   bash tests/spells/lodgen_native.sh
#   OUT=<dir> bash tests/spells/lodgen_native.sh     # keep the bake

set -u

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
NS="${EXE:-$ROOT/release/NifSkope.exe}"
ESM="${ESM:-X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm}"
DATA="${DATA:-E:/Tools/Fallout 4/DataUnpacked/Data}"
PY="${PY:-$(command -v python || echo /c/Windows/py)}"
REGION="${REGION:--20 24 -9 35}"
# a second small region, for the occluders: Sanctuary has no watertight LOD mesh
OCCREGION="${OCCREGION:-0 -12 11 -1}"
KEEP="${OUT:-}"
if [ -n "$KEEP" ]; then W="$KEEP"; mkdir -p "$W"; else W="$(mktemp -d)"; trap 'rm -rf "$W"' EXIT; fi
# The CLI resolves a RELATIVE output path against release/, never the shell's
# cwd, so every path handed to it below is absolute and in WINDOWS form.
#
# The grouping braces are load-bearing. This line read
#   WA="$(cd "$W" && pwd -W 2>/dev/null || cd "$W" && pwd)"
# until 2026-09-11 (lane NATIVE1b), which the shell parses as
#   (((cd && pwd -W) || cd) && pwd)
# -- so on any shell where `pwd -W` SUCCEEDS, `pwd` runs as well and $WA comes
# back as TWO LINES. Every path built from it then carries an embedded newline,
# every output directory is a different one from the one the CLI wrote into,
# and the suite fails in nine places that all look like writer defects.
WA="$(cd "$W" && { pwd -W 2>/dev/null || pwd; })"

[ -x "$NS" ] || { echo "no NifSkope.exe at $NS"; exit 2; }
[ -f "$ESM" ] || { echo "no plugin at $ESM"; exit 2; }

checks=0
# THE PAIR MOVED (lane LAYOUT1, 2026-09-16, bungo 19:3x): a FO4CS bake
# writes its .lodo/.lodi under <mod folder>/FO4CSLOD/<ws>/, and `--native`
# names that mod folder. A RUNG exe from before the move writes them at the
# --native directory itself, so this asks the tree where the pair is rather
# than spelling a layout that depends on which exe baked it.
pairdir () {
	if [ -d "$1/FO4CSLOD/Commonwealth" ]; then echo "$1/FO4CSLOD/Commonwealth"
	else echo "$1"; fi
}

fails=0
note () { checks=$((checks+1)); echo "  ok   $1"; }
bad ()  { checks=$((checks+1)); fails=$((fails+1)); echo "  FAIL $1"; }
run ()  { "$@" > "$W/last.log" 2>&1; return $?; }

echo "== 1. the synthetic known-answer pair"
mkdir -p "$WA/fx" "$WA/fx2"
if run "$NS" -no-gui lodgen "$ESM" --native-fixture "$WA/fx"; then note "the fixture was written"; else bad "the fixture was written"; tail -5 "$W/last.log"; fi
if run "$PY" "$ROOT/tests/spells/lodgen_native_decode.py" "$WA/fx/Synthetic.lodo" "$WA/fx/Synthetic.lodi" --expect "$WA/fx/Synthetic.expect.txt"; then
	note "the independent decoder passes the known answers"
else bad "the independent decoder passes the known answers"; tail -20 "$W/last.log"; fi
grep -E "^[0-9]+ checks" "$W/last.log" | tail -1

echo "== 2. two writes are byte-identical"
run "$NS" -no-gui lodgen "$ESM" --native-fixture "$WA/fx2"
if cmp -s "$W/fx/Synthetic.lodo" "$W/fx2/Synthetic.lodo" && cmp -s "$W/fx/Synthetic.lodi" "$W/fx2/Synthetic.lodi"; then
	note "two writes of the fixture are byte-identical"
else bad "two writes of the fixture are byte-identical"; fi

echo "== 3. the refusal set"
"$PY" "$ROOT/tests/spells/lodgen_native_mutate.py" "$WA/fx" > "$W/mutate.log" 2>&1
mrc=$?
grep -E "^  (ok|FAIL)|^[0-9]+ checks" "$W/mutate.log"
if [ $mrc -eq 0 ]; then note "every mutation is refused by name"; else bad "every mutation is refused by name"; fi

echo "== 4. the real region bake (9-chunk Sanctuary, dim 4)"
mkdir -p "$WA/native/Native" "$WA/stock"
# shellcheck disable=SC2086
# `--keep-bto` (lane BTOFREE1, 2026-09-16). From today a `--native` bake builds
# its `.BTO` chunks in a scratch folder and removes them, and check 5 below is
# the question "are the STOCK outputs the same bytes with and without
# `--native`" -- a question worth asking about the chunk files themselves. So
# this arm spells the way back, check 5 keeps comparing chunk for chunk, and the
# DEFAULT (dropping) bake is what tests/spells/lodgen_btofree.sh measures.
# bungo 2026-09-17, "Authored LODs only": the ladder and the near library ship OFF.
# Sections 6-11 measure the LADDER, so this arm asks for it by switch; the shipped
# default (one authored level a mesh) is what section 12 and lodgen_defaults.sh read.
if run "$NS" -no-gui lodgen "$ESM" --worldspace 3C --terrain-region $REGION --dim 4 \
	--data-root "$DATA" --out-dir "$WA/native" --native "$WA/native/Native" \
	--keep-bto --library near --native-ladder \
	--native-mesh-report "$WA/native/mesh_report.txt"; then
	note "the region baked with --native"
else bad "the region baked with --native"; tail -5 "$W/last.log"; fi
cp "$W/last.log" "$W/bake_native.log"
grep -E "^native: |^native-ladder: |^native-occluders: |^native-casters: " "$W/bake_native.log" 
# the per-source caster counts (bungo 2026-09-11 14:4x). Two floors: the four
# bins must PARTITION the instance table, and they must not all sit in one bin.
CAST="$(grep -m1 "^native-casters: " "$W/bake_native.log")"
if [ -n "$CAST" ]; then note "the native-casters: line was printed"
else bad "the native-casters: line was printed"; fi
case "$CAST" in
	*"== AGREE;"*) note "the four caster bins sum to the instance count (the line says AGREE)" ;;
	*) bad "the four caster bins sum to the instance count: $CAST" ;;
esac
CTREE="$(printf '%s' "$CAST" | sed -n 's/.*tree \([0-9][0-9]*\),.*/\1/p')"
CMESH="$(printf '%s' "$CAST" | sed -n 's/.*, mesh \([0-9][0-9]*\),.*/\1/p')"
if [ -n "$CTREE" ] && [ "$CTREE" -gt 0 ] 2>/dev/null; then
	note "tree casters MOVE off zero on a region that has trees ($CTREE)"
else bad "tree casters MOVE off zero on a region that has trees (read '$CTREE')"; fi
if [ -n "$CMESH" ] && [ "$CMESH" -gt 0 ] 2>/dev/null; then
	note "mesh casters MOVE off zero, so the census is not all in one bin ($CMESH)"
else bad "mesh casters MOVE off zero, so the census is not all in one bin (read '$CMESH')"; fi
[ -s "$(pairdir "$W/native/Native")/Commonwealth.lodo" ] && note "a .lodo was written" || bad "a .lodo was written"
[ -s "$(pairdir "$W/native/Native")/Commonwealth.lodi" ] && note "a .lodi was written" || bad "a .lodi was written"
[ -s "$W/native/mesh_report.txt" ] && note "the per-mesh report was written" || bad "the per-mesh report was written"
# the way back, read off the disk rather than assumed: --keep-bto has to leave
# the chunks in the output folder and leave no scratch folder behind it
KB="$(find "$W/native" -maxdepth 1 -name "*.BTO" | wc -l)"
echo "    --keep-bto left $KB .BTO chunk(s) in the output folder"
[ "$KB" -gt 0 ] && note "--keep-bto leaves the .BTO chunks in the output folder ($KB)" \
	|| bad "--keep-bto leaves the .BTO chunks in the output folder (found $KB)"
[ ! -d "$W/native/lodgen_bto_scratch" ] && note "and no .BTO scratch folder is left behind" \
	|| bad "and no .BTO scratch folder is left behind"

echo "== 5. the stock path is untouched with --native off"
# shellcheck disable=SC2086
run "$NS" -no-gui lodgen "$ESM" --worldspace 3C --terrain-region $REGION --dim 4 \
	--data-root "$DATA" --out-dir "$WA/stock"
d=0; n=0
for f in "$W"/stock/*; do
	b="$(basename "$f")"
	# THE LEDGER IS NOT A STOCK OUTPUT and it is compared field by field below
	# instead. `.lodb` records the COMMAND LINE that made the bake: `switches`
	# is a sha1 over the argument vector with gLgSwitchSkip's tokens dropped,
	# `--native` is on that skip list and `--keep-bto` (which check 4 now
	# spells, so that there ARE chunks here to compare) deliberately is not.
	# The two lines therefore hash differently by design, and `cmp` can only
	# say that they do.
	case "$b" in *.lodb) continue ;; esac
	n=$((n+1))
	cmp -s "$f" "$W/native/$b" || { echo "    DIFFER $b"; d=$((d+1)); }
done
echo "    $n stock files compared, $d differ (the .lodb ledger is compared below)"
[ "$d" -eq 0 ] && [ "$n" -gt 0 ] && note "the stock bake is byte-identical with and without --native" \
	|| bad "the stock bake is byte-identical with and without --native"
# WHERE THE RECORD IS. The stock target writes it beside the bake; the FO4CS
# target writes it at FO4CSLOD/<ws>/<ws>.lodb (lane BAKEREC1, 2026-09-17). It is
# FOUND on both sides rather than spelled, so this check keeps measuring the two
# records and not the layout.
SREC="$(find "$W/stock" -name '*.lodb' | head -1)"
NREC="$(find "$W/native" -name '*.lodb' | head -1)"
echo "    stock record: ${SREC:-none}"
echo "    native record: ${NREC:-none}"
# WHAT THE TWO RECORDS MAY DISAGREE ABOUT, AND IT IS ONE THING (lane AUDIT1,
# 2026-09-17). Lane INCR1 gave the FO4CS target a per-chunk native cache,
# `<ws>.<dim>.<cx>.<cy>.lodj`, and BAKEREC1's record records it because the bake
# left it on disk. The stock target has no cache and no such row. So the two
# records legitimately name different files, `keep` went red on 2026-09-16, and
# the red was this gate rather than the bake. `--fo4cs-vs-stock` does not excuse
# the rows: it CHECKS them -- the FO4CS record must carry at least one and the
# stock record none -- and only then takes them off both sides. A bake that
# stops writing the cache still goes red here, and so does any other moved row.
if [ -n "$SREC" ] && [ -n "$NREC" ]; then
	if "$PY" "$ROOT/tests/spells/lodgen_btofree_ledger.py" keep \
		"$SREC" "$NREC" --fo4cs-vs-stock; then
		note "and the two ledgers differ ONLY in the command-line digest and the cache rows the FO4CS target alone writes"
	else bad "and the two ledgers differ ONLY in the command-line digest and the cache rows the FO4CS target alone writes"; fi
	# THE REFUTER, in this run, on these two records: move ONE recorded digest
	# that is not a cache row and the same comparison must go red. Without it
	# the check above cannot be told from a comparison that forgave everything.
	# the first recorded .BTR digest, its leading hex digit swapped between 0 and
	# 1 -- a flip to a FIXED digit is a no-op one record in sixteen, which is a
	# refuter that quietly proves nothing.
	awk -F'\t' 'BEGIN { OFS = "\t" }
		!done && $1 == "out" && $4 ~ /\.BTR$/ {
			$5 = (substr($5, 1, 1) == "0" ? "1" : "0") substr($5, 2); done = 1 }
		{ print }' "$NREC" > "$W/native_doctored.lodb"
	if cmp -s "$NREC" "$W/native_doctored.lodb"; then
		bad "THE REFUTER: the doctored record differs from the real one (nothing was doctored)"
	elif "$PY" "$ROOT/tests/spells/lodgen_btofree_ledger.py" keep \
		"$SREC" "$W/native_doctored.lodb" --fo4cs-vs-stock > "$W/doctored_ledger.log" 2>&1; then
		bad "THE REFUTER: one moved .BTR digest in the FO4CS record turns this check red"
	else
		note "THE REFUTER: one moved .BTR digest in the FO4CS record turns this check red"
	fi
else
	bad "and the two ledgers differ ONLY in the command-line digest (a .lodb is missing)"
fi

echo "== 6. the decoder on the real pair"
ARGS=""
for x in -20 -16 -12; do for y in 24 28 32; do
	m="$W/native/Commonwealth.4.$x.$y.BTO.manifest.txt"
	# only chunks the stock bake actually wrote: an empty chunk has no manifest
	[ -f "$m" ] && ARGS="$ARGS --chunk $x $y 4 --manifest $m"
done; done
# shellcheck disable=SC2086
if run "$PY" "$ROOT/tests/spells/lodgen_native_decode.py" "$(pairdir "$WA/native/Native")/Commonwealth.lodo" \
	"$(pairdir "$WA/native/Native")/Commonwealth.lodi" --esm "$ESM" --worldspace 3C $ARGS; then
	note "the decoder passes the real pair (ESM leg and manifest leg)"
else bad "the decoder passes the real pair (ESM leg and manifest leg)"; grep -E "^  FAIL" "$W/last.log" | head -10; fi
cp "$W/last.log" "$W/decode_real.log"
grep -E "^[0-9]+ checks" "$W/decode_real.log" | tail -1

echo "== 7. --native-verify with the corpus re-check"
if run "$NS" -no-gui lodgen "$ESM" --worldspace 3C --native-verify \
	"$(pairdir "$WA/native/Native")/Commonwealth.lodo" "$(pairdir "$WA/native/Native")/Commonwealth.lodi" --native-verify-corpus; then
	note "the pair verifies and its three staleness hashes still match the plugin"
else bad "the pair verifies and its three staleness hashes still match the plugin"; tail -5 "$W/last.log"; fi
cp "$W/last.log" "$W/verify_real.log"
grep -E "recomputed |distinctStockIdentities|drawKeyRanksChecked|distinctDrawKeys|pair identity" "$W/verify_real.log"

echo "== 8. the v2 fields, one subsection a field"
# shellcheck disable=SC2086
MANI=""
for x in -20 -16 -12; do for y in 24 28 32; do
	m="$W/native/Commonwealth.4.$x.$y.BTO.manifest.txt"
	[ -f "$m" ] && MANI="$MANI --manifest $m"
done; done
# shellcheck disable=SC2086
"$PY" "$ROOT/tests/spells/lodgen_native_fields.py" "$(pairdir "$WA/native/Native")/Commonwealth.lodo" \
	"$(pairdir "$WA/native/Native")/Commonwealth.lodi" --mesh-report "$WA/native/mesh_report.txt" $MANI \
	> "$W/fields.log" 2>&1
frc=$?
cat "$W/fields.log"
if [ $frc -eq 0 ]; then note "every v2 field is written and moves"; else bad "every v2 field is written and moves"; fi

echo "== 9. the staleness floor: a doctored corpus hash MUST be refused"
cp "$(pairdir "$W/native/Native")/Commonwealth.lodo" "$W/stale.lodo"
cp "$(pairdir "$W/native/Native")/Commonwealth.lodi" "$W/stale.lodi"
"$PY" - "$W/stale.lodo" "$W/stale.lodi" <<'PYEOF'
import struct, sys, zlib
def crc(b, s=0): return zlib.crc32(b, s) & 0xFFFFFFFF
def fnv(b, h=0xCBF29CE484222325):
    for c in b:
        h ^= c
        h = (h * 0x100000001B3) & 0xFFFFFFFFFFFFFFFF
    return h
lodo, lodi = sys.argv[1], sys.argv[2]
o = bytearray(open(lodo, 'rb').read())
struct.pack_into('<Q', o, 0x18, struct.unpack_from('<Q', o, 0x18)[0] ^ 1)
struct.pack_into('<I', o, 0x0C, crc(bytes(o[0x10:0x100])))
open(lodo, 'wb').write(bytes(o))
# the .lodi must still NAME this .lodo, or the pairing rule answers first and
# the staleness rule is never reached
hcrc, model, obj = struct.unpack_from('<I', o, 0x0C)[0], struct.unpack_from('<Q', o, 0x20)[0], struct.unpack_from('<Q', o, 0x18)[0]
ident = fnv(struct.pack('<IQQ', hcrc, model, obj))
i = bytearray(open(lodi, 'rb').read())
struct.pack_into('<Q', i, 0x18, obj)
struct.pack_into('<Q', i, 0x20, ident)
struct.pack_into('<I', i, 0x0C, crc(bytes(i[0x10:0x100])))
open(lodi, 'wb').write(bytes(i))
print('objectCorpusHash flipped, lodoIdentity re-derived, header CRCs re-signed')
PYEOF
if run "$NS" -no-gui lodgen "$ESM" --worldspace 3C --native-verify "$WA/stale.lodo" "$WA/stale.lodi" --native-verify-corpus; then
	bad "a stale pair is refused (it was ACCEPTED)"
else
	if grep -qi "STALE" "$W/last.log"; then note "a stale pair is refused, naming the file and the field"; else bad "a stale pair is refused BY NAME"; fi
	grep -i "REFUSED" "$W/last.log" | head -2
fi

echo "== 10. v3: the geometry gate on the fixture (bounds, cones, the cut, the occluder)"
if run "$PY" "$ROOT/tests/spells/lodgen_native_cut.py" "$WA/fx/Synthetic.lodo" "$WA/fx/Synthetic.lodi" --sample 0; then
	note "the fixture's spheres, cones, cut and occluder box all hold, with every floor red"
else bad "the fixture's spheres, cones, cut and occluder box all hold, with every floor red"; grep -E "^  FAIL" "$W/last.log" | head -10; fi
cp "$W/last.log" "$W/cut_fixture.log"
grep -E "^[0-9]+ checks|^levels " "$W/cut_fixture.log" | tail -2

echo "== 11. v3: the same geometry gate on the REAL pair"
if run "$PY" "$ROOT/tests/spells/lodgen_native_cut.py" "$(pairdir "$WA/native/Native")/Commonwealth.lodo" \
	"$(pairdir "$WA/native/Native")/Commonwealth.lodi" --sample 4000; then
	note "the real pair's spheres, cones, cut and occluder boxes hold, with every floor red"
else bad "the real pair's spheres, cones, cut and occluder boxes hold, with every floor red"; grep -E "^  FAIL" "$W/last.log" | head -10; fi
cp "$W/last.log" "$W/cut_real.log"
sed -n '/the cut, per case/,$p' "$W/cut_real.log"

echo "== 12. v3: the two exact ways back"
mkdir -p "$WA/noladder"
# shellcheck disable=SC2086
run "$NS" -no-gui lodgen "$ESM" --worldspace 3C --terrain-region $REGION --dim 4 \
	--data-root "$DATA" --out-dir "$WA/noladder" --native "$WA/noladder/Native" \
	--native-no-ladder --native-no-occluders
"$PY" - "$(pairdir "$W/noladder/Native")/Commonwealth.lodo" "$(pairdir "$W/noladder/Native")/Commonwealth.lodi" <<'PYEOF'
import struct, sys
o = open(sys.argv[1], 'rb').read()
i = open(sys.argv[2], 'rb').read()
flags = struct.unpack_from('<I', o, 8)[0]
levelMax, ladderGroup = o[0xCC], o[0xCD]
occ = struct.unpack_from('<I', i, 0xA8)[0]
bad = []
if flags & 8:
    bad.append('the LADDER flag is set with --native-no-ladder')
if levelMax or ladderGroup:
    bad.append('levelMax %d / ladderGroup %d are not 0' % (levelMax, ladderGroup))
if occ:
    bad.append('%d occluder boxes written with --native-no-occluders' % occ)
print('way-back .lodo %d bytes, .lodi %d bytes, flags 0x%x, levelMax %d, occluders %d'
      % (len(o), len(i), flags, levelMax, occ))
sys.exit(1 if bad else 0)
PYEOF
if [ $? -eq 0 ]; then note "--native-no-ladder / --native-no-occluders write one level and no boxes"
else bad "--native-no-ladder / --native-no-occluders write one level and no boxes"; fi

echo "== 13. v3: the occluders, on a region that HAS a watertight building"
# MEASURED, 2026-09-11: the nine-chunk Sanctuary region draws 41 distinct LOD
# meshes and NOT ONE of them is watertight, so its occluder count is 0 and leg
# 11's D group is vacuous there -- correct behaviour, no gate. Downtown Boston
# has the closed shells, so the box gates get their teeth from a second small
# region rather than from a loosened rule.
mkdir -p "$WA/occ/Native"
# shellcheck disable=SC2086
if run "$NS" -no-gui lodgen "$ESM" --worldspace 3C --terrain-region $OCCREGION --dim 4 \
	--data-root "$DATA" --out-dir "$WA/occ" --native "$WA/occ/Native"; then
	note "the occluder region baked"
else bad "the occluder region baked"; tail -5 "$W/last.log"; fi
cp "$W/last.log" "$W/bake_occ.log"
grep -E "^native-occluders" "$W/bake_occ.log"
if run "$PY" "$ROOT/tests/spells/lodgen_native_cut.py" "$(pairdir "$WA/occ/Native")/Commonwealth.lodo" \
	"$(pairdir "$WA/occ/Native")/Commonwealth.lodi" --sample 2000; then
	note "the occluder region's boxes are inside their meshes, and a grown box leaks"
else bad "the occluder region's boxes are inside their meshes, and a grown box leaks"; grep -E "^  FAIL" "$W/last.log" | head -6; fi
cp "$W/last.log" "$W/cut_occ.log"
grep -E "^  (ok|FAIL)   D|FLOOR a grown box" "$W/cut_occ.log"

echo "== 14. the C++ reader's own rules: the payload bounds and the aggregates"
# WHY THIS IS NOT leg 3. Leg 3 runs lodgen_native_mutate.py, and that tool asks
# the INDEPENDENT PYTHON DECODER to refuse. These four cases are aimed at rules
# that only src/lodifile.cpp and src/lodofile.cpp enforce, so the thing that has
# to answer is `lodgen --native-verify` -- the product's own reader.
mkdir -p "$W/doctor"
cp "$(pairdir "$W/native/Native")/Commonwealth.lodo" "$W/doctor/clean.lodo"
cp "$(pairdir "$W/native/Native")/Commonwealth.lodi" "$W/doctor/clean.lodi"
# THE FLOOR, first: the untouched copy is ACCEPTED. Without it every line below
# is also passed by a reader that refuses its own output.
if run "$NS" -no-gui lodgen "$ESM" --worldspace 3C \
	--native-verify "$WA/doctor/clean.lodo" "$WA/doctor/clean.lodi"; then
	note "(14 floor) the undoctored copy of the real pair is accepted"
else
	bad "(14 floor) the undoctored copy of the real pair is accepted"
	grep -i "REFUSED" "$W/last.log" | head -2
fi

# doctorCase <case> <expected substring of the refusal>
doctorCase () {
	local case="$1" want="$2"
	cp "$W/doctor/clean.lodo" "$W/doctor/$case.lodo"
	cp "$W/doctor/clean.lodi" "$W/doctor/$case.lodi"
	local said rc
	said="$("$PY" "$ROOT/tests/spells/lodgen_native_doctor.py" "$case" \
		"$W/doctor/$case.lodo" "$W/doctor/$case.lodi" 2>&1)"
	rc=$?
	echo "    $said"
	if [ $rc -eq 3 ]; then
		echo "  SKIP ($case) the bake left nothing to doctor"
		return 0
	elif [ $rc -ne 0 ]; then
		bad "($case) the doctor could not write the case"
		return 0
	fi
	if run "$NS" -no-gui lodgen "$ESM" --worldspace 3C \
		--native-verify "$WA/doctor/$case.lodo" "$WA/doctor/$case.lodi"; then
		bad "($case) the reader ACCEPTED it; it must refuse, naming \"$want\""
	elif grep -qai -- "$want" "$W/last.log"; then
		note "($case) refused by name: $(grep -ai "REFUSED" "$W/last.log" | head -1 | cut -c1-160)"
	else
		bad "($case) refused, but the message never says \"$want\""
		grep -ai "REFUSED" "$W/last.log" | head -2
	fi
}

doctorCase lodi-wrap "runs past the file"
doctorCase lodo-wrap "runs past the file"

# THE AGGREGATES are reached by CONSTRUCTION, not by a bake. `--aggregate`
# REFUSES without `--impostors <card bake tree>` (src/nifcli.cpp:3857) because
# an aggregate sheet is composited from the cell own trees card sheets, and a
# real card library is a bake of its own. It is not needed: a version-5 header
# already carries the aggregate words (src/lodifile.cpp:866..873) and
# src/lodifile.cpp:941 already builds the blob table for `v5 && aggregateCount`,
# so the doctor inserts ONE 4,096-aligned slot holding a single 48-byte record
# where the writer would have put it and re-signs every checksum. Measured
# 2026-09-17 on the exe before the fix: BOTH of these files were ACCEPTED.
doctorCase agg-views "aggregateViews"
doctorCase agg-record "HEIGHT is clear"

echo
echo "$checks checks, $fails failures"
[ $fails -eq 0 ] && echo "RESULT PASS" || echo "RESULT FAIL"
[ -n "$KEEP" ] && echo "bake kept in $W"
exit $([ $fails -eq 0 ] && echo 0 || echo 1)
