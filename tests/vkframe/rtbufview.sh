#!/bin/bash
# Lane VKBUF1 (2026-11-16): cellDdgiAtlas / cellRestir shared with the present device by name (vkrtbuf.inc), against GL's bytes.
# bash tests/vkframe/rtbufview.sh <out dir>. Needs MSYS2 UCRT64 g++. Not while NifSkope or the game runs (a window of ours).
# Also checks the app's owner (rtddgicell.cpp) calls the helper the gate drives, and keys no buffer by a struct's address.
# Last line: `RTBUF GATE OK` or `RTBUF GATE FAILED`.
set -u
OUT="${1:?out dir}"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
export PATH="/c/msys64/ucrt64/bin:$PATH"
if powershell -NoProfile -Command "if (Get-Process NifSkope,Fallout4 -ErrorAction SilentlyContinue) { 'busy' }" | grep -q busy; then echo "RTBUF GATE FAILED (NifSkope or the game runs: no window of ours now)"; exit 1; fi
mkdir -p "$OUT"
APP="$ROOT/src/features/gi/rtddgicell.cpp"
nAtlas=$(grep -c "rtBufShare( WwVkPresent::kRtBufDdgiAtlas" "$APP")
nRestir=$(grep -c "rtBufShare( WwVkPresent::kRtBufRestir" "$APP")
nCopy=$(grep -c "rtBufCopy( WwVkPresent::kRtBufRestir" "$APP")
nOwner=$(grep -c "rtBufOwner( WwVkPresent::kRtBuf" "$APP")
nAddr=$(grep -c "uintptr_t>( &K )" "$APP")
# GL binds the shared cellRestir on its unit while the view is on Vulkan, so a route matching the bound buffer finds it
nBind=$(grep -c "u.bind( WwCellUnit::Restir, GL_TEXTURE_BUFFER, tex )" "$APP")
nVkTex=$(grep -c "tex = G.vkTex" "$APP")
echo "RTBUF app: rtBufShare atlas $nAtlas (want >= 1), restir $nRestir (want >= 1), rtBufCopy restir $nCopy (want >= 1), rtBufOwner $nOwner (want >= 2), keys by &K $nAddr (want 0), restir unit binds the shared copy $nBind / $nVkTex (want >= 1 / >= 1)"
appOk=1
[ "$nAtlas" -ge 1 ] && [ "$nRestir" -ge 1 ] && [ "$nCopy" -ge 1 ] && [ "$nOwner" -ge 2 ] && [ "$nAddr" -eq 0 ] && [ "$nBind" -ge 1 ] && [ "$nVkTex" -ge 1 ] || appOk=0
g++ -O2 -std=c++17 -pthread -I"$ROOT/src" "$ROOT/tests/vkframe/rtbufview.cpp" -o "$OUT/rtbufview.exe" -lopengl32 -lgdi32 -luser32 \
	|| { echo "RTBUF GATE FAILED (g++)"; exit 1; }
"$OUT/rtbufview.exe" 2>&1 | tee "$OUT/rtbufview.txt"
nExact=$(grep -c ": EXACT$" "$OUT/rtbufview.txt")
nBite=$(grep -c "FAIL (red bites)$" "$OUT/rtbufview.txt")
echo "RTBUF greens EXACT $nExact (want 12), reds biting $nBite (want 6)"
if [ "$appOk" -eq 1 ] && [ "$nExact" -eq 12 ] && [ "$nBite" -eq 6 ] && tail -1 "$OUT/rtbufview.txt" | grep -q "RTBUF GATE OK"; then
	echo "RTBUF GATE OK"
else
	echo "RTBUF GATE FAILED"
	exit 1
fi
