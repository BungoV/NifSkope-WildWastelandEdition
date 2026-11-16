#!/bin/bash
# Lane VKFLIGHT4 item 1: the view's QPainter overlay blended on Vulkan against GL's own QPainter (overlayview.cpp: the
# bars and reds are written in its head). Also a site check on the app: glview.cpp hands its paint device to
# WwVkPresent::painterDone after painter.end(), and vkpresent.cpp's painterTarget returns the overlay image.
# bash tests/vkframe/overlayview.sh <out dir> [site-only]. Needs MSYS2 UCRT64 g++ with Qt6 (Core, Gui, OpenGL) + Vulkan headers.
# Last line: `OVERLAYVIEW GATE OK` or `OVERLAYVIEW GATE FAILED`.
set -u
OUT="${1:?out dir}"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
mkdir -p "$OUT"
rc=0
python -I "$ROOT/tests/vkframe/overlaysite.py" "$ROOT" || rc=1
if [ "${2:-}" = "site-only" ]; then
	if [ $rc -eq 0 ]; then echo "OVERLAYVIEW GATE OK"; else echo "OVERLAYVIEW GATE FAILED"; exit 1; fi
	exit 0
fi
export PATH="/c/msys64/ucrt64/bin:$PATH"
Q=/c/msys64/ucrt64/include/qt6
QI="-I$Q -I$Q/QtCore -I$Q/QtGui -I$Q/QtOpenGL"
g++ -O1 -std=c++17 -pthread -DWW_VKPRESENT_NO_QT -I"$ROOT/src" -c "$ROOT/src/features/vkframe/vkpresent.cpp" -o "$OUT/vkpresent.o" \
	> "$OUT/build.log" 2>&1 || { tail -20 "$OUT/build.log"; echo "OVERLAYVIEW GATE FAILED (g++ vkpresent)"; exit 1; }
g++ -O1 -std=c++20 -I"$ROOT/src" $QI -c "$ROOT/tests/vkframe/overlayview.cpp" -o "$OUT/overlayview.o" \
	>> "$OUT/build.log" 2>&1 || { tail -20 "$OUT/build.log"; echo "OVERLAYVIEW GATE FAILED (g++ overlayview)"; exit 1; }
g++ -pthread "$OUT/overlayview.o" "$OUT/vkpresent.o" -o "$OUT/overlayview.exe" -lQt6OpenGL -lQt6Gui -lQt6Core -lopengl32 -lgdi32 -luser32 \
	>> "$OUT/build.log" 2>&1 || { tail -20 "$OUT/build.log"; echo "OVERLAYVIEW GATE FAILED (link)"; exit 1; }
"$OUT/overlayview.exe" > "$OUT/run.txt" 2> "$OUT/run.err"; r=$?
grep -E "^(GL on|OVERLAYVIEW)" "$OUT/run.txt" | grep -v 'GATE'
n=$(grep -cE "^OVERLAYVIEW .* (OK|FAIL)" "$OUT/run.txt")
echo "OVERLAYVIEW $n checks read"
[ "$n" -gt 0 ] || rc=1
[ $r -eq 0 ] || { echo "RUN FAILED (rc $r)"; tail -5 "$OUT/run.err"; rc=1; }
if [ $rc -eq 0 ]; then echo "OVERLAYVIEW GATE OK"; else echo "OVERLAYVIEW GATE FAILED"; exit 1; fi
