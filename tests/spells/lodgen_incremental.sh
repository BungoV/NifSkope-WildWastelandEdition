#!/usr/bin/env bash
# THE INCREMENTAL GATE (lane INCR1, 2026-09-17).
#
# `--incremental` promises one thing: the files it leaves on disk are the files
# a full bake would have left. Everything here is that sentence, measured -- on
# the FO4CS target FIRST, because that is the ruled default pipeline and the one
# `--incremental` used to refuse outright.
#
# EVERY ARM HAS A FLOOR. An arm whose floor is not met is VACUOUS and prints
# VACUOUS, never ok: a comparison over zero files, a "mixed" run in which
# nothing was cached, or a mask that masks nothing are all ways of passing
# without testing anything, and this gate's own first draft did the second one.
#
#   (a) FO4CS, nothing changed      -> the .lodo/.lodi pair is the same bytes
#   (b) FO4CS, one chunk dirty      -> still the same bytes, beside cached ones
#   (c) STOCK, nothing changed      -> every .BTR/.BTO/sheet is the same bytes
#   (d) the record's SUBSTANCE      -> identical, with a refuter
#   (e) --no-native-cache           -> the exact way back still refuses
#   (f) the two normalisers         -> C++ and Python agree on the TEXT
#
#   bash tests/spells/lodgen_incremental.sh
#   EXE=... DATA=... ESM=... REGION='-24 16 -17 23' bash ...
set -u
R="$(cd "$(dirname "$0")/../.." && pwd)"
EXE="${EXE:-$R/release/NifSkope.exe}"
DATA="${DATA:-E:/Tools/Fallout 4/DataUnpacked/Data}"
ESM="${ESM:-X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm}"
REGION="${REGION:--24 16 -17 23}"
DIM="${DIM:-4}"
W="${WW_INCR_WORK:-$R/scratchpad/incr_gate_work}"

FAIL=0
bad ()     { echo "  RED     $*"; FAIL=$((FAIL+1)); }
note ()    { echo "  ok      $*"; }
vacuous () { echo "  VACUOUS $*"; FAIL=$((FAIL+1)); }
skip ()    { echo "  skip    $*"; }

if tasklist 2>/dev/null | grep -qi "Fallout4.exe"; then
	echo "REFUSED: Fallout4.exe is up; no exe launches while the game runs"; exit 3
fi
[ -x "$EXE" ] || { echo "no exe at $EXE"; exit 2; }
[ -f "$ESM" ] || { echo "no plugin at $ESM"; exit 2; }

echo "lodgen_incremental: $EXE"
ls -l "$EXE" | sed 's/^/   /'
echo "   region $REGION dim $DIM"
rm -rf "$W"; mkdir -p "$W"

win () { ( cd "$1" && { pwd -W 2>/dev/null || pwd; } ); }

# bake <logname> <root> [extra args...] -- one bake into <root>, FO4CS or stock
# decided by the caller passing --native or not.
bake () {
	local name="$1" root="$2"; shift 2
	mkdir -p "$root/tex"
	local rr; rr="$(win "$root")"
	local t0 t1; t0=$(date +%s%N)
	# shellcheck disable=SC2086
	"$EXE" -no-gui lodgen "$ESM" --worldspace 3C \
		--terrain-region $REGION --dim "$DIM" --data-root "$DATA" \
		--out-dir "$rr/out" --tex-dir "$rr/tex" \
		--cover --roads --road-detail 1 "$@" \
		> "$W/$name.log" 2>&1
	local rc=$?
	t1=$(date +%s%N)
	echo "   [$name] rc=$rc  $(( (t1-t0)/1000000 )) ms"
	return $rc
}

# treesha <dir> -- one sha1 a file, path-relative, sorted; the record excepted
treesha () {
	( cd "$1" 2>/dev/null || exit 0
	  find . -type f ! -name '*.lodb' ! -name 'bake.log' | LC_ALL=C sort \
	    | while IFS= read -r f; do printf '%s  %s\n' "$(sha1sum "$f" | cut -c1-40)" "$f"; done )
}

recof () { find "$1" -name '*.lodb' 2>/dev/null | head -1; }
num ()   { echo "${1:-0}" | grep -oE '^[0-9]+' || echo 0; }

# ======================================================= (a) + (b) FO4CS first
echo
echo "(a)(b) THE FO4CS TARGET -- the ruled pipeline, and the one --incremental refused"
FO="$W/fo4cs"
mkdir -p "$FO/nat"
NAT="$(win "$FO/nat")"
if ! bake fo_full "$FO" --native "$NAT"; then
	bad "(a) the full FO4CS bake failed; see $W/fo_full.log"
else
	LODO="$(find "$FO/nat" -name '*.lodo' | head -1)"
	LODI="$(find "$FO/nat" -name '*.lodi' | head -1)"
	NJ_FULL="$(find "$FO/nat" -name '*.lodj' | wc -l)"
	SHA_FULL="$(sha1sum "$LODO" "$LODI" 2>/dev/null | cut -c1-40 | tr '\n' ' ')"
	echo "      full: $NJ_FULL .lodj, pair $SHA_FULL"
	cp -p "$(recof "$FO/out")" "$W/rec_full.lodb" 2>/dev/null || true
	if [ "$NJ_FULL" -lt 1 ]; then
		vacuous "(a) the full bake wrote no .lodj, so nothing can be replayed"
	else
		OUTW="$(win "$FO/out")"
		bake fo_null "$FO" --native "$NAT" --incremental "$OUTW"
		RCA=$?
		REP="$(num "$(grep -oE '[0-9]+ replayed from cache' "$W/fo_null.log" | head -1)")"
		DIRTY="$(grep -oE '^incremental: [0-9]+ of [0-9]+' "$W/fo_null.log" | head -1 | awk '{print $2}')"
		SHA_NULL="$(sha1sum "$LODO" "$LODI" 2>/dev/null | cut -c1-40 | tr '\n' ' ')"
		echo "      null incremental: rc=$RCA, dirty=${DIRTY:-?}, replayed=$REP"
		cp -p "$(recof "$FO/out")" "$W/rec_null.lodb" 2>/dev/null || true
		[ "$RCA" -eq 0 ] || bad "(a) --incremental --native was refused (rc=$RCA)"
		if [ "$REP" -lt 1 ]; then
			vacuous "(a) nothing was replayed from cache: the comparison tests nothing"
		elif [ "$SHA_FULL" = "$SHA_NULL" ]; then
			note "(a) a null incremental rewrites the SAME .lodo/.lodi ($REP chunk(s) cached)"
		else
			bad "(a) the null incremental changed the pair: $SHA_FULL -> $SHA_NULL"
		fi

		# ---- (b) one chunk dirty beside cached ones
		VICTIM="$(find "$FO/nat" -name '*.lodj' | LC_ALL=C sort | head -1)"
		rm -f "$VICTIM"
		bake fo_mixed "$FO" --native "$NAT" --incremental "$OUTW"
		RCB=$?
		REPB="$(num "$(grep -oE '[0-9]+ replayed from cache' "$W/fo_mixed.log" | head -1)")"
		WRB="$(num "$(grep -oE '^native cache: [0-9]+' "$W/fo_mixed.log" | head -1 | awk '{print $3}')")"
		DIRTYB="$(grep -oE '^incremental: [0-9]+ of [0-9]+' "$W/fo_mixed.log" | head -1 | awk '{print $2}')"
		SHA_MIX="$(sha1sum "$LODO" "$LODI" 2>/dev/null | cut -c1-40 | tr '\n' ' ')"
		echo "      mixed: rc=$RCB, dirty=${DIRTYB:-?}, rebaked=$WRB, replayed=$REPB"
		[ "$RCB" -eq 0 ] || bad "(b) the mixed incremental was refused (rc=$RCB)"
		if [ "$REPB" -lt 1 ] || [ "$WRB" -lt 1 ]; then
			vacuous "(b) the mix never happened: $WRB rebaked, $REPB cached"
		elif [ "$SHA_FULL" = "$SHA_MIX" ]; then
			note "(b) $WRB chunk(s) rebuilt beside $REPB cached -> the SAME pair"
		else
			bad "(b) the mixed incremental changed the pair: $SHA_FULL -> $SHA_MIX"
		fi
		[ -f "$VICTIM" ] && note "(b) the deleted cache healed itself" \
			|| bad "(b) the deleted cache was not rewritten"
	fi
fi

# ================================================================= (c) STOCK
echo
echo "(c) THE STOCK TARGET -- the chunks and the sheets on disk"
ST="$W/stock"
if ! bake st_full "$ST"; then
	bad "(c) the full stock bake failed; see $W/st_full.log"
else
	treesha "$ST/out" > "$W/st_full.sha"
	treesha "$ST/tex" >> "$W/st_full.sha"
	NF="$(wc -l < "$W/st_full.sha")"
	OUTW="$(win "$ST/out")"
	bake st_null "$ST" --incremental "$OUTW"
	RCC=$?
	treesha "$ST/out" > "$W/st_null.sha"
	treesha "$ST/tex" >> "$W/st_null.sha"
	echo "      $NF file(s) compared, rc=$RCC"
	[ "$RCC" -eq 0 ] || bad "(c) the stock null incremental failed (rc=$RCC)"
	if [ "$NF" -lt 1 ]; then
		vacuous "(c) the full stock bake left no files to compare"
	elif diff -q "$W/st_full.sha" "$W/st_null.sha" > /dev/null; then
		note "(c) every one of $NF stock file(s) is byte for byte what the full bake wrote"
	else
		bad "(c) the stock incremental changed $(diff "$W/st_full.sha" "$W/st_null.sha" \
			| grep -c '^<') file(s)"
		diff "$W/st_full.sha" "$W/st_null.sha" | head -6 | sed 's/^/        /'
	fi
fi

# ======================================================== (d) the record's substance
echo
echo "(d) THE RECORD'S SUBSTANCE -- what an incremental run may NOT change"
# THE SUBSTANCE is every normalised line that is not a `census` line and not a
# `switch` line: the header, the plugins, the load order, the five corpus
# hashes, the chunk rows and the out rows with their digests. A `census` line
# states what THIS RUN did, and an incremental run legitimately did different
# work (0 chunk jobs, 4 replayed, no textures); a `switch` line legitimately
# carries --incremental itself. Everything else describes what is ON DISK, and
# an incremental run that changes any of it has lied about the disk.
subst () {
	python "$R/tests/spells/lodb_read.py" "$1" --normalise 2>/dev/null | grep -v "^census	" | grep -v "^switch	"
}
if [ ! -f "$W/rec_full.lodb" ] || [ ! -f "$W/rec_null.lodb" ]; then
	skip "(d) no pair of records on disk (arm (a) makes them)"
else
	subst "$W/rec_full.lodb" > "$W/subst_full.txt"
	subst "$W/rec_null.lodb" > "$W/subst_null.txt"
	SL="$(wc -l < "$W/subst_full.txt")"
	SC="$(grep -c "^chunk	" "$W/subst_full.txt")"
	SO="$(grep -c "^out	" "$W/subst_full.txt")"
	echo "      substance: $SL line(s), $SC chunk row(s), $SO out row(s)"
	if [ "$SL" -lt 1 ] || [ "$SC" -lt 1 ] || [ "$SO" -lt 1 ]; then
		vacuous "(d) the substance has no chunk or out rows: it masks everything"
	elif diff -q "$W/subst_full.txt" "$W/subst_null.txt" > /dev/null; then
		note "(d) the incremental record says exactly what the full one says about the disk"
	else
		bad "(d) the incremental record changed the substance"
		diff "$W/subst_full.txt" "$W/subst_null.txt" | head -8 | sed 's/^/        /'
	fi
	# THE REFUTER. A comparison that cannot fail is not a comparison. Two
	# injections into a COPY of the record -- one chunk row deleted, one out
	# digest zeroed -- must each make that same comparison differ. If they do
	# not, the filter above is swallowing the very thing it is there to watch.
	awk '/^chunk\t/ && !d { d=1; next } { print }' "$W/rec_null.lodb" > "$W/rec_refute1.lodb"
	awk -F"\t" 'BEGIN{OFS="\t"} /^out\t/ && !d { $NF="000000000000"; d=1 } { print }' "$W/rec_null.lodb" > "$W/rec_refute2.lodb"
	RF=0
	for k in 1 2; do
		subst "$W/rec_refute$k.lodb" > "$W/subst_refute$k.txt"
		if diff -q "$W/subst_full.txt" "$W/subst_refute$k.txt" > /dev/null; then
			bad "(d) REFUTER $k: the comparison did NOT notice the injection"
			RF=$((RF+1))
		fi
	done
	[ "$RF" -eq 0 ] && note "(d) and it notices a deleted chunk row and a changed out digest"
fi

# ============================================== (e) the exact way back still refuses
echo
echo "(e) --no-native-cache: the exact way back (CONSTITUTION 10)"
# The way back has to be tested against a record baked WITH it. Adding
# --no-native-cache to a run whose record was written without it changes the
# SWITCH DIGEST, and THAT refusal fires first and says so -- correct, but a
# different refusal, and taking it for this one is how this arm first read as
# a pass while proving nothing about the cache at all.
WB="$W/wayback"
mkdir -p "$WB/nat"
WBNAT="$(win "$WB/nat")"
if ! bake wb_full "$WB" --native "$WBNAT" --no-native-cache; then
	bad "(e) the full bake with --no-native-cache failed"
else
	NJ_WB="$(find "$WB/nat" -name '*.lodj' | wc -l)"
	echo "      full with --no-native-cache: $NJ_WB .lodj on disk"
	if [ "$NJ_WB" -eq 0 ]; then
		note "(e) with the flag, not one .lodj is written"
	else
		bad "(e) --no-native-cache still wrote $NJ_WB .lodj file(s)"
	fi
	WBOUT="$(win "$WB/out")"
	bake wb_incr "$WB" --native "$WBNAT" --incremental "$WBOUT" --no-native-cache
	RCE=$?
	if [ "$RCE" -eq 0 ]; then
		bad "(e) --no-native-cache --incremental --native did NOT refuse (rc=0)"
	else
		note "(e) it refuses (rc=$RCE)"
		if grep -q -- "--no-native-cache" "$W/wb_incr.log"; then
			note "(e) and the refusal names the flag that caused it"
		else
			bad "(e) the refusal does not name --no-native-cache"
			grep -i refused "$W/wb_incr.log" | head -2 | sed 's/^/        /'
		fi
	fi
fi

# ================================================ (f) the two normalisers agree
echo
echo "(f) THE TWO NORMALISERS -- C++ and Python on the same file"
RECF="$(recof "$FO/nat")"
[ -z "$RECF" ] && RECF="$(recof "$FO/out")"
if [ -z "$RECF" ]; then
	skip "(f) no bake record on disk"
else
	"$EXE" -no-gui lodgen "$ESM" --worldspace 3C --data-root "$DATA" \
		--bake-record "$RECF" > "$W/norm.log" 2>&1
	CPP_N="$(grep -oE '^bake-record normalisedLines [0-9]+' "$W/norm.log" | awk '{print $3}')"
	CPP_H="$(grep -oE '^bake-record normalisedSha1 [0-9a-f]+' "$W/norm.log" | awk '{print $3}')"
	# The digest is taken INSIDE python, over the encoded bytes, and the paths
	# are handed over in WINDOWS form -- a Git Bash /e/... path is not a path to
	# the Windows python this tree uses. Piping the text and hashing the pipe is
	# how this arm first came up red: a text-mode stdout on Windows turns every
	# LF into CRLF, so the two halves agreed on all 77 lines and disagreed on the
	# sha1. Line endings are measured in bytes, never by eye.
	SPW="$(win "$R/tests/spells")"
	RECW="$(win "$(dirname "$RECF")")/$(basename "$RECF")"
	PYOUT="$(python -c "import sys, hashlib; sys.path.insert(0, sys.argv[1]); import lodb_read; t = lodb_read.normalise_file(sys.argv[2]); sys.stdout.write(hashlib.sha1(t.encode()).hexdigest() + chr(32) + str(len(t.split(chr(10))) - 1))" "$SPW" "$RECW" 2>"$W/pynorm.err")"
	PY="$(echo "$PYOUT" | awk '{print $1}')"
	PY_N="$(echo "$PYOUT" | awk '{print $2}')"
	echo "      C++  lines=${CPP_N:-?} sha1=${CPP_H:-?}"
	echo "      py   lines=${PY_N:-?} sha1=${PY:-?}"
	if [ -z "${CPP_H:-}" ]; then
		vacuous "(f) the exe printed no normalisedSha1: nothing was compared"
	elif [ "$CPP_H" = "$PY" ]; then
		note "(f) the two normalisers agree on the TEXT, not merely on the rule"
	else
		bad "(f) the two normalisers disagree: $CPP_H vs $PY"
	fi
fi

# =========================== (g) --incremental spelled without its directory
echo
echo "(g) --incremental with no value: it must refuse, not full-bake"
# `--incremental` TAKES a directory. Spelled as the last token it used to parse
# to an empty string, and an empty gLgIncremental skips the whole incremental
# block -- every refusal with it -- so the run full-baked and exited 0 while the
# operator was watching the clock for a cached one. The census said
# `native-library-build: rebuilt (not offered: this is not an incremental bake)`
# and nothing else did.
GD="$W/novalue"
mkdir -p "$GD/tex"
GDW="$(win "$GD")"
"$EXE" -no-gui lodgen "$ESM" --worldspace 3C --terrain-region $REGION --dim 4 \
	--data-root "$DATA" --out-dir "$GDW" --tex-dir "$GDW/tex" --native "$GDW" \
	--incremental > "$W/novalue.log" 2>&1
RCG=$?
NG="$(find "$GD" -type f -not -path "$GD/tex/*" | wc -l)"
echo "      rc=$RCG, $NG file(s) written under the out-dir"
if [ "$RCG" -eq 0 ]; then
	bad "(g) --incremental with no value did NOT refuse (rc=0) and wrote $NG file(s)"
elif ! grep -qa -- "--incremental" "$W/novalue.log"; then
	bad "(g) it exits non-zero but the message never names --incremental"
	head -3 "$W/novalue.log" | sed 's/^/        /'
elif [ "$NG" -gt 0 ]; then
	# the floor: refusing AFTER baking is not refusing
	bad "(g) it refused but still wrote $NG file(s) -- the refusal must come before the bake"
else
	note "(g) it refuses (rc=$RCG), names the flag, and writes nothing"
fi

echo
echo "FAILURES: $FAIL"
[ "$FAIL" -eq 0 ] && exit 0 || exit 1
