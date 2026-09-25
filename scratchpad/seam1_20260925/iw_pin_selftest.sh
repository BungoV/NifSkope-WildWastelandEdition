#!/bin/bash
# The re-pinned impostor_wind G2 must still fail: run the spell's own G2 lines (cut from the tracked file)
# over copies of the kept hero/heroprev sets: as is (attributed), one more byte in _oct_n (DIFF), one byte in _oct_d (DIFF).
W=/e/Projects/NifskopeWWE-seam1; S=$W/scratchpad/seam1_20260925; K=$W/scratchpad/cardfix1_20260924/wind
FID=000531b3; G2=$(sed -n '/^BC7_ATTR=/,/^case "\$v" in \*DIFF\*/p' $W/tests/spells/impostor_wind.sh)
ok() { echo "  ok   $1"; }; bad() { echo "  FAIL $1"; }
for m in asis oct_n oct_d; do
  WORK=$S/iwpin; rm -rf $WORK; mkdir -p $WORK; cp -r $K/hero $K/heroprev $WORK/
  case $m in oct_n) f=$WORK/hero/cards/${FID}_oct_n.DDS ;; oct_d) f=$WORK/hero/cards/${FID}_oct_d.DDS ;; *) f= ;; esac
  [ -n "$f" ] && { b=$(od -An -tu1 -j 4000 -N1 "$f" | tr -d ' '); printf "\$(printf %03o $(( (b + 1) & 255 )))" | dd of="$f" bs=1 seek=4000 conv=notrunc 2>/dev/null; }
  echo "$m:"; eval "$G2"
done
rm -rf $S/iwpin
