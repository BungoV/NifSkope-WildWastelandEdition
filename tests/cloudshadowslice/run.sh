#!/bin/bash
# lane PERFFIX3 (item 4): the cloud shadow map's time-sliced rebuild. Green must PASS; the red (-DCLOUDSHADOWSLICE_RED,
# CLOUD3's whole-map rebuild on a moved fit) must FAIL.   usage: bash tests/cloudshadowslice/run.sh <scratch dir>
cd "$(dirname "$0")/../.." || exit 2
OUT="${1:?scratch dir}"
mkdir -p "$OUT"
T=$(pwd -W 2>/dev/null || pwd)
rc=0
for arm in green red; do
	def=""; [ $arm = red ] && def="-DCLOUDSHADOWSLICE_RED"
	MSYSTEM=UCRT64 CHERE_INVOKING=1 /c/msys64/usr/bin/bash -lc "cd '$T' && g++ -O2 -std=c++17 $def -I src tests/cloudshadowslice/cloudshadowslice_test.cpp -o '$OUT/cloudshadowslice_$arm.exe'" || { echo "BUILD $arm FAILED"; exit 2; }
	PATH=/c/msys64/ucrt64/bin:$PATH "$OUT/cloudshadowslice_$arm.exe" > "$OUT/cloudshadowslice_$arm.txt" 2>&1
	r=$?
	tail -1 "$OUT/cloudshadowslice_$arm.txt" | sed "s/^/$arm: /"
	if [ $arm = green ] && [ $r != 0 ]; then rc=1; fi
	if [ $arm = red ] && [ $r = 0 ]; then echo "RED DID NOT FAIL"; rc=1; fi
done
echo "CLOUDSHADOWSLICE run rc=$rc"
exit $rc
