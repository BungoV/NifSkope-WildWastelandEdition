#!/bin/bash
# Lane TOOLS1101 (REFLPROBECLI1): the reflection-probe census's gate. Builds tests/features/reflections/reflcensus/reflcensus_test.cpp
# (src/features/reflections/reflcensus.h through the bake's tracer, against ray-plane arithmetic) green and with both reds, then checks
# that every literal reflcensus.h mirrors is still in the pass and its shaders (a drift there fails here).
#   tests/features/reflections/reflcensus/run.sh <out dir>       (from the tree root; CXX defaults to g++, UCRT64 first on PATH)
# Green must print "REFLCENSUS PASS <n>"; red lift must "FAIL below:", red noreach "FAIL hits:", red roof "FAIL floors:", red stand "FAIL stand:", red place "FAIL fit:"; the drift check must
# find every literal and must refuse a copy of the pass with one literal changed (its floor). rc 0 = gate OK.
set -u
OUT="${1:?usage: run.sh <out dir>}"
CXX="${CXX:-g++}"
mkdir -p "$OUT"
SRC=tests/features/reflections/reflcensus/reflcensus_test.cpp
bad=0
"$CXX" -O2 -std=c++17 -Wall -I src "$SRC" -o "$OUT/reflcensus_test.exe" -static || { echo "BUILD FAIL"; exit 2; }
timeout 60 "$OUT/reflcensus_test.exe" > "$OUT/green.txt"; rc=$?
tail -n 1 "$OUT/green.txt"
if [ $rc -ne 0 ] || ! grep -q '^REFLCENSUS PASS [0-9]*$' "$OUT/green.txt"; then echo "GREEN FAILED (rc $rc)"; bad=1; fi
for pair in lift:below noreach:hits roof:floors stand:stand place:fit; do	# roof: the interior --auto cameras; stand: where a probe stands; place: REFLPLACE1's old grid
	red="${pair%%:*}"; check="${pair##*:}"
	timeout 60 "$OUT/reflcensus_test.exe" "$red" > "$OUT/red_$red.txt"; rc=$?
	if [ $rc -eq 1 ] && grep -q "^FAIL $check:" "$OUT/red_$red.txt"; then
		echo "red $red: fails $check as it must ($(grep -c '^FAIL' "$OUT/red_$red.txt") check(s) failed)"
	else
		echo "RED $red DID NOT FAIL $check (rc $rc)"; bad=1
	fi
done

# the mirrored literals: file|literal
LIT=(
	"src/features/reflections/cellreflprobe.cpp|constexpr int kFace = 64;"
	"src/features/reflections/cellreflprobe.cpp|constexpr float kBox = 2048.0f;"
	"src/features/reflections/cellreflprobe.cpp|const float camZ = std::round( cam[2] / 256.0f ) * 256.0f;"
	"src/features/reflections/cellreflprobe.cpp|c.bmin[2] = camZ - kBox * 0.5f;"
	"src/features/reflections/cellreflprobe.cpp|at[a] = std::round( cam[a] / 128.0f ) * 128.0f;"
	"src/features/reflections/cellreflprobe.cpp|const float half[3] = { L->interior ? 1024.0f : 4096.0f, L->interior ? 1024.0f : 4096.0f, L->interior ? 512.0f : 2048.0f };"
	"src/features/reflections/cellreflprobe.cpp|reach = std::max( std::sqrt( reach ) * 2.0f, 4096.0f );"
	"src/features/reflections/cellreflprobe.cpp|const float n = 4.0f, f = reach;"
	"src/features/reflections/cellreflprobe.cpp|const bool byRooms = L->interior && !g.rooms.empty();"
	"src/features/reflections/cellreflprobe.cpp|const int f = reflplace::fit( *fit, bmin, bmax, pos );"
	"res/shaders/features/reflections/cell_reflrelight.frag|vec2 f = uv * 2.0 - 1.0;"
	"res/shaders/features/reflections/cell_reflrelight.frag|float t = max( -n.z, 0.0 );"
	"res/shaders/features/reflections/cell_reflrelight.frag|if ( a.a < 0.5 ) {"
	"res/shaders/features/sky/cell_skybelow.glsl|return d.z < 0.0;"
	"res/shaders/features/sky/cell_skybelow.glsl|float c = 0.93969262, s = -0.34202014;"
	"res/shaders/features/sky/cell_skybelow.glsl|float q = 0.35355339, s6 = -0.86602540;"
	"res/shaders/features/sky/cell_skybelow.glsl|return n > 0.5 ? sum * ( 1.0 / n ) : vec3( 0.5 );"
)
drift() {   # drift <root>: prints the missing literals, rc = how many
	local n=0 e f l
	for e in "${LIT[@]}"; do
		f="${e%%|*}"; l="${e#*|}"
		grep -qF -- "$l" "$1/$f" 2>/dev/null || { echo "  DRIFT $f: $l"; n=$((n+1)); }
	done
	return $n
}
drift . > "$OUT/drift.txt"; nd=$?
if [ $nd -eq 0 ]; then echo "drift: all ${#LIT[@]} mirrored literals present"; else cat "$OUT/drift.txt"; echo "DRIFT $nd"; bad=1; fi
# the floor: a copy with the reach's minimum changed must be refused
mkdir -p "$OUT/floor/src/features/reflections" "$OUT/floor/res/shaders/features/reflections" "$OUT/floor/res/shaders/features/sky"
cp res/shaders/features/reflections/cell_reflrelight.frag "$OUT/floor/res/shaders/features/reflections/"
cp res/shaders/features/sky/cell_skybelow.glsl "$OUT/floor/res/shaders/features/sky/"
sed 's/\* 2\.0f, 4096\.0f );/* 2.0f, 8192.0f );/' src/features/reflections/cellreflprobe.cpp > "$OUT/floor/src/features/reflections/cellreflprobe.cpp"
drift "$OUT/floor" > "$OUT/drift_floor.txt"; nf=$?
if [ $nf -eq 1 ]; then echo "drift floor: a copy with the reach minimum 8192 is refused as it must"; else echo "DRIFT FLOOR DID NOT FIRE ($nf)"; bad=1; fi
rm -rf "$OUT/floor"
[ $bad -eq 0 ] && echo "REFLCENSUS GATE OK (green + 5 reds +${#LIT[@]} literals + drift floor)" || echo "REFLCENSUS GATE FAILED"
exit $bad
