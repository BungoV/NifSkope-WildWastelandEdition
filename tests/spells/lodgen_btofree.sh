#!/bin/bash
# tests/spells/lodgen_btofree.sh -- lane BTOFREE1, 2026-09-16.
#
# THE QUESTION. bungo, 2026-09-12 18:3x: "essentially, no legacy vanilla file
# types are now used by us or baked in the FO4CS lod bake" -- "Except the data
# we're reading from for the bakes". The `.BTO` chunk was the last legacy type
# our own FO4CS bake still left in the mod folder, and it was left because five
# passes READ IT BACK (texture arrays, atlas, shape merge, far-ring cut, card
# arrays), not because anything downstream of the bake wants it. From today it
# is built in `<mod folder>/lodgen_bto_scratch`, read there, and removed.
#
# Three legs, each one a byte comparison against a RUNG exe, because "the same
# files come out" is the only claim worth making about a change that moves where
# a file is written:
#
#   (a) the FO4CS default bake: NO `*.BTO` anywhere under the mod folder, no
#       scratch folder left behind, and every other file -- the native pair, the
#       manifests, the texture arrays, the card arrays, the ledger -- byte for
#       byte what the rung wrote. REFUTER: the rung's own tree is counted and
#       must CONTAIN `.BTO` files, or leg (a) is congratulating itself on an
#       empty folder.
#   (b) the way back, `--keep-bto`: the whole tree byte-identical to the rung's,
#       chunks included. A default with no exact way back is a removal.
#   (c) the stock target (no `--native` at all): the whole tree byte-identical
#       to the rung's. This is the brief's hard gate and it is measured on its
#       own bake, not inferred from (a).
#
# and one census leg: the disposition clause has to be WRITTEN and to MOVE --
# the drop bake says how many chunks it removed and how many bytes that freed,
# with both above zero, and the `--keep-bto` bake says 0 dropped, 0 freed.
#
# USAGE
#   bash tests/spells/lodgen_btofree.sh
#   RUNG=release/NifSkope.before_btofree1.exe  the exe the bytes are pinned to
#   EXE=...   the exe under test          REGION="-20 24 -19 25"   DIM=4
#   OUT=<dir> keep the trees for a look
set -u

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
NS="${EXE:-$ROOT/release/NifSkope.exe}"
RUNG="${RUNG:-$ROOT/release/NifSkope.before_btofree1.exe}"
ESM="${ESM:-X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm}"
DATA="${DATA:-E:/Tools/Fallout 4/DataUnpacked/Data}"
REGION="${REGION:--20 24 -19 25}"
DIM="${DIM:-4}"
PY="${PY:-$(command -v python || echo /c/Windows/py)}"
LEDGER="$ROOT/tests/spells/lodgen_btofree_ledger.py"
KEEP="${OUT:-}"
if [ -n "$KEEP" ]; then W="$KEEP"; mkdir -p "$W"; else W="$(mktemp -d)"; trap 'rm -rf "$W"' EXIT; fi
# the grouping braces are load-bearing -- see the note in lodgen_native.sh
WA="$(cd "$W" && { pwd -W 2>/dev/null || pwd; })"

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
# THE PAIR MOVED (lane LAYOUT1, 2026-09-16, bungo 19:3x): a FO4CS bake
# writes its .lodo/.lodi under <mod folder>/FO4CSLOD/<ws>/, and `--native`
# names that mod folder. A RUNG exe from before the move writes them at the
# --native directory itself, so this asks the tree where the pair is rather
# than spelling a layout that depends on which exe baked it.
pairdir () {
	if [ -d "$1/FO4CSLOD/Commonwealth" ]; then echo "$1/FO4CSLOD/Commonwealth"
	else echo "$1"; fi
}

# WHERE A RUNG FILE IS NOW (lane LAYOUT1, 2026-09-16).  The tree comparisons
# below pair a file baked by the RUNG exe with the same file baked by this one,
# and after the move the two have different relative paths.  This is the map,
# and it is written for THIS harness's own switches -- `--out-dir $WA/<name>`,
# `--tex-dir $WA/<name>/tex`, `--native $WA/<name>/nat` -- because a file's
# relative path depends on which folder the operator named, while the game-path
# rewrite in lodgen_layout_diff.py does not.  A name this map does not know
# stays as it is and shows up as "only in ..." rather than passing quietly.
relmap () {
	echo "$1" | sed \
		-e 's|^tex/Objects/|FO4CSLOD/Commonwealth/Objects/|' \
		-e 's|^Terrain/Commonwealth[.]|FO4CSLOD/Commonwealth/Commonwealth.|' \
		-e 's|^Textures/Lodgen/Aggregate/Commonwealth/|FO4CSLOD/Commonwealth/Aggregate/|' \
		-e 's|^Textures/Lodgen/Cards/|FO4CSLOD/Cards/|' \
		-e 's|^\(Commonwealth[.].*[.]BTO[.]manifest[.]txt\)$|FO4CSLOD/Commonwealth/\1|' \
		-e 's|^nat/\(Commonwealth[.]lod[oi]\)$|nat/FO4CSLOD/Commonwealth/\1|'
}


date
echo "exe   : $NS ($(stat -c%s "$NS") bytes, $(stat -c%y "$NS" | cut -c1-19))"
if [ -x "$RUNG" ]; then
	echo "rung  : $RUNG ($(stat -c%s "$RUNG") bytes, $(stat -c%y "$RUNG" | cut -c1-19))"
else
	echo "rung  : $RUNG -- NOT ON DISK, the byte legs will skip"
fi
echo "region: $REGION dim $DIM"

# bake <exe> <name> [extra switches...] -- one FO4CS bake unless --stock is given
bake () {
	local exe="$1" name="$2"; shift 2
	local stock=0
	# bungo 2026-09-17, "Authored LODs only": the ladder and the near library ship
	# OFF. The rung exes predate that and cannot take the switch, so the exe under
	# test is asked for the rung's old default by name and the bytes stay comparable.
	local eq=""
	[ "$exe" != "$RUNG" ] && eq="--library near --native-ladder"
	if [ "${1:-}" = "--stock" ]; then stock=1; shift; fi
	mkdir -p "$WA/$name" "$WA/$name/tex" "$WA/$name/nat"
	local nat=""
	[ "$stock" = "0" ] && nat="--native $WA/$name/nat"
	# shellcheck disable=SC2086
	"$exe" -no-gui lodgen "$ESM" --worldspace 3C \
		--terrain-region $REGION --dim "$DIM" --data-root "$DATA" \
		--out-dir "$WA/$name" --tex-dir "$WA/$name/tex" $nat \
		--cover --arrays --road-detail 1 $eq "$@" \
		> "$W/$name.log" 2>&1
	local rc=$?
	echo "  $name: rc=$rc, $(find "$W/$name" -type f | wc -l) file(s)"
	return $rc
}

# treecmp <A> <B> <exclude regex or -> -- prints its verdict and sets TC_*
# recof <dir> -- the bake record under <dir>, wherever the exe that wrote it
# put it. The stock target writes it beside the bake; the FO4CS target writes it
# at FO4CSLOD/<ws>/<ws>.lodb since lane BAKEREC1 (2026-09-17). Asked rather than
# spelled, so this harness keeps measuring the record and not the layout.
recof () { find "$1" -name '*.lodb' 2>/dev/null | head -1; }

# recv1 <file> -- true when <file> is the VERSION 1 BINARY record. The rung exe
# writes that container and this exe writes version 2 plain text, so the two
# cannot be compared row by row at all: there is no reader that can read both,
# by design (docs/LODGEN_BAKE_RECORD.md, "the v1 refusal"). A leg that hits this
# SKIPS with the reason rather than reporting a failure it cannot fix.
recv1 () { [ -n "$1" ] && [ -f "$1" ] && [ "$(head -c 4 "$1")" = "LODB" ]; }

treecmp () {
	local a="$1" b="$2" ex="$3"
	TC_SAME=0; TC_DIFF=0; TC_ONLYA=0; TC_ONLYB=0; TC_PATH=0; TC_LIST=""; TC_SEEN=""
	local rel m
	for rel in $(cd "$a" && find . -type f | sed 's|^\./||' | sort); do
		case "$ex" in -) ;; *) echo "$rel" | grep -qE "$ex" && continue ;; esac
		m="$(relmap "$rel")"
		# ASK THE TREE, as pairdir() does (lane LAYOUT1): the stock target did
		# not move at all, and --keep-bto deliberately leaves the sidecar
		# beside its chunk at the out-dir root, so a name the new tree does
		# not carry under the root is looked for where it always was.  A file
		# that is at NEITHER path still counts as "only in" and fails; that
		# the move happened at all is the layout gate's leg (a), not this one.
		[ -f "$b/$m" ] || m="$rel"
		TC_SEEN="$TC_SEEN $m"
		if [ ! -f "$b/$m" ]; then
			TC_ONLYA=$((TC_ONLYA+1)); TC_LIST="$TC_LIST
    only in $(basename "$a"): $rel"
		elif cmp -s "$a/$rel" "$b/$m"; then
			TC_SAME=$((TC_SAME+1))
		elif "$PY" "$ROOT/tests/spells/lodgen_layout_diff.py" --ws Commonwealth \
			--quiet --pair "$a/$rel" "$b/$m" > /dev/null 2>&1; then
			# THE FILE CARRIES A GAME PATH (lane LAYOUT1): the same tool the
			# layout gate's leg (b) uses says these two differ ONLY in the
			# path strings the move rewrote, and in the length words those
			# strings force.  Counted apart from `identical` so the number
			# is never quietly folded into it.
			TC_PATH=$((TC_PATH+1)); TC_LIST="$TC_LIST
    path-rewrite only: $rel -> $m"
		else
			TC_DIFF=$((TC_DIFF+1)); TC_LIST="$TC_LIST
    DIFFERS: $rel -> $m ($(stat -c%s "$a/$rel") vs $(stat -c%s "$b/$m"))"
		fi
	done
	for rel in $(cd "$b" && find . -type f | sed 's|^\./||' | sort); do
		case "$ex" in -) ;; *) echo "$rel" | grep -qE "$ex" && continue ;; esac
		case " $TC_SEEN " in *" $rel "*) continue ;; esac
		[ -f "$a/$rel" ] || { TC_ONLYB=$((TC_ONLYB+1)); TC_LIST="$TC_LIST
    only in $(basename "$b"): $rel"; }
	done
	echo "    $TC_SAME identical, $TC_PATH path-rewrite only, $TC_DIFF differ, $TC_ONLYA only in $(basename "$a"), $TC_ONLYB only in $(basename "$b")"
	[ -n "$TC_LIST" ] && echo "$TC_LIST"
	return 0
}

# ============================================================================
echo
echo "== 0. the bakes =="
HAVE_RUNG=0
if [ -x "$RUNG" ]; then
	HAVE_RUNG=1
	bake "$RUNG" rung_native   || bad "the rung's FO4CS bake ran"
	bake "$RUNG" rung_stock --stock || bad "the rung's stock bake ran"
fi
bake "$NS" drop        || bad "the FO4CS default bake ran"
bake "$NS" keep --keep-bto || bad "the --keep-bto bake ran"
bake "$NS" stock --stock   || bad "the stock bake ran"

# ============================================================================
echo
echo "== (a) the FO4CS default: our types and nothing else =="
NBTO="$(find "$W/drop" -name "*.BTO" | wc -l)"
echo "    .BTO files under the mod folder: $NBTO"
[ "$NBTO" -eq 0 ] && note "(a) the FO4CS default bake leaves ZERO .BTO chunks in the mod folder" \
	|| bad "(a) the FO4CS default bake leaves ZERO .BTO chunks (found $NBTO)"
[ ! -d "$W/drop/lodgen_bto_scratch" ] && note "(a) and no lodgen_bto_scratch folder behind it" \
	|| bad "(a) and no lodgen_bto_scratch folder behind it"
NMAN="$(find "$W/drop" -name "*.BTO.manifest.txt" | wc -l)"
echo "    manifest sidecars kept: $NMAN"
[ "$NMAN" -gt 0 ] && note "(a) the manifest sidecars are kept ($NMAN), which is bungo's open call" \
	|| bad "(a) the manifest sidecars are kept (found $NMAN)"
# and the mod folder carries only our types plus the sidecars and the textures
STRAY="$(find "$W/drop" -type f | sed 's|.*\.||' | tr 'A-Z' 'a-z' | sort -u | tr '\n' ' ')"
echo "    extensions under the mod folder: $STRAY"
case " $STRAY " in
	*" bto "*) bad "(a) a .BTO extension is still present under the mod folder" ;;
	*) note "(a) no .BTO extension appears anywhere under the mod folder" ;;
esac

if [ "$HAVE_RUNG" = "1" ]; then
	# THE REFUTER. If the rung wrote no chunks either, the three checks above
	# are congratulating themselves on an empty folder.
	RBTO="$(find "$W/rung_native" -name "*.BTO" | wc -l)"
	echo "    the RUNG's same bake wrote $RBTO .BTO chunk(s)"
	[ "$RBTO" -gt 0 ] && note "(a) the refuter fires: the rung DID write .BTO chunks here ($RBTO)" \
		|| bad "(a) the refuter fires: the rung DID write .BTO chunks here (found $RBTO)"
	# THE LEDGER IS NOT SWEPT HERE, AND NOT BECAUSE IT IS INCONVENIENT.
	# `.lodb` records what a bake LEFT ON DISK, so a bake that leaves no chunk
	# must not record one -- digesting a file about to be deleted is the defect
	# that once forced full rebakes. `cmp` can only say that it moved; the leg
	# below says which rows moved and asserts that NOTHING else did, which is a
	# stronger statement than the sweep was making.
	echo "    everything except the chunks, the ledger and the native cache, against the rung:"
	# THE NATIVE CACHE IS NOT SWEPT HERE EITHER (lane INCR1, 2026-09-17, added by
	# AUDIT1 2026-09-17). `<ws>.<dim>.<cx>.<cy>.lodj` is written by every FO4CS
	# bake from this exe and the rung predates the feature, so it is "only in
	# drop" -- a NEW OUTPUT, not a moved or changed one. The sweep cannot phrase
	# that, so the file is taken out of it by name and then CHECKED BY NAME just
	# below: excluded here, asserted present there, and the rung asserted to have
	# none, so the exclusion can never hide a file that stopped being written.
	treecmp "$W/rung_native" "$W/drop" '[.]BTO$|[.]lodb$|[.]lodj$'
	if [ "$TC_DIFF" -eq 0 ] && [ "$TC_ONLYA" -eq 0 ] && [ "$TC_ONLYB" -eq 0 ] && [ "$TC_SAME" -gt 0 ]; then
		note "(a) every other file is byte-identical to the rung's ($TC_SAME files, $TC_PATH more equal after the path rewrite)"
	else
		bad "(a) every other file is byte-identical to the rung's ($TC_DIFF differ, $TC_ONLYA/$TC_ONLYB only on one side)"
	fi
	# the two halves of the exclusion above, stated as checks so the sweep is
	# narrowed and not weakened: the new side HAS a native cache, the rung has none.
	NLODJ="$(find "$W/drop" -name "*.lodj" | wc -l)"
	RLODJ="$(find "$W/rung_native" -name "*.lodj" | wc -l)"
	echo "    native cache files: this exe $NLODJ, rung $RLODJ"
	[ "$NLODJ" -gt 0 ] && note "(a) and the excluded native cache IS written by this exe ($NLODJ .lodj)" \
		|| bad "(a) and the excluded native cache IS written by this exe (found $NLODJ .lodj)"
	[ "$RLODJ" -eq 0 ] && note "(a) and the rung wrote none, so the exclusion covers a NEW output and hides no change" \
		|| bad "(a) and the rung wrote none (found $RLODJ .lodj: the exclusion would now hide a real difference)"
	# THE SWEEP'S OWN REFUTER, in the same run: a byte flipped in one compared
	# file must make treecmp say DIFFERS. Without this the three legs above pass
	# on a comparison that cannot fail.
	rm -rf "$W/refute"; cp -r "$W/drop" "$W/refute"
	RF="$(find "$W/refute" -name "*.BTO.manifest.txt" | head -1)"
	if [ -n "$RF" ]; then
		printf 'x' >> "$RF"
		treecmp "$W/rung_native" "$W/refute" '[.]BTO$|[.]lodb$|[.]lodj$' > /dev/null
		[ "$TC_DIFF" -gt 0 ] && note "(a) refuter: one appended byte in a compared file makes the sweep report it ($TC_DIFF differ)" \
			|| bad "(a) refuter: one appended byte in a compared file makes the sweep report it (read $TC_DIFF differ)"
	else
		bad "(a) refuter: no compared file to mutate"
	fi
	rm -rf "$W/refute"
	# the pair moved under FO4CSLOD/<ws>/ (lane LAYOUT1, 2026-09-16) and the
	# rung exe predates the move, so each side is asked where its own is.
	for f in Commonwealth.lodo Commonwealth.lodi; do
		a="$(pairdir "$W/rung_native/nat")/$f"; b="$(pairdir "$W/drop/nat")/$f"
		if [ -f "$a" ] && cmp -s "$a" "$b"; then
			note "(a) $f is byte-identical to the rung's ($(stat -c%s "$b") bytes)"
		else bad "(a) $f is byte-identical to the rung's"; fi
	done
	echo "    the ledger, row by row:"
	RREC="$(recof "$W/rung_native")"; DREC="$(recof "$W/drop")"
	echo "      rung record: ${RREC:-none}"
	echo "      this record: ${DREC:-none}"
	if recv1 "$RREC"; then
		skip "(a) the ledger row-by-row leg: the rung writes the version 1 BINARY record and this exe writes version 2 plain text -- no reader reads both, by design"
	elif [ -n "$RREC" ] && [ -n "$DREC" ] && "$PY" "$LEDGER" drop "$RREC" "$DREC" \
		"$W/rung_native" "$W/drop"; then
		note "(a) the ledger drops the .BTO row and keeps every other digest, the manifest's included"
	else bad "(a) the ledger drops the .BTO row and keeps every other digest, the manifest's included"; fi
else
	skip "(a) the byte legs against the rung: no rung exe on disk"
fi

# ============================================================================
echo
echo "== (b) --keep-bto is the exact way back =="
KBTO="$(find "$W/keep" -name "*.BTO" | wc -l)"
echo "    .BTO files with --keep-bto: $KBTO"
[ "$KBTO" -gt 0 ] && note "(b) --keep-bto leaves the chunks in the mod folder ($KBTO)" \
	|| bad "(b) --keep-bto leaves the chunks in the mod folder (found $KBTO)"
[ ! -d "$W/keep/lodgen_bto_scratch" ] && note "(b) and creates no scratch folder at all" \
	|| bad "(b) and creates no scratch folder at all"
if [ "$HAVE_RUNG" = "1" ]; then
	# the ledger again, and this time it MUST move: `--keep-bto` is one more
	# token on the command line, the ledger digests that line, and an
	# --incremental run has to refuse to reuse chunks a different line produced.
	# `.lodj` out of the sweep for the reason leg (a) states, and checked by name
	# right after it (AUDIT1, 2026-09-17).
	treecmp "$W/rung_native" "$W/keep" '[.]lodb$|[.]lodj$'
	KLODJ="$(find "$W/keep" -name "*.lodj" | wc -l)"
	echo "    native cache files with --keep-bto: $KLODJ"
	[ "$KLODJ" -gt 0 ] && note "(b) the excluded native cache is written on this side too ($KLODJ .lodj)" \
		|| bad "(b) the excluded native cache is written on this side too (found $KLODJ .lodj)"
	if [ "$TC_DIFF" -eq 0 ] && [ "$TC_ONLYA" -eq 0 ] && [ "$TC_ONLYB" -eq 0 ] && [ "$TC_SAME" -gt 0 ]; then
		note "(b) every output file is byte-identical to the rung's ($TC_SAME files, the chunks included, $TC_PATH more equal after the path rewrite)"
	else
		bad "(b) every output file is byte-identical to the rung's ($TC_DIFF differ, $TC_ONLYA/$TC_ONLYB only on one side)"
	fi
	echo "    the ledger, row by row:"
	RREC="$(recof "$W/rung_native")"; KREC="$(recof "$W/keep")"
	echo "      rung record: ${RREC:-none}"
	echo "      this record: ${KREC:-none}"
	if recv1 "$RREC"; then
		skip "(b) the ledger row-by-row leg: the rung writes the version 1 BINARY record and this exe writes version 2 plain text -- no reader reads both, by design"
	elif [ -n "$RREC" ] && [ -n "$KREC" ] && "$PY" "$LEDGER" keep "$RREC" "$KREC" \
		"$W/rung_native" "$W/keep"; then
		note "(b) the ledger differs only in the command-line digest, and that digest moved"
	else bad "(b) the ledger differs only in the command-line digest, and that digest moved"; fi
else
	skip "(b) the byte leg against the rung: no rung exe on disk"
fi

# ============================================================================
echo
echo "== (c) the stock target does not move at all =="
SBTO="$(find "$W/stock" -name "*.BTO" | wc -l)"
echo "    .BTO files from the stock target: $SBTO"
[ "$SBTO" -gt 0 ] && note "(c) the stock target still writes its .BTO chunks ($SBTO)" \
	|| bad "(c) the stock target still writes its .BTO chunks (found $SBTO)"
[ ! -d "$W/stock/lodgen_bto_scratch" ] && note "(c) and never creates a scratch folder" \
	|| bad "(c) and never creates a scratch folder"
if [ "$HAVE_RUNG" = "1" ]; then
	# THE RECORD IS NOT A STOCK OUTPUT BYTE, and it is asked about by name just
	# below instead (lane BAKEREC1, 2026-09-17; taken out of this sweep by AUDIT1
	# the same day). The rung writes the version 1 BINARY `LODB` container and
	# this exe writes the version 2 plain-text record: measured here 2026-09-17,
	# 716 bytes against 2161, and `cmp` can only say that they differ. That is
	# the record's own format landing, not the stock path moving, which is what
	# this leg exists to measure. Legs (a) and (b) already excluded it and asked
	# the record reader instead; this one did not, and that WAS its red.
	treecmp "$W/rung_stock" "$W/stock" '[.]lodb$'
	if [ "$TC_DIFF" -eq 0 ] && [ "$TC_ONLYA" -eq 0 ] && [ "$TC_ONLYB" -eq 0 ] && [ "$TC_SAME" -gt 0 ]; then
		note "(c) the whole stock output is byte-identical to the rung's ($TC_SAME files)"
	else
		bad "(c) the whole stock output is byte-identical to the rung's ($TC_DIFF differ, $TC_ONLYA/$TC_ONLYB only on one side)"
	fi
	# the exclusion, narrowed by a check rather than left as a hole: the stock
	# target still writes a record, and it is this exe's version 2 plain text.
	SRREC="$(recof "$W/rung_stock")"; SREC="$(recof "$W/stock")"
	echo "    rung record: ${SRREC:-none}"
	echo "    this record: ${SREC:-none}"
	if [ -n "$SREC" ] && [ -f "$SREC" ]; then
		note "(c) the stock target still writes its bake record ($(stat -c%s "$SREC") bytes, $(head -c 4 "$SREC"))"
	else bad "(c) the stock target still writes its bake record (none found)"; fi
	if recv1 "$SRREC"; then
		skip "(c) the record row-by-row leg: the rung writes the version 1 BINARY record and this exe writes version 2 plain text -- no reader reads both, by design"
	elif [ -n "$SRREC" ] && [ -n "$SREC" ]; then
		# BOTH SIDES VERSION 2: then the stock target's recorded OUTPUT ROWS --
		# which files, and each one's digest -- have to be the same rows, and
		# the sweep above has already proved the files themselves identical.
		if "$PY" "$LEDGER" same "$SRREC" "$SREC" "$W/rung_stock" "$W/stock"; then
			note "(c) and the two records name the same stock outputs, digest for digest"
		else bad "(c) and the two records name the same stock outputs, digest for digest"; fi
	else
		bad "(c) a bake record is missing on one side"
	fi
	# THE SWEEP'S REFUTER for this leg, in the same run.
	rm -rf "$W/refute_stock"; cp -r "$W/stock" "$W/refute_stock"
	RS="$(find "$W/refute_stock" -name "*.BTO" | head -1)"
	if [ -n "$RS" ]; then
		printf 'x' >> "$RS"
		treecmp "$W/rung_stock" "$W/refute_stock" '[.]lodb$' > /dev/null
		[ "$TC_DIFF" -gt 0 ] && note "(c) refuter: one appended byte in a stock chunk makes the sweep report it ($TC_DIFF differ)" \
			|| bad "(c) refuter: one appended byte in a stock chunk makes the sweep report it (read $TC_DIFF differ)"
	else
		bad "(c) refuter: no stock chunk to mutate"
	fi
	rm -rf "$W/refute_stock"
else
	skip "(c) the byte leg against the rung: no rung exe on disk"
fi

# ============================================================================
echo
echo "== (d) the census clause is written AND it moves =="
DLINE="$(grep -m1 "^bake census: " "$W/drop.log" | sed 's/.*, bto /bto /')"
KLINE="$(grep -m1 "^bake census: " "$W/keep.log" | sed 's/.*, bto /bto /')"
SLINE="$(grep -m1 "^bake census: " "$W/stock.log" | sed 's/.*, bto /bto /')"
echo "    default   : $DLINE"
echo "    --keep-bto: $KLINE"
echo "    stock     : $SLINE"
case "$DLINE" in
	"bto built in scratch "*) note "(d) the default bake says its chunks were built in a scratch folder" ;;
	*) bad "(d) the default bake says its chunks were built in a scratch folder (read '$DLINE')" ;;
esac
case "$KLINE" in
	"bto built in the mod folder, "*", 0 dropped, 0 bytes freed"*) note "(d) --keep-bto reads 0 dropped, 0 bytes freed" ;;
	*) bad "(d) --keep-bto reads 0 dropped, 0 bytes freed (read '$KLINE')" ;;
esac
# the numbers themselves, so the clause is a measurement and not a sentence
DDROP="$(printf '%s' "$DLINE" | sed -n 's/.*chunk(s), \([0-9][0-9]*\) dropped.*/\1/p')"
DFREE="$(printf '%s' "$DLINE" | sed -n 's/.*dropped, \([0-9][0-9]*\) bytes freed.*/\1/p')"
echo "    dropped: ${DDROP:-?} chunk(s), freed: ${DFREE:-?} bytes"
if [ -n "${DDROP:-}" ] && [ "$DDROP" -gt 0 ] 2>/dev/null; then
	note "(d) the dropped count MOVES off zero ($DDROP)"
else bad "(d) the dropped count MOVES off zero (read '${DDROP:-}')"; fi
if [ -n "${DFREE:-}" ] && [ "$DFREE" -gt 0 ] 2>/dev/null; then
	note "(d) the bytes-freed count MOVES off zero ($DFREE)"
else bad "(d) the bytes-freed count MOVES off zero (read '${DFREE:-}')"; fi
# and the dedicated line the CLI prints beside it
if grep -q "^bto scratch: " "$W/drop.log"; then
	grep -m1 "^bto scratch: " "$W/drop.log" | sed 's/^/    /'
	note "(d) the CLI prints its own bto scratch: line"
else bad "(d) the CLI prints its own bto scratch: line"; fi
if grep -q "^bto scratch: " "$W/keep.log"; then
	bad "(d) --keep-bto prints NO bto scratch: line (it printed one)"
else note "(d) --keep-bto prints no bto scratch: line"; fi
case "$SLINE" in
	"bto built in the mod folder, "*) note "(d) the stock target's clause says the mod folder, unchanged" ;;
	*) bad "(d) the stock target's clause says the mod folder (read '$SLINE')" ;;
esac

echo
date
echo "$checks checks, $fails failures"
[ "$fails" -eq 0 ] && { echo PASS; exit 0; } || { echo FAIL; exit 1; }
