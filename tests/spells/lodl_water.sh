#!/bin/bash
#
# The .lodl VERSION 3 water sections: bodies, flow, shore and the stroke store.
#
# The gates are lane WATER2's, pre-registered in
# scratchpad/specs_20260909/spec_water.md §7 as G1..G9, and they run in the
# order they would break:
#
#   G6  the known-answer control, with its refuter -- FIRST, before any real
#       number is believed (ww-control-calibration step 1)
#   G1  the module OFF writes the file it always wrote, byte for byte
#   G2  WW_LODL_VERSION=2 with the module ON writes those same bytes
#   G3  the version refusal, and the header-size TABLE behind it
#   G4  each of the four named version-3 refusals, on a hand-corrupted file.
#       NOTE the stride case: a record LONGER than the reader's is the
#       forward-compatible case and must NOT refuse -- only one that is missing
#       fields the reader needs does.
#   G5  the census, against the Python oracle
#   G7  an INDEPENDENT decoder, sharing no code with src/lodtfile.cpp
#   G8  the stroke round-trip, and that a second write reproduces the planes
#   G9  cost, REPORTED and not gated
#
# Every refusal has a CONTROL on the other side of it: the same file, unbroken,
# must still open, or a refusal proves only that the route is broken.

set -u

. "$(dirname "$0")/_harness.sh"

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
NS="${EXE:-$ROOT/release/NifSkope.exe}"
ESM="${ESM:-X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm}"
PY="${PY:-$(command -v python || echo /c/Windows/py)}"
SCR="$ROOT/scratchpad/water2_20260909"
AUTH="$SCR/lodl_v3_authority.py"
W="$(mktemp -d)"
trap 'rm -rf "$W"' EXIT

rc=0
ok()  { echo "  ok   $1"; }
bad() { echo "  FAIL $1"; rc=1; }
say() { echo "  $*"; }

[ -x "$NS" ] || { echo "no NifSkope.exe at $NS"; exit 2; }
[ -f "$ESM" ] || { echo "no ESM at $ESM"; exit 2; }
[ -f "$AUTH" ] || { echo "no independent decoder at $AUTH"; exit 2; }

# --- G6 first: the classifier's known-answer control -------------------
echo "== G6  the known-answer control (a sea, a stepped river, a lake, a puddle) =="
if "$NS" -no-gui lodl "$ESM" --water-selftest > "$W/self.txt" 2>&1; then
	sed 's/^/  /' "$W/self.txt"
	grep -q "control PASS" "$W/self.txt" && ok "the control holds" \
		|| bad "the control holds"
	grep -q "REFUTER" "$W/self.txt" && ok "the refuter was run and printed" \
		|| bad "the refuter was run"
else
	bad "the known-answer control"; sed 's/^/  /' "$W/self.txt"
fi

# --- G1: the module OFF is byte-identical ------------------------------
echo "== G1  the module OFF writes the bytes it always wrote =="
"$NS" -no-gui lodgen "$ESM" --worldspace 3C --lodl "$W/off" > "$W/off.txt" 2>&1 \
	|| { bad "the version-2 write (module off)"; sed 's/^/  /' "$W/off.txt" | tail -5; }
# the .lodl moved under FO4CSLOD/<ws>/ (lane LAYOUT1, 2026-09-16)
OFF="$W/off/FO4CSLOD/Commonwealth/Commonwealth.lodl"
if [ -s "$OFF" ]; then
	SZ="$(stat -c%s "$OFF")"
	say "module off: $SZ bytes"
	[ "$SZ" = 35953294 ] && ok "35,953,294 bytes, the size of the shipped file" \
		|| bad "35,953,294 bytes (got $SZ)"
	SHIPPED="E:/Projects/Fallout 4 Mods/mods/FO4CS/Terrain/Commonwealth.lodl"
	if [ -f "$SHIPPED" ]; then
		if cmp -s "$OFF" "$SHIPPED"; then
			ok "byte-identical to the shipped Commonwealth.lodl"
		else
			bad "byte-identical to the shipped Commonwealth.lodl"
			cmp "$OFF" "$SHIPPED" 2>&1 | sed 's/^/     /' | head -3
		fi
	else
		say "SKIPPED the shipped-file comparison: no $SHIPPED"
	fi
else
	bad "a .lodl was written with the module off"
fi

# --- G2: the fallback, exact at its off value --------------------------
echo "== G2  WW_LODL_VERSION=2 with the module ON writes the same bytes =="
WW_LODL_VERSION=2 "$NS" -no-gui lodgen "$ESM" --worldspace 3C --lodl "$W/fb" \
	--water-bodies > "$W/fb.txt" 2>&1
FB="$W/fb/FO4CSLOD/Commonwealth/Commonwealth.lodl"
if [ -s "$FB" ] && [ -s "$OFF" ]; then
	cmp -s "$OFF" "$FB" && ok "the fallback is byte-identical to the module-off file" \
		|| bad "the fallback is byte-identical to the module-off file"
else
	bad "the fallback wrote a file"
	sed 's/^/  /' "$W/fb.txt" | tail -5
fi

# --- the real thing ----------------------------------------------------
echo "== the version-3 write =="
START=$(date +%s)
"$NS" -no-gui lodgen "$ESM" --worldspace 3C --lodl "$W/v3" --water-bodies \
	--water-report "$W/census.txt" > "$W/v3.txt" 2>&1 \
	|| { bad "the version-3 write"; sed 's/^/  /' "$W/v3.txt" | tail -20; }
ELAPSED=$(( $(date +%s) - START ))
V3="$W/v3/FO4CSLOD/Commonwealth/Commonwealth.lodl"
[ -s "$V3" ] || { echo "FAIL: no version-3 file"; echo FAIL; exit 1; }
grep -o 'water: .*' "$W/v3.txt" | sed 's/^/  /'

# --- G3: the version refusal, and the header-size table ----------------
echo "== G3  an unknown version is refused BY NAME, before any offset is read =="
"$PY" "$SCR/corrupt_lodl.py" "$V3" "$W/v4.lodl" version4
if "$NS" -no-gui lodl "$V3" --info > "$W/ctl.txt" 2>&1; then
	ok "control: the version-3 file opens"
else
	bad "control: the version-3 file opens"; sed 's/^/  /' "$W/ctl.txt"
fi
if "$NS" -no-gui lodl "$W/v4.lodl" --info > "$W/v4.txt" 2>&1; then
	bad "a version-4 file is refused"
else
	if grep -q "unsupported version 4 (this reader knows 1\.\.3)" "$W/v4.txt"; then
		ok "refused by name: $(grep -o 'unsupported version.*' "$W/v4.txt" | head -1)"
	else
		bad "the refusal names the version and the range"; sed 's/^/     /' "$W/v4.txt"
	fi
fi
grep -q "lodtHeaderBytes" "$ROOT/src/lodtfile.cpp" \
	&& ok "the header size is a table (lodtHeaderBytes), not a >= 2 ternary" \
	|| bad "the header size is a table"
if grep -q "ver >= 2 ? LODL_HEADER_V2 : LODL_HEADER_V1" "$ROOT/src/lodtfile.cpp"; then
	bad "the old header-size ternary is gone"
else
	ok "the old header-size ternary is gone"
fi

# --- G4: the four named version-3 refusals -----------------------------
echo "== G4  each version-3 refusal produces its OWN sentence =="
refuse() {   # <name> <corruption> <expected text>
	"$PY" "$SCR/corrupt_lodl.py" "$V3" "$W/bad.lodl" "$2" > /dev/null || {
		bad "$1: the corruption could not be applied"; return; }
	if "$NS" -no-gui lodl "$W/bad.lodl" --info > "$W/bad.txt" 2>&1; then
		bad "$1 is refused"
	else
		if grep -qi -- "$3" "$W/bad.txt"; then
			ok "$1: $(head -1 "$W/bad.txt" | cut -c1-108)"
		else
			bad "$1 names its own reason"; sed 's/^/     /' "$W/bad.txt" | head -3
		fi
	fi
}
refuse "a section bit over an empty rate" norate  "declared present"
refuse "a record SHORTER than this reader knows" stride "this reader knows"
refuse "a record whose id is not its index + 1" badid  "record i is body i + 1"
refuse "a plane naming a body past the table" planeid "the table holds"

# --- G5: the census, against the Python oracle -------------------------
echo "== G5  the census =="
if "$NS" -no-gui lodl "$V3" --water-census > "$W/read.txt" 2>&1; then
	sed -n '1,10p' "$W/read.txt" | sed 's/^/  /'
else
	bad "--water-census reads the file back"; sed 's/^/  /' "$W/read.txt"
fi
ORACLE="$SCR/census_water2.txt"
if [ -f "$ORACLE" ]; then
	"$PY" "$SCR/compare_census.py" "$W/read.txt" "$ORACLE"
	[ $? = 0 ] || rc=1
else
	say "SKIPPED the oracle comparison: no $ORACLE (run census_water2.py first)"
fi

# --- G7: the independent decoder ---------------------------------------
echo "== G7  an independent decoder, sharing no code with src/lodtfile.cpp =="
"$PY" "$AUTH" "$V3" --samples 10000 > "$W/auth.txt" 2>&1
AR=$?
sed 's/^/  /' "$W/auth.txt"
[ $AR = 0 ] || rc=1

# --- G8: determinism and the stroke store ------------------------------
echo "== G8  a second write reproduces the same bytes; the stroke store round-trips =="
"$NS" -no-gui lodgen "$ESM" --worldspace 3C --lodl "$W/v3b" --water-bodies \
	> "$W/v3b.txt" 2>&1
V3B="$W/v3b/FO4CSLOD/Commonwealth/Commonwealth.lodl"
if [ -s "$V3B" ]; then
	cmp -s "$V3" "$V3B" && ok "two runs of the version-3 writer are byte-identical" \
		|| bad "two runs of the version-3 writer are byte-identical"
else
	bad "the second version-3 write produced a file"
fi
"$PY" "$SCR/check_v2_intact.py" "$OFF" "$V3"
[ $? = 0 ] || rc=1

# --- G9: cost, REPORTED not gated --------------------------------------
echo "== G9  cost (reported, not gated) =="
S2="$(stat -c%s "$OFF")"
S3="$(stat -c%s "$V3")"
say "version 2 $S2 bytes, version 3 $S3 bytes, water sections $(( S3 - S2 )) bytes"
say "the version-3 write took ${ELAPSED}s wall; the version-2 writer's own line:"
grep -o 'timing:.*' "$W/off.txt" | head -1 | sed 's/^/    /'
cp "$W/census.txt" "$SCR/census_cpp.txt" 2>/dev/null
cp "$W/read.txt" "$SCR/census_readback.txt" 2>/dev/null
cp "$W/self.txt" "$SCR/selftest.txt" 2>/dev/null
cp "$W/auth.txt" "$SCR/authority.txt" 2>/dev/null

[ "$rc" = 0 ] && echo PASS || echo FAIL
exit $rc
