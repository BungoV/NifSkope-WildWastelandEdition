#!/bin/bash
# lane PERFFIX3 (item 4): the coverage tables off the GL thread. Green must PASS; the red (jobs keyed on the quantised
# key, -DCLOUDCALJOBS_RED) must FAIL.   usage: bash tests/features/clouds/cloudcaljobs/run.sh <scratch dir>
cd "$(dirname "$0")/../../../.." || exit 2
OUT="${1:?scratch dir}"
mkdir -p "$OUT"
T=$(pwd -W 2>/dev/null || pwd)
rc=0
for arm in green red; do
	def=""; [ $arm = red ] && def="-DCLOUDCALJOBS_RED"
	MSYSTEM=UCRT64 CHERE_INVOKING=1 /c/msys64/usr/bin/bash -lc "cd '$T' && g++ -O2 -std=c++17 -pthread $def -I src tests/features/clouds/cloudcaljobs/cloudcaljobs_test.cpp -o '$OUT/cloudcaljobs_$arm.exe'" || { echo "BUILD $arm FAILED"; exit 2; }
	PATH=/c/msys64/ucrt64/bin:$PATH "$OUT/cloudcaljobs_$arm.exe" > "$OUT/cloudcaljobs_$arm.txt" 2>&1
	r=$?
	tail -1 "$OUT/cloudcaljobs_$arm.txt" | sed "s/^/$arm: /"
	if [ $arm = green ] && [ $r != 0 ]; then rc=1; fi
	if [ $arm = red ] && [ $r = 0 ]; then echo "RED DID NOT FAIL"; rc=1; fi
done
echo "CLOUDCALJOBS run rc=$rc"
exit $rc
