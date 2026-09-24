#!/bin/bash
#
# The LOOSE-FILE EXTENSION WHITELIST (lane GENSMALL1, 2026-09-16).
#
# `lib/libfo76utils/src/ba2file.cpp` decides which loose files a `--resource
# <dir>` may serve, by packing the extension six bits a character into a
# quint64 and switching on it. A file whose extension is not a `case` is
# skipped IN SILENCE -- no warning, no census line -- so `.pbrm` and `.lodm`
# beside a model were invisible to a loose-file run while the same two
# extensions inside a `.ba2` were served normally. This spell is the pin on
# the two new cases and, just as much, on the seventeen that must not move.
#
# WHY A LISTING AND NOT A BAKE. `--resource <dir>` REPLACES the stack, so the
# index a `--list-files N` prints over a scratch directory is exactly the
# filter's output and nothing else: 19 files go in, the count that comes out
# is the number of extensions the switch admits. That makes every check below
# an integer with a stated floor rather than a "seems to work".
#
# THE LEGS
#   1. all nineteen whitelisted extensions are served, by name
#   2. an extension that is NOT whitelisted (`.zzz`) is still refused -- the
#      floor that says the filter is a filter and not an "accept everything"
#   3. the case fold: `PROBE2.PBRM` is served by the same constant as
#      `probe.pbrm` (`c = (c & 0x1F) | ((c & 0x40) >> 1)`)
#   4. THE REFUTER, on the rung exe: the same directory, the same command, and
#      `.pbrm` / `.lodm` are missing with no message of any kind. A gate that
#      is green on the binary it replaces is measuring something else
#      (`ww-spec-gate-audit`), so this leg has to go RED on the rung.
#   5. EVERY OTHER EXTENSION BYTE-IDENTICAL: a stock bake of Sanctuary
#      (-20,24) dim 4 under both exes, `cmp`'d file for file. A filter change
#      may not move a single byte of a bake that uses none of it.
#
# Legs 4 and 5 need the rung exe. Without it they SKIP BY NAME rather than
# passing quietly.
#
# USAGE
#   bash tests/spells/resource_ext.sh
#   RUNG=<old exe> bash tests/spells/resource_ext.sh
#   OUT=<dir> bash tests/spells/resource_ext.sh      # keep the scratch tree

set -u

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
NS="${EXE:-$ROOT/release/NifSkope.exe}"
RUNG="${RUNG:-$ROOT/release/NifSkope.before_gensmall1.exe}"
ESM="${ESM:-X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm}"
DATA="${DATA:-E:/Tools/Fallout 4/DataUnpacked/Data}"
KEEP="${OUT:-}"
if [ -n "$KEEP" ]; then W="$KEEP"; mkdir -p "$W"; else W="$(mktemp -d)"; trap 'rm -rf "$W"' EXIT; fi
# The CLI resolves a RELATIVE output path against release/, never the shell's
# cwd. The grouping braces keep `pwd -W` from printing the path twice
# (lodgen_native.sh has the full story).
WA="$(cd "$W" && { pwd -W 2>/dev/null || pwd; })"

[ -x "$NS" ] || { echo "no NifSkope.exe at $NS"; exit 2; }
[ -f "$ESM" ] || { echo "no plugin at $ESM"; exit 2; }

checks=0
fails=0
skips=0
note () { checks=$((checks+1)); echo "  ok   $1"; }
bad ()  { checks=$((checks+1)); fails=$((fails+1)); echo "  FAIL $1"; }
skip () { skips=$((skips+1)); echo "  SKIP $1"; }

# The nineteen the switch admits, read off ba2file.cpp's own comments.
# `ba2` and `bsa` are in the switch as ARCHIVES (they set a negative fileSize
# and are mounted, not listed), so they are not part of the loose count.
EXTS="bgem bgsm bmp btd bto btr cdb dds dlstrings hdr ilstrings kf lodm mat mesh nif pbrm strings tga"
NEXTS=19

mkdir -p "$W/res/materials/wwtest"
for e in $EXTS; do
	# ba2file.cpp: `if (f.fileSize < 1) continue;` -- an EMPTY file is dropped
	# before the extension is ever looked at, and a gate written with
	# `touch` would measure that instead of the filter.
	printf 'x\n' > "$W/res/materials/wwtest/probe.$e"
done
printf 'x\n' > "$W/res/materials/wwtest/probe.zzz"
printf 'x\n' > "$W/res/materials/wwtest/PROBE2.PBRM"

listfiles () {   # $1 = exe, $2 = output file
	"$1" -no-gui lodgen "$ESM" --resource "$WA/res" --list-files 60 > "$2" 2>&1
}

echo "== 1. every whitelisted extension is served"
listfiles "$NS" "$W/new.txt"
N="$(sed -n 's/^files: \([0-9][0-9]*\)$/\1/p' "$W/new.txt" | head -1)"
N="${N:-0}"
missing=""
for e in $EXTS; do
	grep -q "^file: materials/wwtest/probe\.$e\$" "$W/new.txt" || missing="$missing $e"
done
if [ -z "$missing" ]; then note "all $NEXTS whitelisted extensions are served"
else bad "all $NEXTS whitelisted extensions are served (missing:$missing)"; fi
for e in lodm pbrm; do
	if grep -q "^file: materials/wwtest/probe\.$e\$" "$W/new.txt"; then
		note "the new extension .$e is served from a loose --resource dir"
	else bad "the new extension .$e is served from a loose --resource dir"; fi
done

echo "== 2. an extension that is NOT whitelisted is still refused"
if grep -q "probe\.zzz" "$W/new.txt"; then
	bad ".zzz is refused (the filter admits everything, which is not a filter)"
else note ".zzz is refused, so the switch is still a whitelist"; fi

echo "== 3. the case fold"
if grep -qi "^file: materials/wwtest/probe2\.pbrm\$" "$W/new.txt"; then
	note "PROBE2.PBRM is served by the same constant as probe.pbrm"
else bad "PROBE2.PBRM is served by the same constant as probe.pbrm"; fi

# 19 probes + the uppercase one = 20; .zzz must not be among them.
if [ "$N" -eq 20 ]; then note "the index holds exactly 20 files (19 extensions + the uppercase probe)"
else bad "the index holds exactly 20 files (19 extensions + the uppercase probe); it holds $N"; fi

echo "== 4. the refuter: the rung ignores both, in silence"
if [ -x "$RUNG" ]; then
	listfiles "$RUNG" "$W/rung.txt"
	RN="$(sed -n 's/^files: \([0-9][0-9]*\)$/\1/p' "$W/rung.txt" | head -1)"
	RN="${RN:-0}"
	rmiss=0
	for e in lodm pbrm; do
		grep -q "^file: materials/wwtest/probe\.$e\$" "$W/rung.txt" || rmiss=$((rmiss+1))
	done
	if [ "$rmiss" -eq 2 ]; then
		note "the rung serves NEITHER .pbrm nor .lodm (so this gate can fail)"
	else bad "the rung serves NEITHER .pbrm nor .lodm: it served $((2-rmiss)) of them, so the gate is vacuous"; fi
	# TWO numbers, because either alone can pass on the wrong binary: the rung
	# must hold 17 AND this exe must hold exactly three more.
	if [ "$RN" -eq 17 ] && [ "$((N-RN))" -eq 3 ]; then
		note "the rung's index holds 17 files and this exe's holds $N, exactly three more"
	else bad "the rung's index holds 17 and this exe three more: rung $RN, this exe $N"; fi
	# and it says NOTHING about the two it dropped
	if grep -qiE 'pbrm|lodm' "$W/rung.txt"; then
		bad "the rung drops them in SILENCE (it printed something about them)"
	else note "the rung drops them in SILENCE -- no warning, which is the defect"; fi
else
	skip "the refuter needs the rung exe at $RUNG"
	skip "the rung's file count"
	skip "the rung's silence"
fi

echo "== 5. every other extension byte-identical: a stock bake under both exes"
if [ -x "$RUNG" ]; then
	mkdir -p "$W/bake_new" "$W/bake_rung"
	"$NS" -no-gui lodgen "$ESM" --worldspace 3C --terrain-region -20 24 -17 27 --dim 4 \
		--data-root "$DATA" --road-detail 1 --out-dir "$WA/bake_new" > "$W/bake_new.log" 2>&1
	rc1=$?
	"$RUNG" -no-gui lodgen "$ESM" --worldspace 3C --terrain-region -20 24 -17 27 --dim 4 \
		--data-root "$DATA" --road-detail 1 --out-dir "$WA/bake_rung" > "$W/bake_rung.log" 2>&1
	rc2=$?
	if [ $rc1 -eq 0 ] && [ $rc2 -eq 0 ]; then note "both exes baked (-20,24) dim 4"
	else bad "both exes baked (-20,24) dim 4 (rc $rc1 / $rc2)"; fi
	nfiles=0
	ndiff=0
	for f in "$W/bake_new"/*; do
		[ -f "$f" ] || continue
		b="$(basename "$f")"
		nfiles=$((nfiles+1))
		cmp -s "$f" "$W/bake_rung/$b" || { ndiff=$((ndiff+1)); echo "       differs: $b"; }
	done
	if [ "$nfiles" -gt 0 ]; then note "the bake wrote $nfiles files to compare"
	else bad "the bake wrote $nfiles files to compare (nothing was compared)"; fi
	if [ "$ndiff" -eq 0 ]; then note "$nfiles of $nfiles files byte-identical: the filter moved no other extension"
	else bad "$ndiff of $nfiles files differ: the filter moved something it should not have"; fi
else
	skip "the byte-identity leg needs the rung exe at $RUNG"
	skip "the byte-identity comparison"
	skip "the bake file count"
fi

echo
echo "$checks checks, $fails failures, $skips skipped"
[ "$fails" -eq 0 ] && echo "RESULT PASS" || echo "RESULT FAIL"
[ "$fails" -eq 0 ] || exit 1
exit 0
