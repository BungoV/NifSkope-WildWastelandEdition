#!/bin/bash
# Lane VKBUF2 (2026-11-16): cellGiHalf / cellGiHalfKey shared with the present device by name (vkrtbuf.inc rtImg*), against
# GL's bytes. bash tests/vkframe/rtimgview.sh <out dir>. Needs MSYS2 UCRT64 g++. Not while NifSkope or the game runs.
# Also checks the app's owner (ao.cpp giShare / giHalfPass) calls the helpers the gate drives and keys no target by an
# address inside its AoGpu (a QHash value: it moves). Last line: `RTIMG GATE OK` or `RTIMG GATE FAILED`.
set -u
OUT="${1:?out dir}"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
export PATH="/c/msys64/ucrt64/bin:$PATH"
if powershell -NoProfile -Command "if (Get-Process NifSkope,Fallout4 -ErrorAction SilentlyContinue) { 'busy' }" | grep -q busy; then echo "RTIMG GATE FAILED (NifSkope or the game runs: no window of ours now)"; exit 1; fi
mkdir -p "$OUT"
APP="$ROOT/src/features/ao/ao.cpp"
nHalf=$(grep -c "rtImgShare( WwVkPresent::kRtImgGiHalf," "$APP")
nKey=$(grep -c "rtImgShare( WwVkPresent::kRtImgGiHalfKey," "$APP")
nDrop=$(grep -c "rtImgDrop( WwVkPresent::kRtImg" "$APP")
nOwner=$(grep -c "rtImgOwner( WwVkPresent::kRtImg" "$APP")
nSize=$(grep -c "rtImgSize( W, g.giStep )" "$APP")
nAddr=$(grep -cE "uintptr_t>\( &g\.(giHalf|giKey)" "$APP")
nSay=$(grep -c "WwVkPresent::rtImgSay();" "$APP")
echo "RTIMG app: rtImgShare half $nHalf (want >= 1), key $nKey (want >= 1), rtImgDrop $nDrop (want >= 2), rtImgOwner $nOwner (want >= 2), rtImgSize $nSize (want >= 1), keys by &g.giHalf / &g.giKey $nAddr (want 0), rtImgSay $nSay (want >= 1)"
appOk=1
[ "$nHalf" -ge 1 ] && [ "$nKey" -ge 1 ] && [ "$nDrop" -ge 2 ] && [ "$nOwner" -ge 2 ] && [ "$nSize" -ge 1 ] && [ "$nAddr" -eq 0 ] && [ "$nSay" -ge 1 ] || appOk=0
g++ -O2 -std=c++17 -pthread -I"$ROOT/src" "$ROOT/tests/vkframe/rtimgview.cpp" -o "$OUT/rtimgview.exe" -lopengl32 -lgdi32 -luser32 \
	|| { echo "RTIMG GATE FAILED (g++)"; exit 1; }
"$OUT/rtimgview.exe" 2>&1 | tee "$OUT/rtimgview.txt"
nExact=$(grep -c ": EXACT$" "$OUT/rtimgview.txt")
nBite=$(grep -c "FAIL (red bites)$" "$OUT/rtimgview.txt")
echo "RTIMG greens EXACT $nExact (want 18), reds biting $nBite (want 9)"
# the first said line (the app log's only one for a still view) names both images with their size
first=$(grep -m1 "^vk rtimg:" "$OUT/rtimgview.txt")
sayOk=0
[ -n "$first" ] && ! echo "$first" | grep -q " 0x0 " && sayOk=1
echo "RTIMG first said line: ${first:-(none)} -> $([ $sayOk = 1 ] && echo 'both sized: OK' || echo 'a slot unsized or no line: FAIL')"
nKept=$(grep -c "^RTIMG kept .*: OK$" "$OUT/rtimgview.txt")
echo "RTIMG kept line OK $nKept (want 1)"
if [ "$appOk" -eq 1 ] && [ "$sayOk" -eq 1 ] && [ "$nKept" -eq 1 ] && [ "$nExact" -eq 18 ] && [ "$nBite" -eq 9 ] && tail -1 "$OUT/rtimgview.txt" | grep -q "RTIMG GATE OK"; then
	echo "RTIMG GATE OK"
else
	echo "RTIMG GATE FAILED"
	exit 1
fi
