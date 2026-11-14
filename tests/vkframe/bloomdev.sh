#!/bin/bash
# Lane VKOBJ4 item 2: the bloom chain on the present device against the same chain on a device of its own (bloomdev.cpp).
# bash tests/vkframe/bloomdev.sh <out dir>. Needs MSYS2 UCRT64 g++ with Qt6 (Core, Gui) and the Vulkan headers.
# Green: separate.bin == present.bin byte for byte (EXACT) and every BLOOMDEV check OK; the reds are checks inside the
# runs (red-box-moves: job.red = 1 moves the output; red-noborrow-refuses; red-other-device-refuses).
# Last line: `BLOOMDEV GATE OK` or `BLOOMDEV GATE FAILED`.
set -u
OUT="${1:?out dir}"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
export PATH="/c/msys64/ucrt64/bin:$PATH"
Q=/c/msys64/ucrt64/include/qt6
mkdir -p "$OUT"
QI="-I$Q -I$Q/QtCore -I$Q/QtGui"
g++ -O1 -std=c++17 -pthread -DWW_VKPRESENT_NO_QT -I"$ROOT/src" -c "$ROOT/src/features/vkframe/vkpresent.cpp" -o "$OUT/vkpresent.o" \
	> "$OUT/build.log" 2>&1 || { tail -20 "$OUT/build.log"; echo "BLOOMDEV GATE FAILED (g++ vkpresent)"; exit 1; }
g++ -O1 -std=c++20 -DNIFSKOPE_VERSION=\"2.0.dev11\" -I"$ROOT/src" $QI -c "$ROOT/src/features/vkframe/vkframe.cpp" -o "$OUT/vkframe.o" \
	>> "$OUT/build.log" 2>&1 || { tail -20 "$OUT/build.log"; echo "BLOOMDEV GATE FAILED (g++ vkframe)"; exit 1; }
g++ -O1 -std=c++20 -I"$ROOT/src" $QI -c "$ROOT/tests/vkframe/bloomdev.cpp" -o "$OUT/bloomdev.o" \
	>> "$OUT/build.log" 2>&1 || { tail -20 "$OUT/build.log"; echo "BLOOMDEV GATE FAILED (g++ bloomdev)"; exit 1; }
g++ -pthread "$OUT/bloomdev.o" "$OUT/vkframe.o" "$OUT/vkpresent.o" -o "$OUT/bloomdev.exe" -lQt6Gui -lQt6Core -lopengl32 -lgdi32 -luser32 \
	>> "$OUT/build.log" 2>&1 || { tail -20 "$OUT/build.log"; echo "BLOOMDEV GATE FAILED (link)"; exit 1; }
rc=0
for m in separate present; do
	"$OUT/bloomdev.exe" "$OUT" $m > "$OUT/run_$m.txt" 2> "$OUT/run_$m.err"; r=$?
	grep -E "^(GL on|BLOOMDEV)" "$OUT/run_$m.txt"
	[ $r -eq 0 ] || { echo "RUN $m FAILED (rc $r)"; tail -5 "$OUT/run_$m.err"; rc=1; }
done
if [ -f "$OUT/separate.bin" ] && [ -f "$OUT/present.bin" ] && cmp -s "$OUT/separate.bin" "$OUT/present.bin"; then
	echo "BLOOMDEV green separate vs present EXACT ($(stat -c %s "$OUT/present.bin") bytes)"
else
	echo "BLOOMDEV green separate vs present FAIL ($(cmp "$OUT/separate.bin" "$OUT/present.bin" 2>&1 | head -1))"
	rc=1
fi
if [ $rc -eq 0 ]; then echo "BLOOMDEV GATE OK"; else echo "BLOOMDEV GATE FAILED"; exit 1; fi
