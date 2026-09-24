#!/bin/sh
# LAND1 gate B2 -- THE REFUSALS.
#
# --incremental has four ways to say no, and a refusal is only worth anything if
# it (1) names itself, (2) exits non-zero and (3) BAKES NOTHING.  The third is
# the one a reader would take on trust and the one that matters: a refusal that
# had already written half a chunk would leave an output tree nobody could
# reason about.  So every arm below runs against an EMPTY out-dir and asserts
# the directory is still empty afterwards.
#
#   sh b2_refusals.sh <exe>   ->  logs/b2.txt
set -u
R="$(cd "$(dirname "$0")/../.." && pwd)"
EXE="${1:-$R/release/NifSkope.exe}"
DATA="E:/Tools/Fallout 4/DataUnpacked/Data"
VANILLA="X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm"
# THE SAME PLUGIN PATH gate B3's base ledger was written with. The switch
# digest covers the argument vector and the plugin path is one of its arguments,
# so naming the vanilla path here made every arm refuse with "the switches
# differ" -- two arms failed on it and, worse, the switches arm PASSED on it.
ESM="$R/scratchpad/land1_20260912/out/b3/esm/A.esm"
W="$R/scratchpad/land1_20260912/out/b2"
LOG="$R/scratchpad/land1_20260912/logs/b2.txt"
GOOD="$R/scratchpad/land1_20260912/out/b3/A/base/obj"   # a real ledger to diff against
# The region gate B3's base ledger describes, EXACTLY. It was "-24 16 -13 27"
# until 09:01 on 2026-09-12, left behind when B3's regions grew from 3x3 chunks
# to 5x5: every arm then tripped the wrong-shape refusal before reaching its own,
# and three of five failed for a reason unrelated to what they measure.
REG="-24 16 -5 35"

if tasklist 2>/dev/null | grep -qi Fallout4.exe; then
	echo "REFUSED: Fallout4.exe is up; no exe may be launched." ; exit 3
fi
[ -x "$EXE" ] || { echo "no exe at $EXE"; exit 2; }
[ -f "$GOOD/Commonwealth.lodb" ] || { echo "gate B2 needs gate B3's base bake first"; exit 2; }
# Restore the live plugin to the bytes the base ledger was written from: B3's
# later arms copy edited variants over this path, and an edited plugin would
# make the merge-ok control report dirty chunks for a real reason.
cp -f "$VANILLA" "$ESM" || { echo "cannot restore $ESM"; exit 2; }

fails=0
echo "LAND1 gate B2 -- the refusals" > "$LOG"
date +%H:%M >> "$LOG"
ls -l "$EXE" | sed 's/^/   /' >> "$LOG"

arm() {   # arm <name> <expected phrase> <ledger-dir> <region> [extra...]
	name="$1"; want="$2"; led="$3"; reg="$4"; shift 4
	d="$W/$name"
	rm -rf "$d"; mkdir -p "$d/obj" "$d/tex"
	# shellcheck disable=SC2086
	"$EXE" -no-gui lodgen "$ESM" --worldspace 3C \
		--terrain-region $reg --dim 4 \
		--out-dir "$d/obj" --tex-dir "$d/tex" --data-root "$DATA" \
		--cover --roads --road-detail 1 --incremental "$led" "$@" \
		> "$d/run.log" 2>&1
	rc=$?
	n=$(find "$d/obj" "$d/tex" -type f 2>/dev/null | wc -l)
	line=$(grep -m1 "^refused:" "$d/run.log" | cut -c1-100)
	ok=yes
	[ "$rc" = 0 ] && ok=no
	# The refusal must be the one this arm asked for. Without this line every
	# arm passes on any refusal at all, which is how a stale REG made four
	# different arms report the same wrong-shape message.
	grep -q "$want" "$d/run.log" || ok=no
	[ "$n" = 0 ] || ok=no
	[ "$ok" = yes ] || fails=$((fails + 1))
	printf '%-14s rc=%-3s files-written=%-3s %s\n' "$name" "$rc" "$n" \
		"$( [ "$ok" = yes ] && echo PASS || echo FAIL )" | tee -a "$LOG"
	echo "               $line" | tee -a "$LOG"
	grep -m1 "^  " "$d/run.log" | cut -c1-100 | sed 's/^/               /' | tee -a "$LOG"
}

echo "" | tee -a "$LOG"

# 1. no ledger at all -- the empty directory case.
mkdir -p "$W/_empty"
arm no-ledger "nothing to diff against" "$W/_empty" "$REG"

# 2. the ledger describes a different region.
arm wrong-shape "a different shape" "$GOOD" "-24 16 -9 31"

# 3. the switches differ -- one extra flag that CAN reach an output byte.
arm switches "the switches differ" "$GOOD" "$REG" --land-guide aspecthex:1.0

# 4. a whole-region product is asked for.
#
#    THE SETUP IS THE POINT. --atlas is not on the switch-digest skip list, so
#    against $GOOD (baked without it) this arm would refuse with "the switches
#    differ" and never reach the check it exists to test. So bake the region
#    ONCE with --atlas first -- that writes a ledger carrying the --atlas digest
#    -- and only then ask for an incremental run of the same command.
ATL="$W/_atlasledger"
if [ ! -f "$ATL/obj/Commonwealth.lodb" ]; then
	rm -rf "$ATL"; mkdir -p "$ATL/obj" "$ATL/tex"
	echo "   (baking a ledger WITH --atlas so the whole-region arm is reachable)" | tee -a "$LOG"
	# shellcheck disable=SC2086
	"$EXE" -no-gui lodgen "$ESM" --worldspace 3C --terrain-region $REG --dim 4 \
		--out-dir "$ATL/obj" --tex-dir "$ATL/tex" --data-root "$DATA" \
		--cover --roads --road-detail 1 --atlas \
		> "$ATL/bake.log" 2>&1
	[ -f "$ATL/obj/Commonwealth.lodb" ] || {
		echo "whole-region    SKIPPED -- the --atlas base bake failed, see $ATL/bake.log" \
			| tee -a "$LOG"; }
fi
arm whole-region "build ONE region-wide product" "$ATL/obj" "$REG" --atlas

# 5. the merge, which is ON BY DEFAULT, must NOT refuse. This arm is the
#    negative control for arm 4: without it, arm 4 would pass just as happily
#    on a build that refused everything.
d="$W/merge-ok"
rm -rf "$d"; mkdir -p "$d/obj" "$d/tex"
cp "$GOOD"/* "$d/obj/" 2>/dev/null
cp "$R/scratchpad/land1_20260912/out/b3/A/base/tex"/* "$d/tex/" 2>/dev/null
# shellcheck disable=SC2086
"$EXE" -no-gui lodgen "$ESM" --worldspace 3C --terrain-region $REG --dim 4 \
	--out-dir "$d/obj" --tex-dir "$d/tex" --data-root "$DATA" \
	--cover --roads --road-detail 1 --incremental "$d/obj" \
	> "$d/run.log" 2>&1
rc=$?
if [ "$rc" = 0 ] && grep -q "^incremental:" "$d/run.log"; then
	echo "merge-ok       rc=0   the default merge does NOT refuse           PASS" | tee -a "$LOG"
	grep -m1 "^incremental:" "$d/run.log" | sed 's/^/               /' | tee -a "$LOG"
else
	echo "merge-ok       rc=$rc  FAIL -- the default command cannot run incrementally" | tee -a "$LOG"
	grep -m1 "^refused:" "$d/run.log" | cut -c1-100 | sed 's/^/               /' | tee -a "$LOG"
	fails=$((fails + 1))
fi

echo "" | tee -a "$LOG"
echo "B2: 5 arms, $fails failure(s)" | tee -a "$LOG"
[ "$fails" = 0 ] && echo "RESULT PASS" | tee -a "$LOG" || echo "RESULT FAIL" | tee -a "$LOG"
exit 0
