#!/usr/bin/env bash
# Lane CARDS-AGG, gate A6: the STANDALONE gate for the .lodi v4 aggregate
# layout (ww-standalone-writer-gate).
#
#   1. write the hand-written known-answer set;
#   2. read it back with every check on;
#   3. write it twice -- byte identity;
#   4. mutate one byte at a time, RE-SIGNING the CRCs that cover it so the ROW
#      RULE is what refuses, and require the refusal to NAME its rule.
#
# Every mutation carries the substring its refusal MUST contain. A file that is
# merely rejected proves nothing: what matters is WHICH rule rejected it.
set -u
cd "$(dirname "$0")" || exit 2
FIX=./aggfixture.exe
W=./gate/aggfix
rm -rf "$W"; mkdir -p "$W"
run() { MSYSTEM=UCRT64 CHERE_INVOKING=1 /c/msys64/usr/bin/bash -lc "cd '$PWD' && $*" 2>&1; }

CHECKS=0; FAILS=0
ok()  { CHECKS=$((CHECKS+1)); echo "ok   $1"; }
bad() { CHECKS=$((CHECKS+1)); FAILS=$((FAILS+1)); echo "FAIL $1"; }

echo "== 1. the known-answer fixture"
out=$(run "$FIX $W")
echo "$out"
[ -s "$W/Agg.lodi" ] && ok "Agg.lodi written" || bad "Agg.lodi written"
[ -s "$W/Agg.expect.txt" ] && ok "the expectations were written BEFORE the read-back" \
  || bad "the expectations were written BEFORE the read-back"

echo "== 2. read back, every check on"
vout=$(run "$FIX --verify $W")
echo "$vout"
line=$(echo "$vout" | tail -1)
echo "$line" | grep -q "0 failures" && ok "read-back: $line" || bad "read-back: $line"

echo "== 3. two writes are byte-identical"
mkdir -p "$W/again"
run "$FIX $W/again" > /dev/null
if cmp -s "$W/Agg.lodi" "$W/again/Agg.lodi"; then ok "two writes of one set are byte-identical"
else bad "two writes of one set are byte-identical"; fi

echo "== 4. mutations -- the refusal must NAME its rule"
# off  xor  resign          the substring the refusal must carry
#
# 0x04 (version) is inside headerCrc32's range, so the header is re-signed for
# it; every payload mutation re-signs BOTH so indexCrc32 is not the one that
# answers. The aggregate table sits at the file's own offAggregates, read out
# of the header below.
AGGOFF=$(python -c "
import struct,sys
b=open(r'$W/Agg.lodi','rb').read()
print(struct.unpack_from('<Q',b,0xB0)[0])
")
COVOFF=$(python -c "
import struct,sys
b=open(r'$W/Agg.lodi','rb').read()
print(struct.unpack_from('<Q',b,0xB8)[0])
")
echo "   aggregate table at $AGGOFF, covered blob at $COVOFF"

mutate() {   # $1 off  $2 xor  $3 resign-flag  $4 label  $5 must-contain
  run "$FIX --mutate $W/Agg.lodi $W/m.lodi $1 $2 $3" > /dev/null
  r=$(run "$FIX --verify-file $W/m.lodi" 2>&1)
  # the fixture's --verify takes a DIRECTORY, so copy the mutant into place
  cp "$W/m.lodi" "$W/mut/Agg.lodi" 2>/dev/null || { mkdir -p "$W/mut"; cp "$W/m.lodi" "$W/mut/Agg.lodi"; }
  r=$(run "$FIX --verify $W/mut")
  if echo "$r" | grep -qi "$5"; then ok "$4 -> refused by name: $(echo "$r" | grep -i "$5" | head -1 | cut -c1-150)"
  else bad "$4 -> the refusal did not name '$5'; got: $(echo "$r" | head -2 | tr '\n' ' ' | cut -c1-180)"; fi
}

mkdir -p "$W/mut"
# the version word: 4 -> 5
mutate 4 1 --resign-header "version 4 -> 5" "this reader knows"
# the stride word at 0xC8: 48 -> 49
mutate 200 1 --resign-header "aggregateStride 48 -> 49" "aggregateStride"
# aggregateViews at 0xCA: 8 -> 9 (the header says 9, the rows still say 8)
mutate 202 1 --resign-header "aggregateViews 8 -> 9" "views against the header"
# the band ratio at 0xD0: make it 1.0 or less by clearing the exponent's top bit
mutate 211 0x40 --resign-header "aggBandRatio out of range" "aggBandRatio"
# a reserved header byte past 0xD4
mutate 216 1 --resign-header "a reserved header byte at 0xd8" "reserved header byte"
# row 0's flags word (offset 0x22 in the row): clear HEIGHT
mutate $((AGGOFF + 0x22)) 1 --resign-all "row 0 HEIGHT cleared" "HEIGHT is clear"
# row 0's flags word: set a RESERVED bit
mutate $((AGGOFF + 0x22)) 4 --resign-all "row 0 flags reserved bit" "set reserved bits"
# row 0's identity: clear the top bit
mutate $((AGGOFF + 0x27)) 0x80 --resign-all "row 0 identity top bit cleared" "identity"
# row 0's coveredCount low byte
# row 0's coveredCount 4 -> 5: it then swallows the NEXT aggregate's first
# instance, and the rule that answers is the one with teeth -- an aggregate may
# only cover trees of its OWN cell. The label says which rule, because the
# refusal text is the proof and not the label (ww-standalone-writer-gate).
mutate $((AGGOFF + 0x2C)) 1 "--resign-all" "row 0 coveredCount 4 -> 5, reaching into cell (2,1)" "stands in cell"
# row 1's coveredFirst 4 -> 5: the partition rule is the one with something to
# say here, and nothing else moves, so it is the rule that must fire.
mutate $((AGGOFF + 48 + 0x28)) 1 "--resign-all" "row 1 coveredFirst 4 -> 5" "coveredFirst"
# row 0's cellX
mutate $((AGGOFF + 0x1C)) 1 --resign-all "row 0 cellX moved" "cell"
# a covered index: point it at an instance in the other cell
mutate $((COVOFF + 0)) 4 --resign-all "a covered index points elsewhere" "cell\|ascending"
# row 0's depthSpan sign bit -- byte +3 of the field at 0x14 (the float-sign trap)
mutate $((AGGOFF + 0x17)) 0x80 --resign-all "row 0 depthSpan negative" "depthSpan"
# row 0's half[0] sign bit -- byte +3 of the field at 0x0C
mutate $((AGGOFF + 0x0F)) 0x80 --resign-all "row 0 halfW negative" "half extent"

echo
echo "$CHECKS checks, $FAILS failures, $([ "$FAILS" -eq 0 ] && echo PASS || echo FAIL)"
[ "$FAILS" -eq 0 ]
