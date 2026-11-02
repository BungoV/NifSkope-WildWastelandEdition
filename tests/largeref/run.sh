#!/bin/bash
# lane GEOLOD1: build and run the large-ref unit test, green and reds (each red must fail). usage: run.sh [outdir]
# reds: -DLARGEREF_RED (RNAM read X,Y), -DLARGEREF_STREAM_RED (lane LARGEREF2: the stream's double draw)
T="$(cd "$(dirname "$0")/../.." && pwd)"
O="${1:-$T/tests/largeref/out}"
mkdir -p "$O"
export PATH="/c/msys64/ucrt64/bin:$PATH"
g++ -std=c++17 -O1 -I"$T/src" "$T/tests/largeref/largeref_test.cpp" -o "$O/largeref.exe" || exit 2
g++ -std=c++17 -O1 -DLARGEREF_RED -I"$T/src" "$T/tests/largeref/largeref_test.cpp" -o "$O/largeref_red.exe" || exit 2
g++ -std=c++17 -O1 -DLARGEREF_STREAM_RED -I"$T/src" "$T/tests/largeref/largeref_test.cpp" -o "$O/largeref_red2.exe" || exit 2
# lane LARGEREF3: -DLARGEREF_FLAT_RED (over a flat far field the ref is drawn full beside its far copy)
g++ -std=c++17 -O1 -DLARGEREF_FLAT_RED -I"$T/src" "$T/tests/largeref/largeref_test.cpp" -o "$O/largeref_red3.exe" || exit 2
# lane LARGEREF4: -DLARGEREF_PERMODE_RED (one far kind per mode, no per-cell map: the mixed FO4CS field doubles)
g++ -std=c++17 -O1 -DLARGEREF_PERMODE_RED -I"$T/src" "$T/tests/largeref/largeref_test.cpp" -o "$O/largeref_red4.exe" || exit 2
timeout 30 "$O/largeref_red4.exe" > "$O/red4.txt"; R4=$?
grep -E "^FAIL" "$O/red4.txt"
timeout 30 "$O/largeref.exe"; G=$?
timeout 30 "$O/largeref_red.exe" > "$O/red.txt"; R=$?
timeout 30 "$O/largeref_red2.exe" > "$O/red2.txt"; R2=$?
timeout 30 "$O/largeref_red3.exe" > "$O/red3.txt"; R3=$?
tail -1 "$O/red.txt"
grep -E "^FAIL|stream walk" "$O/red2.txt"
grep -E "^FAIL" "$O/red3.txt"
echo "green rc=$G red rc=$R streamred rc=$R2 flatred rc=$R3 permodered rc=$R4"
[ $G -eq 0 ] && [ $R -ne 0 ] && [ $R2 -ne 0 ] && [ $R3 -ne 0 ] && [ $R4 -ne 0 ] && echo "LARGEREF UNIT PASS" \
	|| { echo "LARGEREF UNIT FAIL"; exit 1; }
# lane LARGEREF2: RNAM=<dump> (a world's RNAM subrecords, uint32 length + bytes each) adds the census walk, green + stream red
if [ -n "$RNAM" ]; then
	timeout 300 "$O/largeref.exe" --rnam "$RNAM"; CG=$?
	timeout 300 "$O/largeref_red2.exe" --rnam "$RNAM" > "$O/census_red2.txt"; CR=$?
	tail -1 "$O/census_red2.txt"
	# lane LARGEREF3: the same walk over a flat far field (no layer, the far copy alone), green + flat red
	timeout 300 "$O/largeref.exe" --rnam "$RNAM" --far flat; FG=$?
	timeout 300 "$O/largeref_red3.exe" --rnam "$RNAM" --far flat > "$O/census_red3.txt"; FR=$?
	tail -1 "$O/census_red3.txt"
	# lane LARGEREF4: mapped fields, green + permode red: mixed (ours + the game's .bto), budget (a refused object ring)
	timeout 300 "$O/largeref.exe" --rnam "$RNAM" --far mixed; MG=$?
	timeout 300 "$O/largeref_red4.exe" --rnam "$RNAM" --far mixed > "$O/census_red4m.txt"; MR=$?
	tail -1 "$O/census_red4m.txt"
	timeout 300 "$O/largeref.exe" --rnam "$RNAM" --far budget; BG=$?
	timeout 300 "$O/largeref_red4.exe" --rnam "$RNAM" --far budget > "$O/census_red4b.txt"; BR=$?
	tail -1 "$O/census_red4b.txt"
	echo "census green rc=$CG streamred rc=$CR flat green rc=$FG flatred rc=$FR mixed green rc=$MG red rc=$MR" \
		"budget green rc=$BG red rc=$BR"
	[ $CG -eq 0 ] && [ $CR -ne 0 ] && [ $FG -eq 0 ] && [ $FR -ne 0 ] && [ $MG -eq 0 ] && [ $MR -ne 0 ] \
		&& [ $BG -eq 0 ] && [ $BR -ne 0 ] && echo "LARGEREF CENSUS PASS" || { echo "LARGEREF CENSUS FAIL"; exit 1; }
fi
