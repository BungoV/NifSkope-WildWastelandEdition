"""IMPOSTORFIX5 -- splice row 14c into tests/spells/impostor_draw.sh.

IMPOSTORFIX4 s6 wrote this row as text for a build lane. It fails on the
sheets exe ee87eb9e baked (the 8-ring cliff) and passes on the ramped ones,
which is the whole point: it is a row that goes red on the state this lane
found and green on the state it leaves.

The floor is IMPOSTORFIX4's 0.5 per cent, unchanged and NOT lowered. The
maple still fails it at 1.089 and that is deliberate and said out loud in the
comment: not all of the maple's chips come from the cliff.
"""
import os, sys

ROOT = r'E:\Projects\NifskopeWildWastelandEdition'
PATH = os.path.join(ROOT, 'tests', 'spells', 'impostor_draw.sh')

ANCHOR = '''	bad "14b $( tail -1 "$work/reach.txt" ) -- the height outside the silhouette is the card plane, i.e. the bake predates the 8-ring fill"
fi
'''

ADD = '''	bad "14b $( tail -1 "$work/reach.txt" ) -- the height outside the silhouette is the card plane, i.e. the bake predates the 8-ring fill"
fi

# ---------------------------------------------------------------------------
# 14c. DOES THE HEIGHT SHEET DECODE TO WHAT THE ENCODER WAS GIVEN?
#
#     THIS IS THE ROW THAT FAILS ON exe ee87eb9e's SHEETS. 14b asks whether
#     the fill reached outside the silhouette at all; this asks whether what
#     it wrote SURVIVED BC1. It does not, where the fill ends on a cliff: a
#     hard step inside one 4x4 block gives the block a height range its single
#     colour line cannot carry, and the block comes back as a flat chip a
#     person sees on a trunk.
#
#     Per cent of the sheet's texels whose decoded height is more than twelve
#     levels from the encoder's input. `impostor_height_ref.py` reconstructs
#     that input by porting lodgenRepairOctHeight and REFUSES unless its own
#     four-integer census matches the one the bake printed, so a drifted port
#     fails by name instead of becoming the reference.
#
#     Measured, five subjects, both sheet sets, this instrument:
#
#       subject    8-ring cliff (ee87eb9e)   16-ring ramp (this exe)
#       blast_n4         0.663%                    0.011%
#       blast_n8         0.886%                    0.035%
#       maple_n4         2.252%                    1.089%
#       dead_n4          1.581%                    0.362%
#       rock_n4          1.382%                    0.699%
#
#     The floor is IMPOSTORFIX4's 0.5 per cent, and it is NOT set to let
#     everything through: the maple is 1.089 after the ramp and still FAILS.
#     That is the honest state -- the cliff was one source of the maple's
#     large-range blocks and not the only one -- and a lane that wants this
#     row green on the maple has to find the other source, not move the bar.
# ---------------------------------------------------------------------------
DECODE_MAX="${IMPOSTOR_DECODE_MAX:-0.5}"
if "$PY" - "$IMPOSTOR_LODM" "$DECODE_MAX" "$here" > "$work/hdec.txt" 2>&1 <<'PYEOF'
import sys
sys.path.insert(0, sys.argv[3])
from impostor_height_ref import decode_error
try:
	r = decode_error(sys.argv[1])
except Exception as e:
	print('%s' % e)
	sys.exit(2)
print('oct height decode: %.3f%% of %d texels more than 12 levels from the encoder\\'s input '
      '(max %.1f%%, mean err %.2f, worst %d, mean 4x4 block range %.2f, fill = %s)'
      % (r['pct'], r['texels'], float(sys.argv[2]), r['mean'], r['worst'], r['blk_range'],
         ('%d-ring ramp' % r['ramp']) if r['ramp'] else '8-ring cliff'))
sys.exit(0 if r['pct'] <= float(sys.argv[2]) else 1)
PYEOF
then
	ok "14c $( tail -1 "$work/hdec.txt" )"
else
	if [ "$?" = "2" ]; then
		bad "14c REFUSED: $( tail -1 "$work/hdec.txt" )"
	else
		bad "14c $( tail -1 "$work/hdec.txt" ) -- the height fill ends on a cliff and BC1 cannot carry it"
	fi
fi
'''


def main(apply_it):
    b = open(PATH, 'rb').read()
    s = b.decode('utf-8')
    n = s.count(ANCHOR)
    print('impostor_draw.sh  anchor matches %d  %s' % (n, 'OK' if n == 1 else 'REFUSE'))
    print('before: bytes %d  CR %d  LF %d' % (len(b), b.count(b'\r'), b.count(b'\n')))
    if n != 1:
        return 1
    if not apply_it:
        print('--check only: nothing written. Pass --apply to write.')
        return 0
    out = s.replace(ANCHOR, ADD, 1).encode('utf-8')
    assert out.count(b'\r') == b.count(b'\r'), 'CR count moved'
    open(PATH, 'wb').write(out)
    print('after:  bytes %d  CR %d  LF %d' % (len(out), out.count(b'\r'), out.count(b'\n')))
    print('WROTE', PATH)
    return 0


if __name__ == '__main__':
    sys.exit(main('--apply' in sys.argv))
