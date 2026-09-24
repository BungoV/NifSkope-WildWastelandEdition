#!/bin/bash
# tests/spells/lodgen_layout.sh -- lane LAYOUT1, 2026-09-16.
#
# THE QUESTION. bungo, 2026-09-16 19:2x: "shouldn't all these files sit under a
# new single directory then?", and 19:3x: "The folder should be called FO4CSLOD
# maybe, so it'd be Data/FO4CSLOD, sound fine?".  From today every FO4CS-target
# output of a bake lands under ONE root inside the output mod folder:
#
#     FO4CSLOD/<ws>/<ws>.lodl            the landscape file
#     FO4CSLOD/<ws>/<ws>.VT.<dim>.lodt   the terrain virtual texture, per level
#     FO4CSLOD/<ws>/<ws>.VT.lodm         its index
#     FO4CSLOD/<ws>/<ws>.lodo + .lodi    the native object library and instances
#     FO4CSLOD/<ws>/Aggregate/...        the ring-3 aggregate impostor sheets
#     FO4CSLOD/<ws>/Objects/...          the mesh arrays, atlas and card arrays
#     FO4CSLOD/<ws>/*.BTO.manifest.txt   the sidecars BTOFREE1 keeps
#     FO4CSLOD/Cards/<id>_oct.*          the impostor cards (per TREE, shared)
#     FO4CSLOD/<ws>/<ws>.lodb            the bake record (lane BAKEREC1, 2026-09-17)
#
# WHAT STAYS PUT, and why each one is named rather than swept:
#     Textures/Terrain/<ws>/*.HeightMap.*  a SHIPPED FO4CS reader composes that
#                                          path from loose files; moving it is a
#                                          later FO4CS change, bungo's call
#     meshes/terrain/<ws>/, textures/terrain/<ws>/   the STOCK target's engine
#                                          paths; not one byte of them moved
#     --keep-bto's .BTO chunks             exactly where BTOFREE1 left them
#     <out>/<ws>.lodb                      MOVED 2026-09-17: the bake record is
#                                          FO4CSLOD/<ws>/<ws>.lodb under the FO4CS
#                                          target now (lane BAKEREC1) and one left
#                                          at the old spot is a stray
#     <out>/<ws>.<dim>.<x>.<y>.BTR         a legacy terrain chunk a --native run
#                                          still writes; a legacy type, not this
#                                          lane's to move
#     the --tex-dir the operator NAMED     the legacy chunk sheets that go with
#                                          the .BTR, written where asked
#
# THE LEGS
#   (a) a FO4CS bake on the fixture chunk puts every expected file at its new
#       path and leaves NOTHING of ours outside FO4CSLOD/ but the exemptions
#       above, each counted by name.  REFUTER: the rung exe's own bake is put
#       through the same test and must FAIL it on every row.
#   (b) byte identity across the move: the rung's files and ours differ ONLY in
#       the game-relative path strings written inside them (lodgen_layout_diff.py),
#       with the no-rewrite run as the refuter.
#   (c) the stock target is untouched: whole-tree byte identity against the rung
#       at dim 4, 8, 16 and 32 on the same chunk.
#   (d) --keep-bto still puts the .BTO chunks where BTOFREE1 left them, and the
#       manifest beside the files it describes under the new root.
#   (e) no second spelling in the source: the old spellings survive only as READ
#       paths and the HeightMap writer, listed line by line here so a new one
#       fails this leg.
#   (f) the census names the root, read back from the paths actually written,
#       and that folder exists on disk with that many files under it.
#
# USAGE
#   bash tests/spells/lodgen_layout.sh
#   RUNG=release/NifSkope.before_layout1.exe   the exe the bytes are pinned to
#   EXE=...  the exe under test   REGION="-20 24 -20 24"   DIM=4   OUT=<dir>
set -u

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
NS="${EXE:-$ROOT/release/NifSkope.exe}"
RUNG="${RUNG:-$ROOT/release/NifSkope.before_layout1.exe}"
ESM="${ESM:-X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm}"
DATA="${DATA:-E:/Tools/Fallout 4/DataUnpacked/Data}"
CARDS="${CARDS:-$ROOT/scratchpad/showcase1_20260912/cards}"
REGION="${REGION:--20 24 -20 24}"
DIM="${DIM:-4}"
WS="${WS:-Commonwealth}"
PY="${PY:-$(command -v python || echo /c/Windows/py)}"
DIFF="$ROOT/tests/spells/lodgen_layout_diff.py"
KEEP="${OUT:-}"
if [ -n "$KEEP" ]; then W="$KEEP"; mkdir -p "$W"; else W="$(mktemp -d)"; trap 'rm -rf "$W"' EXIT; fi
# the grouping braces are load-bearing -- see the note in lodgen_native.sh
WA="$(cd "$W" && { pwd -W 2>/dev/null || pwd; })"
FO4="FO4CSLOD/$WS"

[ -x "$NS" ] || { echo "no NifSkope.exe at $NS"; exit 2; }
[ -f "$ESM" ] || { echo "no plugin at $ESM"; exit 2; }
if tasklist 2>/dev/null | grep -qi "Fallout4.exe"; then
	echo "REFUSED: Fallout4.exe is running"; exit 2
fi

checks=0
fails=0
note () { checks=$((checks+1)); echo "  ok   $1"; }
bad ()  { checks=$((checks+1)); fails=$((fails+1)); echo "  FAIL $1"; }
skip () { echo "  SKIP $1"; }

date
echo "exe   : $NS ($(stat -c%s "$NS") bytes, $(stat -c%y "$NS" | cut -c1-19))"
HAVE_RUNG=0
if [ -x "$RUNG" ]; then
	HAVE_RUNG=1
	echo "rung  : $RUNG ($(stat -c%s "$RUNG") bytes, $(stat -c%y "$RUNG" | cut -c1-19))"
else
	echo "rung  : $RUNG -- NOT ON DISK, the byte legs will skip"
fi
echo "region: $REGION dim $DIM, worldspace $WS"
HAVE_CARDS=0
[ -d "$CARDS" ] && HAVE_CARDS=1
echo "cards : $CARDS ($([ "$HAVE_CARDS" = 1 ] && echo "$(ls "$CARDS" | wc -l) file(s)" || echo "absent -- the card path strings are not exercised"))"

# bake <exe> <name> [--stock] [extra switches...]
bake () {
	local exe="$1" name="$2"; shift 2
	local stock=0
	# bungo 2026-09-17, "Authored LODs only": the ladder and the near library ship
	# OFF. The rung exes predate that and cannot take the switch, so the exe under
	# test is asked for the rung's old default by name and the bytes stay comparable.
	local eq=""
	[ "$exe" != "$RUNG" ] && eq="--library near --native-ladder"
	if [ "${1:-}" = "--stock" ]; then stock=1; shift; fi
	mkdir -p "$WA/$name" "$WA/$name/tex"
	local nat="" imp=""
	# `--native` NAMES A MOD FOLDER since this lane: the out-dir IS the mod
	[ "$stock" = "0" ] && nat="--native $WA/$name"
	[ "$stock" = "0" ] && [ "$HAVE_CARDS" = "1" ] && imp="--impostors $CARDS --impostors-from-level 0"
	# shellcheck disable=SC2086
	"$exe" -no-gui lodgen "$ESM" --worldspace 3C \
		--terrain-region $REGION --dim "$DIM" --data-root "$DATA" \
		--out-dir "$WA/$name" --tex-dir "$WA/$name/tex" $nat $imp \
		--cover --arrays --road-detail 1 $eq "$@" \
		> "$W/$name.log" 2>&1
	local rc=$?
	echo "  $name: rc=$rc, $(find "$W/$name" -type f | wc -l) file(s)"
	return $rc
}

# stray <dir> -- prints every file under <dir> that is NOT under FO4CSLOD/ and
# is not one of the named exemptions. One line each, so a failure says WHICH.
#
# `.lodb` was an exemption until 2026-09-17 and is not one any more: lane
# BAKEREC1 moved the bake record to FO4CSLOD/<ws>/<ws>.lodb, so a record left
# at the old spot is a stray and this leg is what says so.
stray () {
	( cd "$1" && find . -type f | sed 's|^\./||' | sort ) | while read -r rel; do
		case "$rel" in
			FO4CSLOD/*) continue ;;
			Textures/Terrain/*HeightMap*|textures/terrain/*HeightMap*) continue ;;
			tex/*) continue ;;                     # the --tex-dir the operator named
			*.BTR) continue ;;                     # a legacy chunk type, not ours to move
			*.BTO) continue ;;                     # --keep-bto's way back
		esac
		echo "$rel"
	done
}

# ============================================================================
echo
echo "== (a) the FO4CS bake puts every file under the one root =="
bake "$NS" new || bad "(a) the bake runs"
WANT="$FO4/$WS.lodo
$FO4/$WS.lodi
$FO4/$WS.lodb
$FO4/$WS.$DIM.$(echo "$REGION" | cut -d' ' -f1).$(echo "$REGION" | cut -d' ' -f2).BTO.manifest.txt
$FO4/Objects/$WS.LodgenArrays.txt"
n_have=0; n_want=0
while read -r rel; do
	[ -z "$rel" ] && continue
	n_want=$((n_want+1))
	if [ -f "$W/new/$rel" ]; then n_have=$((n_have+1)); else echo "    MISSING: $rel"; fi
done <<EOF
$WANT
EOF
[ "$n_have" = "$n_want" ] && note "(a) every expected file is at its new path ($n_have/$n_want)" \
	|| bad "(a) every expected file is at its new path ($n_have/$n_want)"

STRAY="$(stray "$W/new")"
NSTRAY="$(printf '%s' "$STRAY" | grep -c . || true)"
if [ "$NSTRAY" = "0" ]; then
	note "(a) nothing of ours outside FO4CSLOD/ but the named exemptions"
else
	bad "(a) nothing of ours outside FO4CSLOD/ but the named exemptions ($NSTRAY stray)"
	echo "$STRAY" | sed 's/^/    stray: /'
fi
echo "    the exemptions, counted by name:"
for pat in '*.BTR' '*.BTO'; do
	c="$(find "$W/new" -name "$pat" -not -path '*/tex/*' | wc -l)"
	echo "      $pat: $c"
done
echo "      the named --tex-dir: $(find "$W/new/tex" -type f 2>/dev/null | wc -l) file(s)"
# EMPTY-FOLDER HYGIENE: a bake creates no folder it does not fill.
EMPTY="$(find "$W/new" -type d -empty | sed "s|$W/new/*||" | grep -v '^$' || true)"
if [ -z "$EMPTY" ]; then note "(a) the bake left no empty folder behind"
else bad "(a) the bake left no empty folder behind"; echo "$EMPTY" | sed 's/^/    empty: /'; fi
for d in Terrain Textures/Lodgen meshes; do
	if [ -e "$W/new/$d" ]; then bad "(a) no $d/ folder is created at all"
	else note "(a) no $d/ folder is created at all"; fi
done

# THE LEDGER STILL POINTS AT REAL FILES. The ledger is lane BAKEREC1's and this
# lane writes nothing into its new home, but it RECORDS the outputs of a chunk
# by path and digest -- and the sidecar it records is one of the files that
# moved. Pointing it at a path nothing writes does not fail loudly: the entry
# simply keeps an EMPTY digest, and every later --incremental run silently
# rebakes. This row is what caught exactly that (2026-09-16), so it stays.
LEDGER="$(find "$W/new" -name '*.lodb' | head -1)"
# The record's output paths are relative to the RECORD'S OWN folder, which is
# FO4CSLOD/<ws>/ since 2026-09-17 and was the out-dir before it. Resolving them
# against the out-dir would report every row as NOT ON DISK.
LEDDIR="$(dirname "$LEDGER" 2>/dev/null || echo "$W/new")"
if [ -n "$LEDGER" ]; then
	if "$PY" - "$LEDGER" "$LEDDIR" "$ROOT/tests/spells" <<'LEDEOF'
import hashlib, os, sys
# THE ONE READER (lane BAKEREC1, 2026-09-17), in place of this leg's own copy
# of the v1 binary container's parser.
sys.path.insert(0, sys.argv[3])
import lodb_read
led, out = sys.argv[1], sys.argv[2]
doc = lodb_read.read(led)
ok = bad = 0
for ch in doc.get('chunks', []):
    for entry in ch.get('out', []):
        rel, _, want = entry.rpartition(' ')
        full = rel if os.path.isabs(rel) else os.path.join(out, rel)
        if not want:
            print('      NO DIGEST: %s' % rel); bad += 1; continue
        if not os.path.isfile(full):
            print('      NOT ON DISK: %s' % rel); bad += 1; continue
        got = hashlib.sha1(open(full, 'rb').read()).hexdigest()
        if got != want:
            print('      DIGEST MISMATCH: %s' % rel); bad += 1
        else:
            ok += 1
print('      ledger: %d entry(ies) resolve and match, %d do not' % (ok, bad))
sys.exit(1 if bad or not ok else 0)
LEDEOF
	then note "(a) every output the ledger records is on disk with the digest it recorded"
	else bad "(a) every output the ledger records is on disk with the digest it recorded"; fi
else
	skip "(a) the ledger leg: no .lodb in the bake"
fi

if [ "$HAVE_RUNG" = "1" ]; then
	echo "    THE REFUTER: the same test on the rung exe's own bake --"
	bake "$RUNG" rung_a >/dev/null 2>&1
	RSTRAY="$(stray "$W/rung_a")"
	NR="$(printf '%s' "$RSTRAY" | grep -c . || true)"
	RUNDER="$(find "$W/rung_a/FO4CSLOD" -type f 2>/dev/null | wc -l)"
	echo "      rung: $NR file(s) outside FO4CSLOD/, $RUNDER under it"
	if [ "$NR" -gt 0 ] && [ "$RUNDER" = "0" ]; then
		note "(a) the refuter fires: the rung's bake fails this leg on every row"
	else
		bad "(a) the refuter fires: the rung's bake fails this leg on every row"
	fi
else
	skip "(a) the refuter: no rung exe on disk"
fi

# ============================================================================
echo
echo "== (b) the bytes across the move: only the path strings changed =="
if [ "$HAVE_RUNG" = "1" ]; then
	# the rung wrote the OLD layout; --native named a plain directory then, so
	# its pair sits at the out-dir root and its object sets under --tex-dir.
	X0="$(echo "$REGION" | cut -d' ' -f1)"; Y0="$(echo "$REGION" | cut -d' ' -f2)"
	PAIRS=""
	addpair () { PAIRS="$PAIRS --pair $1 $2"; }
	addpair "$W/rung_a/$WS.lodo"                       "$W/new/$FO4/$WS.lodo"
	addpair "$W/rung_a/$WS.lodi"                       "$W/new/$FO4/$WS.lodi"
	addpair "$W/rung_a/$WS.$DIM.$X0.$Y0.BTO.manifest.txt" "$W/new/$FO4/$WS.$DIM.$X0.$Y0.BTO.manifest.txt"
	addpair "$W/rung_a/$WS.$DIM.$X0.$Y0.BTR"           "$W/new/$WS.$DIM.$X0.$Y0.BTR"
	for f in $( ( cd "$W/new/$FO4/Objects" 2>/dev/null && ls ) || true ); do
		addpair "$W/rung_a/tex/Objects/$f" "$W/new/$FO4/Objects/$f"
	done
	for f in $( ( cd "$W/new/tex" 2>/dev/null && ls ) || true ); do
		addpair "$W/rung_a/tex/$f" "$W/new/tex/$f"
	done
	# shellcheck disable=SC2086
	if "$PY" "$DIFF" --ws "$WS" $PAIRS; then
		note "(b) every file is byte-identical once the path strings are rewritten"
	else
		bad "(b) every file is byte-identical once the path strings are rewritten"
	fi
	echo "    THE REFUTER: the same comparison WITHOUT the rewrite --"
	# shellcheck disable=SC2086
	if "$PY" "$DIFF" --ws "$WS" --quiet --no-rewrite $PAIRS; then
		note "(b) the refuter fires: without the rewrite the path-carrying files differ"
	else
		bad "(b) the refuter fires: without the rewrite the path-carrying files differ"
	fi
else
	skip "(b) the byte legs: no rung exe on disk"
fi


# reccmp <old.lodb> <new.lodb> -- the bake record compared on its SUBSTANCE.
#
# The record cannot be byte-compared across two exes: its header line is
#     lodb<TAB>2<TAB><ws><TAB><exe version><TAB><exe size in bytes>
# so it moves whenever the linker does. This drops that line and the `census`
# lines (wall-clock timings) from lodbNormalise()-equivalent text and compares
# what is left, which is every claim the bake made about the files on disk.
# FLOOR: at least one `chunk` row and one `out` row, or the comparison is
# vacuous -- two empty files are equal -- and it fails.
reccmp() {
	"$PY" - "$1" "$2" "$ROOT/tests/spells" <<'PYEOF'
import sys
sys.path.insert(0, sys.argv[3])
import lodb_read

V2 = ("lodb" + chr(9)).encode("utf-8")


def substance(path):
    """The record minus everything that cannot survive a relink.

    Dropped: the `lodb` header line, whose last two fields are the exe
    version and the exe SIZE IN BYTES, and the `census` lines, which are
    wall-clock timings. What is left is every claim the bake made about
    the files it wrote.
    """
    text = lodb_read.normalise_file(path)
    return [l for l in text.split(chr(10))
            if l != "" and not l.startswith("census" + chr(9))
            and not l.startswith("lodb" + chr(9))]


for p in (sys.argv[1], sys.argv[2]):
    with open(p, "rb") as fh:
        head = fh.read(5)
    if not head.startswith(V2):
        print("      NOT COMPARABLE: %s is the v1 binary container"
              " (magic %r), the rung exe predates the plain-text record."
              % (p.split("/")[-1], head[:4]))
        print("      The record is still measured, on this exe, by"
              " tests/spells/lodgen_bakerec.sh and by lodgen_incremental.sh"
              " arm (d). It is not measured here.")
        sys.exit(3)

a, b = substance(sys.argv[1]), substance(sys.argv[2])
chunks = sum(1 for l in a if l.startswith("chunk" + chr(9)))
outs = sum(1 for l in a if l.startswith("out" + chr(9)))
print("      the record is excepted from the bytes (its header stamps the"
      " exe version and size); substance: %d line(s), %d chunk row(s),"
      " %d out row(s)" % (len(a), chunks, outs))
if chunks < 1 or outs < 1:
    print("      VACUOUS: no chunk/out rows to compare")
    sys.exit(2)
if a == b:
    sys.exit(0)
for i in range(min(len(a), len(b))):
    if a[i] != b[i]:
        print("      first difference at line %d:" % (i + 1))
        print("        rung: %s" % a[i][:160])
        print("        ours: %s" % b[i][:160])
        break
if len(a) != len(b):
    print("      %d line(s) on the rung side, %d on ours" % (len(a), len(b)))
sys.exit(1)
PYEOF
}
# ============================================================================
echo
echo "== (c) the stock target is untouched, dim 4/8/16/32 =="
if [ "$HAVE_RUNG" = "1" ]; then
	for d in 4 8 16 32; do
		DIM="$d" bake "$NS"   "stock_new_$d" --stock >/dev/null 2>&1
		DIM="$d" bake "$RUNG" "stock_old_$d" --stock >/dev/null 2>&1
		same=0; diff=0; only=0; excused=0; notcmp=0
		for rel in $( cd "$W/stock_old_$d" && find . -type f | sed 's|^\./||' | sort ); do
			if [ ! -f "$W/stock_new_$d/$rel" ]; then only=$((only+1)); echo "    only in the rung's: $rel"
			elif [ "${rel%.lodb}" != "$rel" ]; then
				# THE ONE EXCEPTION, and it says so out loud. See reccmp() above:
				# the record stamps the exe's version AND SIZE, so byte identity
				# here would only ever measure that the binary grew. Its substance
				# is compared instead, and that comparison has its own floor.
				excused=$((excused+1))
				reccmp "$W/stock_old_$d/$rel" "$W/stock_new_$d/$rel"; rrc=$?
				if [ "$rrc" = "0" ]; then same=$((same+1))
				elif [ "$rrc" = "3" ]; then notcmp=$((notcmp+1))
				else diff=$((diff+1)); echo "    DIFFERS in SUBSTANCE: $rel"; fi
			elif cmp -s "$W/stock_old_$d/$rel" "$W/stock_new_$d/$rel"; then same=$((same+1))
			else diff=$((diff+1)); echo "    DIFFERS: $rel"; fi
		done
		for rel in $( cd "$W/stock_new_$d" && find . -type f | sed 's|^\./||' | sort ); do
			[ -f "$W/stock_old_$d/$rel" ] || { only=$((only+1)); echo "    only in ours: $rel"; }
		done
		if [ "$diff" = "0" ] && [ "$only" = "0" ] && [ "$same" -gt 0 ]; then
			note "(c) dim $d: the whole stock tree matches the rung's ($same files; $excused record(s) excepted from the bytes, $notcmp of those not comparable at all)"
		else
			bad "(c) dim $d: the stock tree does not match the rung's ($diff differ, $only on one side only, $excused excepted)"
		fi
	done
	DIM="${DIM:-4}"
else
	skip "(c) the stock target: no rung exe on disk"
fi

# ============================================================================
echo
echo "== (d) --keep-bto keeps its chunks where BTOFREE1 left them =="
bake "$NS" keep --keep-bto >/dev/null 2>&1
KB="$(find "$W/keep" -name '*.BTO' | wc -l)"
KBROOT="$(find "$W/keep" -maxdepth 1 -name '*.BTO' | wc -l)"
[ "$KB" -gt 0 ] && [ "$KB" = "$KBROOT" ] \
	&& note "(d) the .BTO chunks are still written outside the new root ($KB at the out-dir root)" \
	|| bad "(d) the .BTO chunks are still written outside the new root ($KB found, $KBROOT at the root)"
[ ! -d "$W/keep/lodgen_bto_scratch" ] && note "(d) and --keep-bto still creates no scratch folder" \
	|| bad "(d) and --keep-bto still creates no scratch folder"
# THE SIDECAR FOLLOWS ITS CHUNK, which is the whole point of calling it a
# sidecar. With the scratch folder the `.BTO` does not survive, so the
# sidecar lands under the one root beside the files that DID survive; with
# --keep-bto the chunk stays at the out-dir root and its sidecar stays next
# to it, because --keep-bto is the way back to the old tree BYTE FOR BYTE
# (lane BTOFREE1) and a sidecar in another folder is not that tree. BOTH
# sides are counted, so a build that moved it under the root anyway fails.
KMR="$(find "$W/keep" -maxdepth 1 -name '*.BTO.manifest.txt' 2>/dev/null | wc -l)"
KMF="$(find "$W/keep/$FO4" -name '*.BTO.manifest.txt' 2>/dev/null | wc -l)"
[ "$KMR" = "$KB" ] && [ "$KMF" = "0" ] \
	&& note "(d) each kept chunk still has its sidecar beside it ($KMR at the out-dir root, 0 under $FO4/)" \
	|| bad "(d) each kept chunk still has its sidecar beside it ($KMR beside $KB chunk(s), $KMF under $FO4/)"

# ============================================================================
echo
echo "== (e) one spelling in the source, and the old ones are READ paths =="
# Every line the old spellings may still appear on, by file and reason. A new
# writer that spells a path itself lands outside this list and fails the leg.
HITS="$(cd "$ROOT" && grep -rn 'Textures[/\\]Lodgen\|"/Terrain"\|/Terrain/\|Terrain\\\\%1' src/ --include='*.cpp' --include='*.h' || true)"
echo "$HITS" | sed 's/^/    /'
# Each exclusion is a REASON, not a sweep, and the list printed above is
# what a reviewer reads:
#   * a comment -- the line is prose about a path and writes nothing
#   * the HeightMap -- the far heightmap DDS did NOT move, by the ruling,
#     and both its writer and the panel row that reports it name the path
BAD="$(echo "$HITS" | grep -v '^\s*$' \
	| grep -v ':[0-9]*:[[:space:]]*[*/]' \
	| grep -v 'HeightMap' \
	| grep -v 'Textures/Terrain/" ) + edid' \
	| grep -v 'g_vanillaLodRoot' \
	| grep -v '^src/[a-z]*\.h:' \
	| grep -v 'data..Textures..Terrain..%1..Objects' \
	|| true)"
NBAD="$(printf '%s' "$BAD" | grep -c . || true)"
if [ "$NBAD" = "0" ]; then
	note "(e) every remaining hit is a READ path, a comment or the HeightMap writer"
else
	bad "(e) every remaining hit is a READ path, a comment or the HeightMap writer ($NBAD unaccounted)"
	echo "$BAD" | sed 's/^/    unaccounted: /'
fi
# the root itself is spelled ONCE, in the one function that composes it
SPELL="$(cd "$ROOT" && grep -rln '"FO4CSLOD"' src/ || true)"
if [ "$SPELL" = "src/lodgenlayout.cpp" ]; then
	note "(e) the folder name is a string literal in ONE file (src/lodgenlayout.cpp)"
else
	bad "(e) the folder name is a string literal in ONE file (found: $(echo "$SPELL" | tr '\n' ' '))"
fi

# ============================================================================
echo
echo "== (f) the census names the root it actually wrote to =="
CL="$(grep -o 'layout [^,]*, [0-9]* file(s), [0-9]* outside' "$W/new.log" | tail -1)"
echo "    $CL"
CROOT="$(echo "$CL" | sed 's/^layout //; s/, [0-9]* file(s).*//')"
CN="$(echo "$CL" | sed 's/.*, \([0-9]*\) file(s).*/\1/')"
COUT="$(echo "$CL" | sed 's/.*, \([0-9]*\) outside/\1/')"
if [ -n "$CROOT" ] && [ -d "$CROOT" ]; then
	note "(f) the census root is a folder that exists ($CROOT)"
else
	bad "(f) the census root is a folder that exists (read '$CROOT')"
fi
# The census line is composed BEFORE the bake record is written, so the
# record cannot be in the number it carries. That is the law the format
# states for the same count on the `end` line (docs/LODGEN_BAKE_RECORD.md
# section 2.8: "Counted by walking the record's own directory tree at write
# time, the record itself excepted (it is not written yet)"), so the count on
# THIS side of the comparison excepts it too -- and says how many it took
# out, so the exception cannot quietly swallow a second file.
ON_DISK_ALL="$(find "$W/new/FO4CSLOD" -type f 2>/dev/null | wc -l)"
RECS="$(find "$W/new/FO4CSLOD" -type f -name '*.lodb' 2>/dev/null | wc -l)"
ON_DISK=$((ON_DISK_ALL - RECS))
if [ "$RECS" -gt 1 ]; then
	bad "(f) exactly one bake record under the root (found $RECS)"
else
	note "(f) $RECS bake record excepted from the disk count (section 2.8: it is not written when the census line is composed)"
fi
if [ "$CN" = "$ON_DISK" ]; then
	note "(f) it counts what is on disk ($CN file(s), $ON_DISK_ALL on disk less $RECS record)"
else
	bad "(f) it counts what is on disk (census $CN, on disk $ON_DISK_ALL less $RECS record = $ON_DISK)"
fi
[ "$COUT" = "0" ] && note "(f) and reports 0 files written outside it" \
	|| bad "(f) and reports 0 files written outside it (says $COUT)"

# ============================================================================
echo
echo "layout checks: $checks, failures: $fails"
[ -n "$KEEP" ] && echo "trees kept under $W"
[ "$fails" -eq 0 ] && { echo "RESULT PASS"; exit 0; }
echo "RESULT FAIL"
exit 1
