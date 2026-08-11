#!/bin/bash
#
# Regression guard for the 2026-08-03 bug sweep.
#
# Each case below was CONFIRMED broken before the fix by reverting the one file
# and re-running this script — the numbers in the comments are what the old code
# actually produced, not a prediction. Anything here that starts passing for a
# reason other than its fix is a false negative, so the checks are written to be
# non-vacuous: they demand that the operation did something as well as that it
# did the right thing.
#
# All CLI, no GUI, no window — safe to run while someone is working.
#
# USAGE
#   bash tests/spells/bug_sweep.sh

set -u
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
EXE="${EXE:-$ROOT/release/NifSkope.exe}"
DATA="${DATA:-E:/Tools/Fallout 4/DataUnpacked/Data}"
BODY="${BODY:-$DATA/meshes/actors/character/characterassets/MaleBody.nif}"
SIGN="${SIGN:-$DATA/meshes/SetDressing/35CourtSign/35CourtSign01.nif}"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

[ -x "$EXE" ] || { echo "no NifSkope.exe at $EXE"; exit 2; }
[ -f "$BODY" ] || { echo "no fixture at $BODY"; exit 2; }
[ -f "$SIGN" ] || { echo "no fixture at $SIGN"; exit 2; }

# Pure bash, deliberately no sed: when this script is launched from a Git-Bash
# parent, the MSYS2 shell inherits Git's sed, and BRE groups passed across the
# two runtimes silently stop matching — winpath then no-ops. Single argv/env
# paths still get rescued by MSYS2's automatic conversion, but a
# semicolon-joined LIST (WW_SAMPOSE_WEAPON) is not, so the weapon step quietly
# skips. Parameter expansion cannot be PATH-poisoned.
winpath() {
	case "$1" in
		/[a-zA-Z]/*) local d="${1:1:1}"; printf '%s' "${d}:${1:2}" ;;
		*) printf '%s' "$1" ;;
	esac
}
W="$(winpath "$TMP")"

pass=0; fail=0
check() {  # desc, actual, expected
	if [ "$2" = "$3" ]; then
		echo "  ok   $1"; pass=$((pass+1))
	else
		echo "  FAIL $1 (got '$2', want '$3')"; fail=$((fail+1))
	fi
}
ns() { "$EXE" -no-gui "$@" 2>&1; }
get() { ns get "$1" -b "$2" -f "$3" | tail -1; }

# ---------------------------------------------------------------------------
# 1. Optimize Indices must not scramble the FO4 segment table.
#
#    meshopt permutes the whole triangle array; Segment/Sub Segment address
#    triangles by POSITION in that array. Before the fix the ranges were left
#    untouched, so each dismemberment slot ended up describing a different set
#    of faces — on this fixture one slot kept 0 of its 438 triangles, another
#    20 of 435. The counts all still agreed, so nothing flagged the file.
#
#    Non-vacuous by construction: the array must be reordered (the optimization
#    ran) AND every segment must still hold exactly its own faces.
# ---------------------------------------------------------------------------
echo "--- Optimize Indices vs FO4 segments ---"
SHAPE="$(ns list "$(winpath "$BODY")" -t BSSubIndexTriShape \
	| sed -n 's/^\[\([0-9]*\)\].*/\1/p' | head -1)"
[ -n "$SHAPE" ] || { echo "no BSSubIndexTriShape in $BODY"; exit 2; }

ns cast "$(winpath "$BODY")" -s "Mesh/Optimize Indices" -b "$SHAPE" -o "$W/opt.nif" > /dev/null
ns dump "$(winpath "$BODY")" -b "$SHAPE" -f Triangles -n 100000 > "$TMP/tri_before.txt"
ns dump "$W/opt.nif"         -b "$SHAPE" -f Triangles -n 100000 > "$TMP/tri_after.txt"
ns dump "$(winpath "$BODY")" -b "$SHAPE" -f Segment -d 2 -n 100000 > "$TMP/seg.txt"

SEGRESULT="$(TMP="$TMP" python - <<'PY'
import os, re
T = os.environ['TMP']
def tris(p):
    out = []
    for ln in open(p, encoding='utf-8', errors='replace'):
        m = re.search(r'<Triangle>\s*=\s*(\d+) (\d+) (\d+)', ln)
        if m:
            out.append(tuple(int(x) for x in m.groups()))
    return out
# (Start Index / 3, Num Primitives) pairs, in file order
nums = []
for ln in open(T + '/seg.txt', encoding='utf-8', errors='replace'):
    m = re.search(r'(Start Index|Num Primitives)\s+<\w+>\s*=\s*(\d+)', ln)
    if m:
        nums.append((m.group(1), int(m.group(2))))
segs, cur = [], None
for k, v in nums:
    if k == 'Start Index':
        cur = v // 3
    elif cur is not None:
        if v:
            segs.append((cur, v))
        cur = None
a, b = tris(T + '/tri_before.txt'), tris(T + '/tri_after.txt')
same = all(sorted(map(lambda t: tuple(sorted(t)), a[s:s+n]))
           == sorted(map(lambda t: tuple(sorted(t)), b[s:s+n])) for s, n in segs)
print('%d %s %s %s' % (len(segs), 'yes' if same else 'no',
                       'yes' if a != b else 'no', 'yes' if len(a) == len(b) else 'no'))
PY
)"
set -- $SEGRESULT
check "the fixture has segments to scramble" "$([ "${1:-0}" -ge 2 ] && echo yes)" "yes"
check "every segment still holds exactly its own faces" "${2:-}" "yes"
check "...and the triangles really were reordered" "${3:-}" "yes"
check "...with no triangle lost" "${4:-}" "yes"

# ---------------------------------------------------------------------------
# 2. "Add Tangent Spaces and Update" must not be a no-op on BSTriShape files.
#
#    The batch spell tested applicability of a NiTriShape-FILTERED index, which
#    is invalid for any BSTriShape — so it skipped every block on every Skyrim
#    SE / Fallout 4 mesh. Output was byte-identical to the input.
# ---------------------------------------------------------------------------
echo "--- Add Tangent Spaces and Update on FO4 ---"
cp "$BODY" "$TMP/tan_in.nif"
ns cast "$W/tan_in.nif" -s "Batch/Add Tangent Spaces and Update" -o "$W/tan_out.nif" > /dev/null
A="$(md5sum < "$TMP/tan_in.nif" | cut -c1-32)"
B="$(md5sum < "$TMP/tan_out.nif" | cut -c1-32)"
check "the spell changes a BSTriShape file" "$([ "$A" != "$B" ] && echo changed)" "changed"

# ---------------------------------------------------------------------------
# 3. A collision-layer edit must write BOTH copies of the filter.
#
#    bhkRigidBody stores the HavokFilter twice: its own, flattened onto the
#    block, and a copy inside Rigid Body Info. Only the second was ever written,
#    and the viewport colours collision by the FIRST — so an edit appeared to do
#    nothing and the saved file held two disagreeing layers.
#
#    Also covers the Motion System read: it lives on the block / Rigid Body
#    Info, never on the filter row. Reading it off filter.parent() always
#    yielded 0, so "infer Static or Props" could only ever infer Static.
# ---------------------------------------------------------------------------
echo "--- collision layer writes both filter copies ---"
ns cast "$(winpath "$SIGN")" -s "Havok/Decompile All Compiled Collision" -o "$W/dec.nif" > /dev/null
BODYBLK="$(ns list "$W/dec.nif" -t bhkRigidBody | sed -n 's/^\[\([0-9]*\)\].*/\1/p' | head -1)"
if [ -z "$BODYBLK" ]; then
	echo "  skip  no editable bhkRigidBody after decompile"
else
	# zero the Rigid Body Info copy (what makes the spell applicable) and put a
	# distinct wrong value in the block-level one, so a single-copy write shows
	ns set "$W/dec.nif" -b "$BODYBLK" -f "Rigid Body Info/Layer" -v 0 -o "$W/z1.nif" > /dev/null
	ns set "$W/z1.nif"  -b "$BODYBLK" -f "Layer"                 -v 7 -o "$W/z2.nif" > /dev/null
	check "the two copies start out disagreeing" \
		"$(get "$W/z2.nif" "$BODYBLK" "Layer")/$(get "$W/z2.nif" "$BODYBLK" "Rigid Body Info/Layer")" "7/0"
	ns cast "$W/z2.nif" -s "Havok/Set Collision Layer from Motion" -b "$BODYBLK" -o "$W/z3.nif" > /dev/null
	check "the block's own filter is written" "$(get "$W/z3.nif" "$BODYBLK" "Layer")" "1"
	check "...and so is the Rigid Body Info copy" \
		"$(get "$W/z3.nif" "$BODYBLK" "Rigid Body Info/Layer")" "1"
fi

# ---------------------------------------------------------------------------
# 4. Deleting geometry must leave the segment table describing real triangles.
#
#    Not reachable from the CLI (the delete lives in the edit-mode viewport), so
#    this checks the weaker invariant the whole family shares: after any spell
#    that rewrites triangles, Sum(Num Primitives) == Num Triangles and no range
#    runs past the end.
# ---------------------------------------------------------------------------
echo "--- segment table tiles the triangle array ---"
ns dump "$W/opt.nif" -b "$SHAPE" -f Segment -d 2 -n 100000 > "$TMP/seg2.txt"
NT="$(get "$W/opt.nif" "$SHAPE" "Num Triangles")"
NP="$(get "$W/opt.nif" "$SHAPE" "Num Primitives")"
TILED="$(TMP="$TMP" NT="$NT" python - <<'PY'
import os, re
T, nt = os.environ['TMP'], int(os.environ['NT'])
nums = []
for ln in open(T + '/seg2.txt', encoding='utf-8', errors='replace'):
    m = re.search(r'(Start Index|Num Primitives)\s+<\w+>\s*=\s*(\d+)', ln)
    if m:
        nums.append((m.group(1), int(m.group(2))))
segs, cur = [], None
for k, v in nums:
    if k == 'Start Index':
        cur = v // 3
    elif cur is not None:
        segs.append((cur, v)); cur = None
total = sum(n for _, n in segs)
inrange = all(s >= 0 and s + n <= nt for s, n in segs)
print('yes' if (total == nt and inrange) else 'no total=%d nt=%d inrange=%s' % (total, nt, inrange))
PY
)"
check "Num Primitives matches Num Triangles" "$NP" "$NT"
check "segments tile [0, Num Triangles) and stay in range" "$TILED" "yes"

echo
echo "checks passed: $pass   failed: $fail"
[ "$fail" -eq 0 ] || exit 1
