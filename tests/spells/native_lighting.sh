#!/usr/bin/env bash
#
# NATIVE LOD LIGHTING -- the model-space normal path in the FO4 GLSL program.
#
# Lane NATIVEVIEW2, 2026-09-16.  Why this gate exists, in bungo's terms: the
# native LOD terrain came out covered in dark blotches, and the blotches were
# NOT in the colour tiles.  The measurement that found the cause (HANDOFF
# 2026-09-12 "MEASURED 20:57") replaced every normal tile with one that says
# "straight up" everywhere and got THE SAME BLOTCHES -- dark fraction 20.0 per
# cent against 20.1, the two dark masks overlapping to IoU 0.86.  A normal map
# that cannot change the picture is a normal map nobody is reading.
#
# The cause: a terrain sheet is an `_msn`, a MODEL-space normal map, and the
# shape says so (Shader Flags 1 bit 12).  fo4_default.frag had no model-space
# branch, so it read the sheet as a TANGENT-space map and sent its "up" along
# whatever bitangent src/btdterrain.cpp happened to build (T = n x worldUp,
# B = n x T -- an arbitrary frame).  The fix is one branch: when bit 12 is set,
# transform the texel by the model matrix alone.
#
# WHAT EACH NUMBER MEANS
#
#   gate (a)  a shape the branch must not touch renders BYTE-identical.  The
#             .BTO objects (bit 12 clear) and the legacy .BTR are the victims
#             here; the .BTR is measured, not assumed, to be lit by sk_msn.prog.
#   gate (b)  the terrain's OWN normal tiles and FLAT tiles must now make
#             DIFFERENT pictures, and the flat ones must be lit evenly.  The
#             rung is the refuter: there they were the same to IoU 0.86.
#   gate (c)  the dark fraction (luma < 40) over the terrain.
#   gate (d)  a slope test with a known answer.  Three synthetic sheets --
#             "up", "tilted 28.94 deg EAST", "tilted 28.94 deg WEST" -- and the
#             headlight's own direction give the answer before the render:
#             at the TOP view east and west must be the SAME picture, and at
#             the oblique luma must order west > flat > east.
#   gate (f)  the TWIN.  A fifth cache carries the container's own normal tiles
#             with their BC1 blocks shuffled into the wrong places -- same
#             words, same histogram, same codec -- and the real sheet must make
#             at least 1.60x as much block-scale shading as it does.  This is
#             gate (b)'s old "a real slope signal survives block averaging"
#             claim re-cut so that its floor is measured in the same run, on the
#             same container, instead of being a number pinned to one generation
#             of an untracked artefact.
#   gate (g)  the top view against the container's own normals.  Looking straight
#             down, N.L IS the normal's up component, and two arms in the run
#             have an exactly known up (0.9988 and 0.8751), so the frame's mean
#             luma is predicted from the sheet before it is measured.  This is
#             the row that catches a sheet whose axes are wrong.
#
# WHAT WAS RETIRED.  The darkest-fifth IoU row (own vs flat, bar 0.800) came out
# on 2026-09-19, lane GATEFIX2, and not for being red: measured against the
# defects it exists to catch, the healthy state sits INSIDE the broken
# population (sheet ignored 1.000, axes swapped 0.887, HEALTHY 0.850, wrong
# places 0.821/0.822/0.821), so no threshold of its shape can admit the healthy
# state and reject the defects.  native_lighting_check.py carries the table.
#
# Every floor's comment carries the value measured on the exe that introduced
# it AND the value the rung gave, so the floor is visibly able to fire.
# Measured 2026-09-16 on release/NifSkope.exe 11:54:47, 22,288,896 B: 14 checks,
# 0 failures.  Five rows added by GATEFIX1 on 2026-09-19 (gate (e), and gate (d)'s
# own floor): measured 19 checks on release/NifSkope.exe 16:48:03, 23,505,408 B.
# GATEFIX2 the same day retired one row and added three (gates (f) and (g), and
# gate (b)'s collapse floor): measured 21 checks, 0 failures, on
# release/NifSkope.exe 19:49:03, 23,625,728 B.
# COUNT FLOOR 21 (measured, not predicted).
#
# FIXTURES.  This gate renders a real baked .lodl and the real sheet cache left
# by the showcase and NATIVEVIEW1 lanes.  They are large and untracked; when they
# are absent the gate SKIPS with the missing path named and does not pass.
#
# The three SYNTHETIC caches -- flat, tilt, tiltw -- are BUILT BY THIS GATE, every
# run, by tests/spells/native_lighting_fixtures.py, and the gate then proves they
# are pairwise distinct before it believes any picture.  Lane GATEFIX1,
# 2026-09-19.  They used to be artefacts on disk from 2026-09-12, and the exe ate
# them: src/lodtsheets.cpp:501-506 reuses a cached tile only when it is NEWER than
# the .lodt container, the container was re-baked 2026-09-16 16:44, so the first
# run after that rule (2026-09-18 09:12:47..09:13:19, one arm every ten seconds)
# refilled all four caches from the SAME container.  Commonwealth.VT.2.0.4.n.DDS
# then read sha1 ad7f8084e3fb3653 in all four, and t_own/flat/tilt/tiltw_obl.png
# all came out b39e407cc0a7 -- one photograph four times.  Gates (b) and (d) were
# comparing a picture with itself and could not fail.
#
# So the `own` arm renders FIRST (that is what refreshes the own cache from the
# container), the three synthetics are rebuilt from it, and their mtimes are newer
# than the container by construction.

. "$(dirname "$0")/_harness.sh" 2>/dev/null || true
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
EXE="${EXE:-$ROOT/release/NifSkope.exe}"
PORT="${PORT:-42937}"

S="$ROOT/scratchpad/showcase1_20260912/out"
R1="$ROOT/scratchpad/nativeview1_20260912"
R2="$ROOT/scratchpad/nativeview2_20260912"
BASE="$ROOT/tests/baselines/native_lighting"
OUT="${OUT:-$R2/work/gate}"
LOG="$ROOT/release/ww_native_lighting.log"

LODL="$S/lodl/Terrain/Commonwealth.lodl"
SHEETS="$S/look/mod/Terrain"
OBJ="$S/look/obj"
OWN="$R1/work/sheetcache"
FLAT="$R2/work/flatcache"
TILT="$R2/work/tiltcache"
TILTW="$R2/work/tiltwcache"
SCRAM="$R2/work/scramcache"

# WW_LODGEN_RESOURCES is a SEMICOLON-JOINED LIST, and MSYS2 rescues a single
# argv/env path but never a list -- `_harness.sh` says so in full.  $ROOT comes
# from `cd .. && pwd`, so it is /e/Projects/...; joining two of those with a
# semicolon hands the exe two paths it cannot open, the object textures never
# load, and the legacy frames come out flat and DIFFERENT from their baselines.
# Measured 2026-09-16: the unconverted form rendered
# tests/baselines/native_lighting/legacy_bto_top.png at 390,854 B against the
# baseline's 537,794 B, mean |dColour| 30.93.  Convert each half first.
if ! type winpath >/dev/null 2>&1; then
	winpath() {
		case "$1" in
			/[a-zA-Z]/*) local d="${1:1:1}"; printf '%s' "${d}:${1:2}" ;;
			*) printf '%s' "$1" ;;
		esac
	}
fi
RES="$(winpath "$R1/resroot");$(winpath "$OBJ")"

rm -f "$LOG"
miss=""
# $FLAT/$TILT/$TILTW are NOT required to exist: this gate writes them.
for p in "$LODL" "$SHEETS" "$OBJ" "$OWN" \
         "$OBJ/Commonwealth.4.-20.24.BTR" "$OBJ/Commonwealth.4.-20.24.BTO"; do
	[ -e "$p" ] || miss="$miss $p"
done
if [ -n "$miss" ]; then
	echo "SKIP: the native LOD fixtures are not in this tree:"
	for p in $miss; do echo "  missing $p"; done
	echo "SKIP is never a pass; rebuild them with the showcase1 / nativeview1 bakes."
	exit 77
fi

mkdir -p "$OUT"
rm -f "$OUT"/*.png "$OUT"/*.census.txt

CEN="-73728,106496,0"
CENO="-73728,106496,8500"
LCEN="8192,8192,0"
LCENO="8192,8192,8500"
ORT=8192

shot() {  # shot <name> <file> <view> <center> [extra env...]
	local name="$1" file="$2" view="$3" ctr="$4"; shift 4
	env "$@" \
		WW_WINDOW_AT=1960,40 \
		WW_RENDER_SHOT="$OUT/$name.png" WW_RENDER_SIZE=1024x1024 \
		WW_RENDER_VIEW="$view" WW_RENDER_CLEAN=1 \
		WW_RENDER_CENTER="$ctr" WW_RENDER_ORTHO="$ORT" \
		WW_PROGRAM_CENSUS="$OUT/$name.census.txt" \
		WW_LODGEN_RESOURCES="$RES" \
		timeout 300 "$EXE" --port "$PORT" "$file" >/dev/null 2>&1
	local rc=$?
	[ -s "$OUT/$name.png" ] || echo "  render $name produced nothing (rc=$rc)"
}

terrain_arm() {  # terrain_arm <arm> <cache>
	local arm="$1" cache="$2" v n c
	for v in 1 8; do
		if [ "$v" = 1 ]; then n="t_${arm}_top"; c="$CEN"; else n="t_${arm}_obl"; c="$CENO"; fi
		shot "$n" "$LODL" "$v" "$c" \
			WW_LODL_REGION="-20,24,-17,27,2" \
			WW_LODL_SHEETS="$SHEETS" \
			WW_LODL_SHEET_CACHE="$cache"
	done
}

# The OWN arm first, because it is what refreshes $OWN from the container; only
# then is $OWN a usable source for the three synthetic caches.
terrain_arm own "$OWN"

LODT="$(ls "$SHEETS"/Commonwealth.VT.*.lodt 2>/dev/null | head -1)"
# The own arm's normals come out of this container, and gate (b)'s own-vs-flat
# floors were measured against whatever generation of it was on disk on
# 2026-09-16 11:54:47.  It has been re-baked since (16:44:04 the same day), so
# print its identity: a floor and a fixture from different generations is the
# first thing to check when own-vs-flat reads low.  GATEFIX1, 2026-09-19.
[ -n "$LODT" ] && echo "  own-arm container: $(basename "$LODT") $(stat -c '%s B, %y' "$LODT" | cut -c1-40) sha1 $(sha1sum "$LODT" | cut -c1-16)"
MAN="$OUT/fixtures.txt"
rm -f "$MAN"
if [ -z "$LODT" ]; then
	echo "  no Commonwealth.VT.*.lodt under $SHEETS -- the fixture builder cannot check freshness"
else
	python "$ROOT/tests/spells/native_lighting_fixtures.py" \
		"$OWN" "$FLAT" "$TILT" "$TILTW" "$SCRAM" "$LODT" "$MAN"
	echo "  fixture builder rc=$?"
fi

terrain_arm flat  "$FLAT"
terrain_arm tilt  "$TILT"
terrain_arm tiltw "$TILTW"

# The twin is the floor under gate (f) and is only ever read at the oblique, so
# it costs one frame, not two.
shot "t_scram_obl" "$LODL" 8 "$CENO" \
	WW_LODL_REGION="-20,24,-17,27,2" \
	WW_LODL_SHEETS="$SHEETS" \
	WW_LODL_SHEET_CACHE="$SCRAM"

for v in 1 8; do
	if [ "$v" = 1 ]; then s=top; lc="$LCEN"; c="$CEN"; else s=obl; lc="$LCENO"; c="$CENO"; fi
	shot "legacy_btr_$s" "$OBJ/Commonwealth.4.-20.24.BTR" "$v" "$lc"
	shot "legacy_bto_$s" "$OBJ/Commonwealth.4.-20.24.BTO" "$v" "$c"
done

python "$ROOT/tests/spells/native_lighting_check.py" "$OUT" "$BASE" "$MAN" "$OWN" > "$LOG" 2>&1
rc=$?
cat "$LOG"

COUNT=$(grep -oE '^[0-9]+ checks' "$LOG" | grep -oE '^[0-9]+')
# Measured on the introducing build, never predicted (ww-test-harness-add 5c).
[ -n "$COUNT" ] && [ "$COUNT" -ge 21 ] || {
	echo "FAIL: the gate ran $COUNT checks, floor 21 -- a block of it did not run"
	exit 1
}
grep -q '^PASS$' "$LOG" || exit 1
exit $rc
