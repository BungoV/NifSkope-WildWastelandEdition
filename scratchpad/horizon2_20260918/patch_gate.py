#!/usr/bin/env python3
"""Splice G6 into tests/spells/lodgen_horizon.sh. Byte splice, unique anchors,
line endings preserved (the file is LF-only and stays so)."""

P = 'E:/Projects/NifskopeWildWastelandEdition/tests/spells/lodgen_horizon.sh'
b = open(P, 'rb').read()
s = b.decode('utf-8')
crlf, lf = b.count(b'\r\n'), b.count(b'\n')
print('line endings before: CRLF %d of %d LF, %d bytes' % (crlf, lf, len(b)))
assert crlf == 0, 'expected an LF-only file'


def sub(old, new):
    global s
    assert s.count(old) == 1, 'anchor not unique (%d): %r' % (s.count(old), old[:90])
    s = s.replace(old, new)


# 1. the header paragraph: G6 named where G1..G5 are named
sub("""#   G4 THE SYNTHETIC SELF-TEST.""",
    """#   G6 THE THIRD WITNESS, and it is the check G3 cannot be. G3's reference
#      calls `LodgenHorizonField::maxAlong`, the same function the march calls
#      (src/lodghorizonrefute.h:107,112), so it MOVES WHEN THE MARCH MOVES. Lane
#      HORIZON2 found the two agreeing with each other at 97% while both stood
#      about 8.5 degrees above the real skyline, and the sheet called a whole
#      downtown chunk 0.0% lit at a 15-degree sun. G6 scores the sheet against
#      ten receivers whose skylines were computed from the RAW inputs -- a
#      1-degree pencil over the BTD heightmap plus every placement as an exact
#      world box -- frozen into `lodgen_horizon_witness.json`, which shares no
#      line of code with `src/lodghorizon.h`. Its bars are 9.0 degrees of mean
#      error and 2.0 of signed BIAS, set from the measurement and not round:
#      the same pencil run over the 128-unit LATTICE the march reads already
#      misses by 7.33, so the lattice owns most of the error and a 2-degree bar
#      is unreachable by any march. The bias bar is the one that bites. THE RED
#      CONTROL is the sheet the PRE-FIX exe baked, fed to the same script with
#      `--expect-fail`: it must fail, on a real file, or the check is theatre.
#
#   G4 THE SYNTHETIC SELF-TEST.""")

# 2. the fixture line, next to the others
sub("""REF="$ROOT/tests/spells/lodgen_horizon_refuters.py\"""",
    """REF="$ROOT/tests/spells/lodgen_horizon_refuters.py"
WIT="$ROOT/tests/spells/lodgen_horizon_witness.py"
# the RED CONTROL for G6: the sheet the exe baked BEFORE the HORIZON2 footprint
# fix. It is a real shipped artefact, not a mutation, and it must fail G6.
OLDVT="${OLDVT:-$ROOT/scratchpad/horizon1_20260918/v8/vt/FO4CSLOD/Commonwealth}\"""")

# 3. the check itself, immediately before G5
sub("""# ---- G5 the neighbours -----------------------------------------------------""",
    """# ---- G6 the third witness --------------------------------------------------
echo "G6 the third witness (raw inputs, no shared code) and the pre-fix sheet as the red control"
if [ -f "$WIT" ] && [ -f "$V8VT/Commonwealth.VT.4.lodt" ]; then
	"$PY" "$WIT" "$V8VT/Commonwealth.VT.4.lodt" --quiet > "$OUT/witness.log" 2>&1
	rc=$?
	ck "G6 the sheet stands up to the raw-input witness" $rc \\
		"$(grep -m1 "receivers x" "$OUT/witness.log")"
	if [ -f "$OLDVT/Commonwealth.VT.4.lodt" ]; then
		"$PY" "$WIT" "$OLDVT/Commonwealth.VT.4.lodt" --quiet --expect-fail \\
			> "$OUT/witness_control.log" 2>&1
		rc=$?
		if [ "$rc" -eq 0 ]; then
			echo "  red  G6 control: the PRE-FIX sheet                $(grep -m1 "^witness: FAIL" "$OUT/witness_control.log" | cut -c10-)"
			pass=$((pass+1))
		else
			ck "G6 control: the PRE-FIX sheet must FAIL" 1 \\
				"$(grep -m1 "receivers x" "$OUT/witness_control.log")"
		fi
	else
		echo "  SKIP G6 control -- no pre-fix sheet at $OLDVT"; skip=$((skip+1))
	fi
	# the fixture is data, and a gate that cannot see its fixture rot is not a
	# gate: the terrain-only column must stay far below the full skyline, or the
	# placements have fallen out of the witness and it is measuring bare ground
	"$PY" - "$ROOT/tests/spells/lodgen_horizon_witness.json" <<-'EOF' > "$OUT/witness_fixture.log" 2>&1
		import json, sys
		W = json.load(open(sys.argv[1]))
		n = len(W['receivers']); a = W['azimuths']
		full = sum(sum(r['trueDirDeg']) for r in W['receivers']) / (n * a)
		ter = sum(sum(r['terrainOnlyDeg']) for r in W['receivers']) / (n * a)
		print('%d receivers x %d bins; witness mean %.2f deg, terrain-only mean %.2f deg' % (n, a, full, ter))
		sys.exit(0 if (n == 10 and a == 16 and full > ter + 20.0) else 1)
	EOF
	ck "G6 the fixture still carries the placements" $? "$(cat "$OUT/witness_fixture.log")"
else
	echo "  SKIP G6 -- no witness script or no sheet at $V8VT"; skip=$((skip+1))
fi

# ---- G5 the neighbours -----------------------------------------------------""")

out = s.encode('utf-8')
open(P, 'wb').write(out)
print('line endings after:  CRLF %d of %d LF, %d bytes' % (out.count(b'\r\n'), out.count(b'\n'), len(out)))
