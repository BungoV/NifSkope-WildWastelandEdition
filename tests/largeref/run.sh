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
# lane LARGEREF4 (MAP-LIVE): -DLARGEREF_STALEMAP_RED (a live far rebuild keeps the old map and kind); (FIXEDOPEN-MIXED):
# -DLARGEREF_FIXEDOPEN_RED (the fixed open keeps the mode's answer after the far build)
g++ -std=c++17 -O1 -DLARGEREF_STALEMAP_RED -I"$T/src" "$T/tests/largeref/largeref_test.cpp" -o "$O/largeref_red5.exe" || exit 2
g++ -std=c++17 -O1 -DLARGEREF_FIXEDOPEN_RED -I"$T/src" "$T/tests/largeref/largeref_test.cpp" -o "$O/largeref_red6.exe" || exit 2
timeout 30 "$O/largeref_red5.exe" > "$O/red5.txt"; R5=$?
grep -E "^FAIL" "$O/red5.txt"
timeout 30 "$O/largeref_red6.exe" > "$O/red6.txt"; R6=$?
grep -E "^FAIL" "$O/red6.txt"
# lane LARGEREF5 (FIXEDOPEN-FLATMODE): -DLARGEREF_FIXEDFLAT_RED (a flat-mode fixed open places no large ref full)
g++ -std=c++17 -O1 -DLARGEREF_FIXEDFLAT_RED -I"$T/src" "$T/tests/largeref/largeref_test.cpp" -o "$O/largeref_red7.exe" || exit 2
timeout 30 "$O/largeref_red7.exe" > "$O/red7.txt"; R7=$?
grep -E "^FAIL" "$O/red7.txt"
# lane LARGEREF6 (FLATMASK-FIX): -DLARGEREF_FLATMASK_RED (the flat mask's cut stays whatever the env picks: no promote, no box)
g++ -std=c++17 -O1 -DLARGEREF_FLATMASK_RED -I"$T/src" "$T/tests/largeref/largeref_test.cpp" -o "$O/largeref_red8.exe" || exit 2
timeout 30 "$O/largeref_red8.exe" > "$O/red8.txt"; R8=$?
grep -E "^FAIL" "$O/red8.txt"
timeout 30 "$O/largeref.exe"; G=$?
timeout 30 "$O/largeref_red.exe" > "$O/red.txt"; R=$?
timeout 30 "$O/largeref_red2.exe" > "$O/red2.txt"; R2=$?
timeout 30 "$O/largeref_red3.exe" > "$O/red3.txt"; R3=$?
tail -1 "$O/red.txt"
grep -E "^FAIL|stream walk" "$O/red2.txt"
grep -E "^FAIL" "$O/red3.txt"
echo "green rc=$G red rc=$R streamred rc=$R2 flatred rc=$R3 permodered rc=$R4 stalemapred rc=$R5 fixedred rc=$R6" \
	"fixedflatred rc=$R7 flatmaskred rc=$R8"
[ $G -eq 0 ] && [ $R -ne 0 ] && [ $R2 -ne 0 ] && [ $R3 -ne 0 ] && [ $R4 -ne 0 ] && [ $R5 -ne 0 ] && [ $R6 -ne 0 ] \
	&& [ $R7 -ne 0 ] && [ $R8 -ne 0 ] \
	&& echo "LARGEREF UNIT PASS" \
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
	# lane LARGEREF4 (MAP-LIVE): a live far rebuild mid-walk (budget -> mixed; far LOD removed), green + stalemap red
	timeout 300 "$O/largeref.exe" --rnam "$RNAM" --far live; LG=$?
	timeout 300 "$O/largeref_red5.exe" --rnam "$RNAM" --far live > "$O/census_red5l.txt"; LR=$?
	tail -1 "$O/census_red5l.txt"
	timeout 300 "$O/largeref.exe" --rnam "$RNAM" --far liveoff; OG=$?
	timeout 300 "$O/largeref_red5.exe" --rnam "$RNAM" --far liveoff > "$O/census_red5o.txt"; OR=$?
	tail -1 "$O/census_red5o.txt"
	# lane LARGEREF4 (FIXEDOPEN-MIXED): the fixed open over the mixed and budget fields, green + fixedpermode red
	timeout 300 "$O/largeref.exe" --rnam "$RNAM" --far fixedmixed; XG=$?
	timeout 300 "$O/largeref_red6.exe" --rnam "$RNAM" --far fixedmixed > "$O/census_red6m.txt"; XR=$?
	tail -1 "$O/census_red6m.txt"
	timeout 300 "$O/largeref.exe" --rnam "$RNAM" --far fixedbudget; YG=$?
	timeout 300 "$O/largeref_red6.exe" --rnam "$RNAM" --far fixedbudget > "$O/census_red6b.txt"; YR=$?
	tail -1 "$O/census_red6b.txt"
	# lane LARGEREF5 (FIXEDOPEN-FLATMODE): the fixed open in flat mode (a .bto-less chunk and a refused ring None), green + red7
	timeout 300 "$O/largeref.exe" --rnam "$RNAM" --far fixedflat; ZG=$?
	timeout 300 "$O/largeref_red7.exe" --rnam "$RNAM" --far fixedflat > "$O/census_red7f.txt"; ZR=$?
	tail -1 "$O/census_red7f.txt"
	# lane LARGEREF6 (FLATMASK-FIX): the flat-mask walk needs every ref's footprint: EXTENT=<text table>
	# (notes/largeref6/tools/extent_txt.py from largeref4's lr_extent json); without it the walk is skipped
	WG=0; WR=1
	if [ -n "$EXTENT" ]; then
		timeout 300 "$O/largeref.exe" --rnam "$RNAM" --far flatmask "$EXTENT"; WG=$?
		timeout 300 "$O/largeref_red8.exe" --rnam "$RNAM" --far flatmask "$EXTENT" > "$O/census_red8.txt"; WR=$?
		tail -1 "$O/census_red8.txt"
	fi
	echo "census green rc=$CG streamred rc=$CR flat green rc=$FG flatred rc=$FR mixed green rc=$MG red rc=$MR" \
		"budget green rc=$BG red rc=$BR live green rc=$LG red rc=$LR liveoff green rc=$OG red rc=$OR" \
		"fixedmixed green rc=$XG red rc=$XR fixedbudget green rc=$YG red rc=$YR fixedflat green rc=$ZG red rc=$ZR" \
		"flatmask green rc=$WG red rc=$WR (${EXTENT:-no EXTENT: skipped})"
	[ $CG -eq 0 ] && [ $CR -ne 0 ] && [ $FG -eq 0 ] && [ $FR -ne 0 ] && [ $MG -eq 0 ] && [ $MR -ne 0 ] \
		&& [ $BG -eq 0 ] && [ $BR -ne 0 ] && [ $LG -eq 0 ] && [ $LR -ne 0 ] && [ $OG -eq 0 ] && [ $OR -ne 0 ] \
		&& [ $XG -eq 0 ] && [ $XR -ne 0 ] && [ $YG -eq 0 ] && [ $YR -ne 0 ] && [ $ZG -eq 0 ] && [ $ZR -ne 0 ] \
		&& [ $WG -eq 0 ] && [ $WR -ne 0 ] \
		&& echo "LARGEREF CENSUS PASS" || { echo "LARGEREF CENSUS FAIL"; exit 1; }
fi
