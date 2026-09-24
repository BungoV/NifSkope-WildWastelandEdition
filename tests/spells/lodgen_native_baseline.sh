#!/bin/bash
#
# LANE 0 of the FO4CS-native migration (spec_fo4cs_native.md 9, docs/LODGEN_NATIVE_LODO_LODI.md):
# the STOCK object bake that every native lane must leave byte-identical, as a
# checked-in file of hashes from ONE NAMED BUILD -- not a fresh bake from a tree
# ten lanes are editing.
#
#   --write     bake the region set with the named exe and write
#               tests/baselines/stock_baseline.sha256 (header: the exe's sha256 and
#               mtime, git describe, the profile; then one `sha256  name` line per
#               output file, sorted by name)
#   --check     re-bake the same set into a scratch dir and compare hash for hash;
#               prints EVERY file that differs, is missing or is new; exit 1 on any
#   --selftest  prove the comparator can fail without baking: a copy of the
#               baseline with one hex digit flipped must name exactly that file
#   --drop-proof  plan 5 row 13: bake (-32,0) dim 32 ONCE with --slot-fallback --native
#               and count the placements the stock chunk drops against the ones the
#               native pair keeps (the block above the region set says how)
#
# The region set reaches the paths a dim-4 bake never touches (spec 9):
#   (-20,24) dim 4            the ordinary case, the identity harness's chunk
#   (-24,24) dim 8            the first fallback ring
#   (-32,16) dim 16 --slot-fallback   the fork's filled far ring
#   (-32,0)  dim 32           THE BUCKET-CAP CHUNK (2,628 of 42,560 placements dropped today)
#   region (-20,24)..(-19,25) dim 4 with --arrays --atlas   the texture-array, atlas and
#                             manifest writers (the .BTR of that run is NOT hashed: terrain
#                             has its own lanes and its own gates)
# NOT HASHED, for the same reason: the `.lodb` incremental ledger every bake now
# writes beside its output (docs/LODGEN_LEDGER_FORMAT.md). It is bookkeeping, not a
# stock object-bake output, and it has its own gates -- header/sort/relative-path/
# determinism, and a byte-for-byte comparison against a full bake that includes it
# by name. Hashing it here would turn every ledger version bump into a red about
# the stock vertex writer. Added 2026-09-12 (LAND1), the day the file first existed.
# Profile: identity on (the default), AO on unless AO=0, --data-root the unpacked corpus.
# The profile is written into the header and --check refuses a baseline made under another.
#
# THE MUTATION TEST THE SPEC ASKS FOR (flip one constant in the stock vertex writer,
# rebuild, --check must name the file) needs a rebuild and is a director step; --selftest
# is the half that runs without one. Record both in this header when they have run:
#   selftest: <date> <result>      writer-mutation: <date> <constant> <files named>
#
# USAGE
#   bash tests/spells/lodgen_native_baseline.sh --write|--check|--selftest|--drop-proof
#   EXE=release/NifSkope.exe ESM=... DATA=... AO=1 BASE=tests/baselines/stock_baseline.sha256

set -u

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
NS="${EXE:-$ROOT/release/NifSkope.exe}"
ESM="${ESM:-X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm}"
DATA="${DATA:-E:/Tools/Fallout 4/DataUnpacked/Data}"
AO="${AO:-1}"
PY="${PY:-$(command -v python || echo /c/Windows/py)}"
BASE="${BASE:-$ROOT/tests/baselines/stock_baseline.sha256}"
MODE="${1:---check}"

fails=0
ok() { echo "  ok   $1"; }
bad() { echo "  FAIL $1"; fails=$((fails + 1)); }

# -------------------------------------------------------------- the comparator
# compare <baseline> <hashlist>: both are `sha256  name` lines (header lines start with #).
compare() {
	"$PY" - "$1" "$2" <<'PYEOF'
import sys
def load(p):
    d = {}
    for l in open(p, encoding='utf-8'):
        if l.startswith('#') or not l.strip():
            continue
        h, n = l.rstrip('\n').split('  ', 1)
        d[n] = h
    return d
a = load(sys.argv[1])
b = load(sys.argv[2])
diff = []
for n in sorted(set(a) | set(b)):
    if n not in b:
        diff.append('MISSING  ' + n)
    elif n not in a:
        diff.append('NEW      ' + n)
    elif a[n] != b[n]:
        diff.append('CHANGED  ' + n)
for d in diff:
    print('  ' + d)
print('%d files in the baseline, %d baked, %d differ' % (len(a), len(b), len(diff)))
sys.exit(1 if diff else 0)
PYEOF
}

# hashlist <dir> : sha256 of every file under dir except *.BTR and *.LODB, sorted by relative name
hashlist() {
	"$PY" - "$1" <<'PYEOF'
import hashlib, os, sys
root = sys.argv[1]
rows = []
for dp, dn, fn in os.walk(root):
    for f in fn:
        if f.upper().endswith('.BTR') or f.upper().endswith('.LODB'):
            continue
        p = os.path.join(dp, f)
        rel = os.path.relpath(p, root).replace(os.sep, '/')
        rows.append((rel, hashlib.sha256(open(p, 'rb').read()).hexdigest()))
for rel, h in sorted(rows):
    print('%s  %s' % (h, rel))
PYEOF
}

if [ "$MODE" = "--selftest" ]; then
	[ -f "$BASE" ] || { echo "no baseline at $BASE (run --write first)"; echo "RESULT FAIL"; exit 1; }
	W="$(mktemp -d)"; trap 'rm -rf "$W"' EXIT
	grep -v '^#' "$BASE" > "$W/clean.txt"
	first="$(head -1 "$W/clean.txt")"
	name="${first#*  }"
	# flip the first hex digit of the first hash
	"$PY" - "$W/clean.txt" "$W/flip.txt" <<'PYEOF'
import sys
lines = open(sys.argv[1], encoding='utf-8').read().split('\n')
h = lines[0]
c = h[0]
lines[0] = ('0' if c != '0' else '1') + h[1:]
open(sys.argv[2], 'w', encoding='utf-8').write('\n'.join(lines))
PYEOF
	out="$(compare "$BASE" "$W/flip.txt")"
	echo "$out"
	if echo "$out" | grep -q "CHANGED  $name" && echo "$out" | grep -q ", 1 differ"; then
		ok "a one-digit flip names exactly one file: $name"
	else
		bad "the comparator did not name the flipped file"
	fi
	if compare "$BASE" "$W/clean.txt" >/dev/null; then
		ok "the baseline compared to itself: 0 differ"
	else
		bad "the baseline differs from itself"
	fi
	echo "$fails failures"; echo "RESULT $([ $fails -eq 0 ] && echo PASS || echo FAIL)"; exit $fails
fi

[ -x "$NS" ] || { echo "no NifSkope.exe at $NS"; exit 2; }
[ -f "$ESM" ] || { echo "no ESM at $ESM"; exit 2; }
if tasklist //FI "IMAGENAME eq Fallout4.exe" 2>/dev/null | grep -q Fallout4.exe; then
	echo "GAME UP: Fallout4.exe is running; no bake"; exit 3
fi

W="$(mktemp -d)"; trap 'rm -rf "$W"' EXIT
mkdir -p "$W/out/chunks" "$W/out/region"
AOFLAG=""; [ "$AO" = "1" ] || AOFLAG="--no-ao"

# ------------------------------------------------ --drop-proof (plan 5 row 13)
# THE ASYMMETRIC-DROP PROOF ON THE NATIVE PATH (lane INCRGATE1, 2026-09-24).
# docs/LODGEN_NATIVE_LODO_LODI.md 9 says the stock (-32,0) dim-32 chunk writes
# no geometry at all for 2,628 of its 42,560 placements (6.17%), because a
# bucket's 16-bit index domain fills, and that the native format cannot drop
# one by construction. Both halves measured from ONE bake of that chunk:
#   --slot-fallback --identity --keep-bto --native
# so the stock chunk and the native pair come from the same placements.
#   stock   manifest rows whose identity index is on no `i` line of
#           `lodgen --dump-geometry` (tests/spells/lodgen_drop_count.py) > 0
#   native  the decoder finds EVERY manifest row in the .lodi (--manifest),
#           instanceCount = the manifest's rows, and --native-verify reads
#           instancesWithNeitherGeometryNorCard 0
# RED CONTROLS: the stock count IS the first one (a stock side that dropped
# nothing makes the proof vacuous and fails); the second is the same decoder
# check on a copy of the manifest with one row whose ref is in no table -- it
# must fail by name, or "every row is in the table" could not see a drop.
if [ "$MODE" = "--drop-proof" ]; then
	D="$W/drop"; DW="$(cd "$W" && { pwd -W 2>/dev/null || pwd; })/drop"
	mkdir -p "$D/Native"
	t0=$(date +%s)
	"$NS" -no-gui lodgen "$ESM" --worldspace 3C --terrain-region -32 0 -1 31 --dim 32 $AOFLAG \
		--slot-fallback --identity --keep-bto --data-root "$DATA" --out-dir "$DW" --native "$DW/Native" \
		> "$W/bake_drop.log" 2>&1 || { bad "the (-32,0) dim-32 bake failed: $(tail -2 "$W/bake_drop.log")"; }
	echo "  bake wall-clock $(( $(date +%s) - t0 )) s"
	bto="$(find "$D" -maxdepth 1 -name "Commonwealth.32.-32.0.BTO" | head -1)"
	man="$bto.manifest.txt"
	pd="$D/Native"; [ -d "$pd/FO4CSLOD/Commonwealth" ] && pd="$pd/FO4CSLOD/Commonwealth"
	if [ -z "$bto" ] || [ ! -s "$man" ] || [ ! -s "$pd/Commonwealth.lodi" ]; then
		bad "the bake left the chunk, its manifest and the pair (bto '$bto', pair dir $pd)"
		echo "$fails failures"; echo "RESULT FAIL"; exit 1
	fi
	"$NS" -no-gui lodgen --dump-geometry "$bto" > "$W/geom.txt" 2>&1
	"$PY" "$ROOT/tests/spells/lodgen_drop_count.py" "$man" "$W/geom.txt" | tee "$W/drop.txt"
	N="$(sed -n 's/^drop.placements \([0-9]*\).*/\1/p' "$W/drop.txt")"
	M="$(sed -n 's/^drop.dropped \([0-9]*\).*/\1/p' "$W/drop.txt")"
	[ -n "$M" ] && [ "$M" -gt 0 ] \
		&& ok "(red control) the stock chunk drops $M of $N placements: the comparator sees a drop" \
		|| bad "(red control) the stock chunk dropped ${M:-?} of ${N:-?}: the proof has nothing to prove"
	"$PY" "$ROOT/tests/spells/lodgen_native_decode.py" "$pd/Commonwealth.lodo" "$pd/Commonwealth.lodi" \
		--manifest "$man" > "$W/decode.txt" 2>&1; rc=$?
	grep -E "^lodi.instanceCount|manifest .*rows|^(REFUSED|[0-9]+ checks)" "$W/decode.txt"
	IC="$(sed -n 's/^lodi.instanceCount \([0-9]*\).*/\1/p' "$W/decode.txt")"
	[ $rc -eq 0 ] && ok "the native pair holds every manifest row (decoder --manifest, rc 0)" \
		|| bad "the decoder refused the native pair against the manifest (rc $rc)"
	[ -n "$IC" ] && [ "$IC" = "$N" ] && ok "instanceCount $IC = the manifest's $N placements: 0 dropped natively" \
		|| bad "instanceCount ${IC:-?} against the manifest's ${N:-?} placements"
	"$NS" -no-gui lodgen "$ESM" --worldspace 3C --native-verify "$pd/Commonwealth.lodo" "$pd/Commonwealth.lodi" \
		> "$W/verify.txt" 2>&1; rc=$?
	grep -E "instancesWith|REFUSED" "$W/verify.txt" | head -3
	[ $rc -eq 0 ] && grep -q "instancesWithNeitherGeometryNorCard 0" "$W/verify.txt" \
		&& ok "--native-verify: every instance has geometry or a card" \
		|| bad "--native-verify did not read instancesWithNeitherGeometryNorCard 0 (rc $rc)"
	cp "$man" "$W/doctored.manifest.txt"
	echo "999999 00000000 STAT 0 0 0 1 building 0 ffffffff 0" >> "$W/doctored.manifest.txt"
	"$PY" "$ROOT/tests/spells/lodgen_native_decode.py" "$pd/Commonwealth.lodo" "$pd/Commonwealth.lodi" \
		--manifest "$W/doctored.manifest.txt" > "$W/decode_red.txt" 2>&1; rc=$?
	[ $rc -ne 0 ] && grep -q "FAIL.*manifest: every row is in the table" "$W/decode_red.txt" \
		&& ok "(red control) one row in no table fails \"every row is in the table\" by name" \
		|| bad "(red control) a manifest row in no table was not caught (rc $rc)"
	echo "  exe $NS $(date -r "$NS" +%Y-%m-%dT%H:%M:%S)"
	echo "$fails failures"; echo "RESULT $([ $fails -eq 0 ] && echo PASS || echo FAIL)"; exit $fails
fi

bake() {	# x y dim extra out
	# --identity is SPELLED: the profile header below says identity=1 and the
	# checked-in baseline was made that way, but identity became opt-in on
	# 2026-09-12 (bungo's ruling).
	"$NS" -no-gui lodgen "$ESM" --worldspace 3C --objects "$1" "$2" --dim "$3" $AOFLAG --identity $4 \
		--data-root "$DATA" -o "$5" > "$W/bake_$1_$2_$3.log" 2>&1 || { bad "bake ($1,$2) dim $3 failed: $(tail -2 "$W/bake_$1_$2_$3.log")"; return 1; }
	[ -s "$5" ] && [ -s "$5.manifest.txt" ] || { bad "bake ($1,$2) dim $3 wrote no chunk or manifest"; return 1; }
}
t0=$(date +%s)
bake -20 24 4  ""               "$W/out/chunks/Commonwealth.4.-20.24.bto"
bake -24 24 8  ""               "$W/out/chunks/Commonwealth.8.-24.24.bto"
bake -32 16 16 "--slot-fallback" "$W/out/chunks/Commonwealth.16.-32.16.bto"
bake -32 0  32 ""               "$W/out/chunks/Commonwealth.32.-32.0.bto"
"$NS" -no-gui lodgen "$ESM" --worldspace 3C --terrain-region -20 24 -19 25 --dim 4 $AOFLAG --identity \
	--arrays --atlas --data-root "$DATA" --out-dir "$W/out/region" > "$W/bake_region.log" 2>&1 \
	|| bad "region bake failed: $(tail -2 "$W/bake_region.log")"
ls "$W/out/region"/*.BTO >/dev/null 2>&1 || bad "region bake wrote no .BTO"
ls "$W/out/region"/Objects/* >/dev/null 2>&1 || bad "region bake wrote no Objects/ (arrays + atlas)"
t1=$(date +%s)
echo "  bake wall-clock $((t1 - t0)) s (AO=$AO)"
[ $fails -eq 0 ] || { echo "RESULT FAIL"; exit 1; }

hashlist "$W/out" > "$W/hashes.txt"
n=$(wc -l < "$W/hashes.txt")
[ "$n" -ge 8 ] && ok "$n output files hashed (floor 8: 4 chunks + 4 manifests)" || bad "only $n files hashed"

exesha="$("$PY" -c "import hashlib,sys;print(hashlib.sha256(open(sys.argv[1],'rb').read()).hexdigest())" "$NS")"
exemtime="$(date -r "$NS" +%Y-%m-%dT%H:%M:%S)"
gitdesc="$(cd "$ROOT" && git describe --always --dirty 2>/dev/null || echo unknown)"
profile="ao=$AO identity=1 arrays=1 atlas=1 slot-fallback=dim16 exclude=BTR"   # exclude=BTR+LODB since 2026-09-12; the string is NOT changed, or every checked-in baseline is refused

if [ "$MODE" = "--write" ]; then
	mkdir -p "$(dirname "$BASE")"
	{
		echo "# stock_baseline 1  (tests/spells/lodgen_native_baseline.sh --write)"
		echo "# exe $exesha $exemtime $NS"
		echo "# git $gitdesc"
		echo "# profile $profile"
		echo "# bake-seconds $((t1 - t0))"
		echo "# region (-20,24)d4 (-24,24)d8 (-32,16)d16+slot-fallback (-32,0)d32 region(-20,24..-19,25)d4+arrays+atlas"
		cat "$W/hashes.txt"
	} > "$BASE"
	ok "wrote $BASE ($n files, exe $exesha)"
	echo "$fails failures"; echo "RESULT $([ $fails -eq 0 ] && echo PASS || echo FAIL)"; exit $fails
fi

# --check
[ -f "$BASE" ] || { bad "no baseline at $BASE (run --write first)"; echo "RESULT FAIL"; exit 1; }
bp="$(grep '^# profile ' "$BASE" | sed 's/^# profile //')"
[ "$bp" = "$profile" ] && ok "profile matches the baseline ($profile)" || bad "profile differs: baseline '$bp', this run '$profile'"
if compare "$BASE" "$W/hashes.txt"; then
	ok "every stock file byte-identical to the baseline"
else
	bad "the stock bake differs from the baseline (files named above)"
fi
echo "  baseline exe $(grep '^# exe ' "$BASE" | cut -d' ' -f3,4); this exe $exesha $exemtime"
echo "$fails failures"; echo "RESULT $([ $fails -eq 0 ] && echo PASS || echo FAIL)"; exit $fails
