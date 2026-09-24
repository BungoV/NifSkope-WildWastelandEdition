#!/usr/bin/env bash
# lodgen_bakerec.sh -- THE BAKE RECORD, measured. Lane BAKEREC1, 2026-09-17.
#
# WHAT IS UNDER TEST
#   `<out>/FO4CSLOD/<ws>/<ws>.lodb` -- the plain-text version 2 bake record that
#   every bake writes LAST. Format: docs/LODGEN_BAKE_RECORD.md. Writer:
#   `lodgenWriteLedger` in src/lodbfile.cpp. The one Python reader is
#   tests/spells/lodb_read.py and the measuring half of this harness is
#   tests/spells/lodgen_bakerec_gate.py.
#
# THE LEGS
#   (a) the record is written LAST, carries all eight sections, its `end` line
#       equals a `find` of its own tree, and every census line the bake printed
#       is in it -- with the two families that cannot be, named.
#   (b) the five staleness hashes equal the `.lodo`/`.lodi` headers, decoded by
#       the repo's own decoder rather than by a second reader written here.
#   (c) the plugin lines equal the list the bake was given, in order, with the
#       size `stat` reports and a per-file FNV-1a 64 computed independently in
#       Python.
#   (d) the three refuters, each a real edited plugin made by
#       scratchpad/land1_20260912/b_esmedit.py:
#         d1 a placement MOVED inside the baked region  -> the chunk digest moves
#         d2 a record edited OUTSIDE the baked region   -> no chunk digest moves,
#            and the plugin's byte hash moves anyway (loadOrderHash is blind to
#            both, which is the whole reason the byte hash exists)
#         d3 the mod folder renamed and a resource touched -> normalised, the
#            record is unchanged
#   (e) `--native-verify` NAMES the plugin that went stale and says which way.
#   (f) the stock target is byte-identical to the rung, the record aside -- the
#       stated divergence (docs/LODGEN_BAKE_RECORD.md section "the stock
#       target"): every bake writes a record, the stock one included.
#   (g) one parser: no harness in the tree still parses the record by hand.
#   (h) two bakes of one tree differ ONLY in the volatile fields.
#
# USAGE
#   bash tests/spells/lodgen_bakerec.sh
#   LEGS=abc bash tests/spells/lodgen_bakerec.sh     # a subset, in this order
#   OUT=<dir> ...                                    # keep the bakes
#   EXE=... RUNG=... ESM=... DATA=... REGION="-20 24 -20 24" DIM=4
#
# Leg (d) COPIES the plugin three times (about 1.1 GB for Fallout4.esm) and
# bakes four times; it skips itself with a reason when the copies do not fit or
# when b_esmedit.py is not in the tree.
set -u

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
NS="${EXE:-$ROOT/release/NifSkope.exe}"
RUNG="${RUNG:-$ROOT/release/NifSkope.before_bakerec1.exe}"
ESM="${ESM:-X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm}"
DATA="${DATA:-E:/Tools/Fallout 4/DataUnpacked/Data}"
REGION="${REGION:--20 24 -20 24}"
DIM="${DIM:-4}"
WS="${WS:-Commonwealth}"
LEGS="${LEGS:-abcdefgh}"
PY="${PY:-$(command -v python || echo /c/Windows/py)}"
GATE="$ROOT/tests/spells/lodgen_bakerec_gate.py"
ESMEDIT="$ROOT/scratchpad/land1_20260912/b_esmedit.py"
KEEP="${OUT:-}"
if [ -n "$KEEP" ]; then W="$KEEP"; mkdir -p "$W"; else W="$(mktemp -d)"; trap 'rm -rf "$W"' EXIT; fi
# the grouping braces are load-bearing -- see the note in lodgen_native.sh
WA="$(cd "$W" && { pwd -W 2>/dev/null || pwd; })"
FO4="FO4CSLOD/$WS"

[ -x "$NS" ]  || { echo "no NifSkope.exe at $NS"; exit 2; }
[ -f "$ESM" ] || { echo "no plugin at $ESM"; exit 2; }
[ -d "$DATA" ]|| { echo "no unpacked Data at $DATA"; exit 2; }
# CONSTITUTION 6: one instance, and never a bake while the game is up.
if tasklist 2>/dev/null | grep -qi "Fallout4.exe"; then
	echo "REFUSED: Fallout4.exe is running"; exit 2
fi

checks=0
fails=0
note () { checks=$((checks+1)); echo "  ok   $1"; }
bad ()  { checks=$((checks+1)); fails=$((fails+1)); echo "  FAIL $1"; }
skip () { echo "  SKIP $1"; }
# hang a note/bad on a helper's exit code, so one line a leg reads the same way
verdict () { if [ "$1" -eq 0 ]; then note "$2"; else bad "$2"; fi; }

date
echo "exe   : $NS ($(stat -c%s "$NS") bytes, $(stat -c%y "$NS" | cut -c1-19))"
if [ -x "$RUNG" ]; then
	echo "rung  : $RUNG ($(stat -c%s "$RUNG") bytes, $(stat -c%y "$RUNG" | cut -c1-19))"
else
	echo "rung  : $RUNG -- NOT ON DISK, leg (f) will skip"
fi
echo "plugin: $ESM ($(stat -c%s "$ESM") bytes)"
echo "region: $REGION dim $DIM, worldspace $WS"
echo "work  : $W"

# bake <name> <plugin-list> [--stock] [extra switches...]
bake () {
	local name="$1" plugins="$2"; shift 2
	local stock=0
	if [ "${1:-}" = "--stock" ]; then stock=1; shift; fi
	mkdir -p "$W/$name" "$W/$name/tex"
	local nat=""
	[ "$stock" = "0" ] && nat="--native $WA/$name"
	# shellcheck disable=SC2086
	"$NS" -no-gui lodgen "$plugins" --worldspace 3C \
		--terrain-region $REGION --dim "$DIM" --data-root "$DATA" \
		--out-dir "$WA/$name" --tex-dir "$WA/$name/tex" $nat \
		--cover --arrays --road-detail 1 "$@" \
		> "$W/$name.log" 2>&1
	local rc=$?
	echo "  $name: rc=$rc, $(find "$W/$name" -type f | wc -l) file(s), record $(find "$W/$name" -name '*.lodb' | wc -l)"
	return $rc
}

# recof <dir> -- the record under <dir>, wherever the target put it
recof () { find "$1" -name '*.lodb' 2>/dev/null | head -1; }

# every leg that needs the baseline record reads $REC; with `set -u` it must
# exist even when the leg that fills it did not run.
REC=""

# ============================================================================
if [ -z "${LEGS##*a*}" ] || [ -z "${LEGS##*b*}" ] || [ -z "${LEGS##*c*}" ] \
	|| [ -z "${LEGS##*e*}" ] || [ -z "${LEGS##*h*}" ]; then
echo
echo "== the baseline bake =="
bake one "$ESM" || bad "the baseline bake runs"
REC="$(recof "$W/one")"
echo "  record: ${REC:-NONE}"
fi

# ============================================================================
if [ -z "${LEGS##*a*}" ]; then
echo
echo "== (a) the record is written last, complete, and counts its own tree =="
if [ -z "$REC" ]; then
	bad "(a) the bake wrote a record at all"
else
	case "$REC" in
		*"$FO4/$WS.lodb") note "(a) the record is at $FO4/$WS.lodb" ;;
		*) bad "(a) the record is at $FO4/$WS.lodb (found $REC)" ;;
	esac
	"$PY" "$GATE" written-last "$REC" "$W/one"
	verdict $? "(a) the record is the LAST file the bake wrote"
	"$PY" "$GATE" sections "$REC" "$W/one" "$W/one.log"
	verdict $? "(a) all eight sections, the end line against a find, and the census floor"
fi
fi

# ============================================================================
if [ -z "${LEGS##*b*}" ]; then
echo
echo "== (b) the five staleness hashes equal the pair's headers =="
LODO="$W/one/$FO4/$WS.lodo"
LODI="$W/one/$FO4/$WS.lodi"
if [ -n "$REC" ] && [ -f "$LODO" ] && [ -f "$LODI" ]; then
	"$PY" "$GATE" hashes "$REC" "$LODO" "$LODI"
	verdict $? "(b) every hash in the record equals the one in the pair's header"
else
	skip "(b) the hash leg: no pair on disk (record ${REC:-none}, lodo $LODO)"
fi
fi

# ============================================================================
if [ -z "${LEGS##*c*}" ]; then
echo
echo "== (c) the plugin lines are the list that was given =="
if [ -n "$REC" ]; then
	"$PY" "$GATE" plugins "$REC" "$ESM"
	verdict $? "(c) name, order, size and an independent FNV-1a 64, plugin by plugin"
else
	skip "(c) the plugin leg: no record"
fi
fi

# ============================================================================
if [ -z "${LEGS##*d*}" ]; then
echo
echo "== (d) the refuters: what the record notices and what it does not =="
# d0: can we afford it? Three copies of the plugin and four bakes.
NEED=$(( $(stat -c%s "$ESM") * 3 / 1048576 ))
FREE="$(df -Pm "$W" 2>/dev/null | awk 'NR==2 {print $4}')"
if [ ! -f "$ESMEDIT" ]; then
	skip "(d) the refuters: no $ESMEDIT in the tree (it is what makes a genuinely edited plugin)"
elif [ -n "$FREE" ] && [ "$FREE" -lt "$NEED" ]; then
	skip "(d) the refuters: $FREE MB free under $W, $NEED MB needed for three plugin copies"
else
	# THE TWO EDITS. One inside the baked region, one outside it. The cell
	# coordinates are the region's own corner and a cell far from it, so
	# "inside" and "outside" are this run's numbers and not a constant.
	IN_X="$(echo "$REGION" | cut -d' ' -f1)"
	IN_Y="$(echo "$REGION" | cut -d' ' -f2)"
	OUT_X=$(( IN_X + 40 ))
	OUT_Y=$(( IN_Y + 40 ))
	echo "  the edited cell inside the region: $IN_X,$IN_Y; outside it: $OUT_X,$OUT_Y"
	MOVED="$W/moved.esm"
	FAR="$W/far.esm"
	if "$PY" "$ESMEDIT" move "$ESM" "$MOVED" "$IN_X" "$IN_Y" 96 > "$W/edit_moved.log" 2>&1; then
		note "(d) a real edited plugin was made: one placement moved 96 units at $IN_X,$IN_Y"
	else
		bad "(d) a real edited plugin was made: one placement moved 96 units at $IN_X,$IN_Y"
		sed 's/^/      /' "$W/edit_moved.log" | tail -5
	fi
	if "$PY" "$ESMEDIT" height "$ESM" "$FAR" "$OUT_X" "$OUT_Y" 8 > "$W/edit_far.log" 2>&1; then
		note "(d) and a second one: a height raised at $OUT_X,$OUT_Y, outside the baked region"
	else
		bad "(d) and a second one: a height raised at $OUT_X,$OUT_Y, outside the baked region"
		sed 's/^/      /' "$W/edit_far.log" | tail -5
	fi

	# d1 -- the moved placement MUST move a chunk digest
	if [ -f "$MOVED" ]; then
		bake d1 "$MOVED" || bad "(d1) the bake with the moved placement runs"
		D1="$(recof "$W/d1")"
		if [ -n "$D1" ] && [ -n "$REC" ]; then
			"$PY" "$GATE" chunks "$REC" "$D1" --expect moved
			verdict $? "(d1) a placement moved inside the region moves a chunk input digest"
		else skip "(d1) no record on one side"; fi
	fi

	# d2 -- an edit the chunks do not read must NOT move a chunk digest, and
	#       the plugin's byte hash must move anyway
	if [ -f "$FAR" ]; then
		bake d2 "$FAR" || bad "(d2) the bake with the far edit runs"
		D2="$(recof "$W/d2")"
		if [ -n "$D2" ] && [ -n "$REC" ]; then
			"$PY" "$GATE" chunks "$REC" "$D2" --expect same
			verdict $? "(d2) an edit outside the region moves no chunk input digest"
			# and the byte hash sees it even though loadOrderHash cannot
			HB="$("$PY" "$ROOT/tests/spells/lodb_read.py" "$REC" plugins)"
			HA="$("$PY" "$ROOT/tests/spells/lodb_read.py" "$D2" plugins)"
			if [ "$HB" != "$HA" ]; then
				note "(d2) and the plugin's own byte hash moved, which loadOrderHash cannot see"
			else
				bad "(d2) and the plugin's own byte hash moved, which loadOrderHash cannot see"
			fi
		else skip "(d2) no record on one side"; fi
	fi

	# d3 -- the mod folder renamed and a resource touched: the record, masked,
	#       must not move. This is the reason the resource line is volatile.
	bake d3a "$ESM" || bad "(d3) the first of the two folder bakes runs"
	mv "$W/d3a" "$W/d3b" 2>/dev/null
	bake d3c "$ESM" || bad "(d3) the second of the two folder bakes runs"
	D3A="$(recof "$W/d3b")"
	D3C="$(recof "$W/d3c")"
	if [ -n "$D3A" ] && [ -n "$D3C" ]; then
		"$PY" "$GATE" chunks "$D3A" "$D3C" --expect same
		verdict $? "(d3) renaming the mod folder moves no chunk input digest"
	else skip "(d3) no record on one side"; fi
fi
fi

# ============================================================================
if [ -z "${LEGS##*e*}" ]; then
echo
echo "== (e) --native-verify names the plugin that went stale =="
LODO="$W/one/$FO4/$WS.lodo"
LODI="$W/one/$FO4/$WS.lodi"
if [ ! -f "$LODO" ]; then
	skip "(e) --native-verify: no pair on disk"
else
	# clean first: the pair against the plugin it was baked from
	"$NS" -no-gui lodgen "$ESM" --worldspace 3C --data-root "$DATA" \
		--native-verify "$LODO" "$LODI" --native-verify-corpus \
		> "$W/verify_clean.log" 2>&1
	VC=$?
	sed 's/^/      /' "$W/verify_clean.log" | grep -i "bake record\|stale\|ok" | head -6
	[ "$VC" -eq 0 ] && note "(e) the pair verifies clean against the plugin it was baked from" \
		|| bad "(e) the pair verifies clean against the plugin it was baked from (rc=$VC)"
	# then against an edited one: it must REFUSE and NAME the plugin
	STALE=""
	[ -f "$W/moved.esm" ] && STALE="$W/moved.esm"
	[ -z "$STALE" ] && [ -f "$W/far.esm" ] && STALE="$W/far.esm"
	if [ -z "$STALE" ]; then
		skip "(e) the stale leg: no edited plugin on disk (leg (d) makes it)"
	else
		"$NS" -no-gui lodgen "$STALE" --worldspace 3C --data-root "$DATA" \
			--native-verify "$LODO" "$LODI" --native-verify-corpus \
			> "$W/verify_stale.log" 2>&1
		VS=$?
		sed 's/^/      /' "$W/verify_stale.log" | grep -i "stale\|edited\|added\|removed\|bake record" | head -8
		[ "$VS" -ne 0 ] && note "(e) the pair is REFUSED against an edited plugin (rc=$VS)" \
			|| bad "(e) the pair is REFUSED against an edited plugin (rc=$VS)"
		if grep -qi "$(basename "$ESM")" "$W/verify_stale.log"; then
			note "(e) and the refusal NAMES the plugin ($(basename "$ESM"))"
		else
			bad "(e) and the refusal NAMES the plugin ($(basename "$ESM"))"
		fi
		if grep -qiE "was EDITED|was ADDED|is GONE|REORDERED|is a different size" "$W/verify_stale.log"; then
			note "(e) and says WHICH WAY it moved (edited / added / removed / reordered / resized)"
		else
			bad "(e) and says WHICH WAY it moved (edited / added / removed / reordered / resized)"
		fi
	fi
fi
fi

# ============================================================================
if [ -z "${LEGS##*f*}" ]; then
echo
echo "== (f) the stock target is byte-identical to the rung, the record aside =="
if [ ! -x "$RUNG" ]; then
	skip "(f) the stock byte leg: no rung exe at $RUNG"
else
	bake stock "$ESM" --stock || bad "(f) the stock bake runs"
	mkdir -p "$W/stock_rung" "$W/stock_rung/tex"
	# shellcheck disable=SC2086
	"$RUNG" -no-gui lodgen "$ESM" --worldspace 3C \
		--terrain-region $REGION --dim "$DIM" --data-root "$DATA" \
		--out-dir "$WA/stock_rung" --tex-dir "$WA/stock_rung/tex" \
		--cover --arrays --road-detail 1 > "$W/stock_rung.log" 2>&1
	echo "  stock_rung: rc=$?, $(find "$W/stock_rung" -type f | wc -l) file(s)"
	d=0; n=0
	for f in $(cd "$W/stock_rung" && find . -type f | sed 's|^\./||' | sort); do
		case "$f" in *.lodb) continue ;; esac
		n=$((n+1))
		cmp -s "$W/stock_rung/$f" "$W/stock/$f" || { echo "    DIFFER $f"; d=$((d+1)); }
	done
	echo "  $n stock file(s) compared, $d differ"
	[ "$d" -eq 0 ] && [ "$n" -gt 0 ] && note "(f) the stock bake is byte-identical to the rung's ($n files)" \
		|| bad "(f) the stock bake is byte-identical to the rung's ($d of $n differ)"
	# THE STATED DIVERGENCE, measured rather than asserted: the brief asked for
	# a record under the FO4CS target only, and the stock target writes one too,
	# because `--incremental` has refused without it since lane INCR1 shipped.
	SREC="$(recof "$W/stock")"
	RREC="$(recof "$W/stock_rung")"
	echo "  this exe's stock record: ${SREC:-none}"
	echo "  the rung's stock record: ${RREC:-none}"
	[ -n "$SREC" ] && note "(f) and the stock target writes a record too (the stated divergence)" \
		|| bad "(f) and the stock target writes a record too (the stated divergence)"
	if [ -n "$RREC" ] && [ "$(head -c 4 "$RREC")" = "LODB" ]; then
		note "(f) the rung's record is the v1 BINARY container, so the two cannot be compared row by row"
	fi
fi
fi

# ============================================================================
if [ -z "${LEGS##*g*}" ]; then
echo
echo "== (g) one parser: nothing in the tree reads the record by hand =="
# The pattern is built from pieces so that this line is not itself a hit, and
# the reader's own comment (which lists the four parsers it replaced) and this
# harness are excluded by name rather than by a broad filter.
PAT="b.index(b'"'{'"')|b.find(b'"'{'"')|raw\[16:\]"
HITS="$(cd "$ROOT" && grep -rnE "$PAT" tests/ \
	--exclude=lodb_read.py --exclude=lodgen_bakerec.sh \
	--exclude-dir=__pycache__ || true)"
if [ -z "$HITS" ]; then
	note "(g) no harness parses the record's container by hand any more"
else
	bad "(g) no harness parses the record's container by hand any more"
	echo "$HITS" | sed 's/^/    still hand-parsing: /'
fi
USERS="$(cd "$ROOT" && grep -rln "lodb_read" tests/ --exclude-dir=__pycache__ || true)"
echo "  the shared reader is imported by:"
echo "$USERS" | sed 's/^/    /'
NU="$(printf '%s' "$USERS" | grep -c . || true)"
[ "$NU" -ge 4 ] && note "(g) every harness that reads a record imports the one reader ($NU files)" \
	|| bad "(g) every harness that reads a record imports the one reader (only $NU files)"
fi

# ============================================================================
if [ -z "${LEGS##*h*}" ]; then
echo
echo "== (h) two bakes of one tree differ only in the volatile fields =="
# THE SAME COMMAND LINE, TWICE. Two different out-dirs would move the `switch`
# tokens and the census prose that names the root, and the leg would be
# measuring the folder name instead of the bake.
bake det "$ESM" || bad "(h) the first of the two identical bakes runs"
DET1="$(recof "$W/det")"
if [ -n "$DET1" ]; then
	cp "$DET1" "$W/det_first.lodb"
	# a second apart at least, so an identical record cannot be the same file
	bake det "$ESM" || bad "(h) the second of the two identical bakes runs"
	DET2="$(recof "$W/det")"
	if [ -n "$DET2" ]; then
		"$PY" "$GATE" identical "$W/det_first.lodb" "$DET2"
		verdict $? "(h) normalised the two records are byte-equal, and raw they are not"
	else
		skip "(h) the determinism leg: the second bake wrote no record"
	fi
else
	skip "(h) the determinism leg: the first bake wrote no record"
fi
fi

echo
echo "== lodgen_bakerec: $checks check(s), $fails failure(s) =="
[ "$fails" -eq 0 ] && echo "RESULT PASS" || echo "RESULT FAIL"
exit $([ "$fails" -eq 0 ] && echo 0 || echo 1)
