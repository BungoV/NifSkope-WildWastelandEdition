"""Point the two pictures' candidate columns at the BAKED variants when they
exist on disk, and keep the simulation only as a labelled fallback."""
import io
import sys

P = r'E:\Projects\NifskopeWildWastelandEdition\scratchpad\roads3_20260911\r3_pics.py'
s = io.open(P, encoding='utf-8', newline='').read()
if 'def candidate(' in s:
    sys.exit('REFUSED: already patched')


def sub(old, new):
    global s
    if s.count(old) != 1:
        sys.exit('REFUSED: %d matches for %r' % (s.count(old), old[:60]))
    s = s.replace(old, new)


sub("CAND = (0.326, 0.83)",
    """CAND = ((0.326, 'new_op0326'), (0.83, 'new_op083'))


def candidate(tile, a, variant, A, G):
    \"\"\"The candidate sheet: the BAKED one if that variant is on disk, else the
    simulation, and the label says which. A baked panel has been through 8-bit
    quantisation and BC1; a simulated one has not.\"\"\"
    try:
        return R3.ours(variant, tile), 'BAKED'
    except Exception:
        return G + (A - G) * a, 'SIMULATED'""")

sub("""        for a in CAND:
            W = G + (A - G) * a
            gw = gates(W, Lv, mask, sur, sd)
            why = ('fits vanilla`s RISE on (-20,20)' if a == 0.326
                   else 'fits vanilla`s LEVEL on (-20,20)')
            cols.append(panel(W, tile, '--road-opacity %.3f   SIMULATED' % a,""",
    """        for a, variant in CAND:
            W, how = candidate(tile, a, variant, A, G)
            gw = gates(W, Lv, mask, sur, sd)
            why = ('fits vanilla`s RISE on (-20,20)' if a == 0.326
                   else 'fits vanilla`s LEVEL on (-20,20)')
            why += ' -- %s' % how
            cols.append(panel(W, tile, '--road-opacity %.3f   %s' % (a, how),""")

sub("""                               why + ' -- SIMULATED, not baked'],""",
    """                               why],""")

sub("""    for a, col in ((0.326, (255, 200, 120)), (0.83, (255, 140, 90))):
        series.append(('--road-opacity %.3f (simulated)' % a,
                       R3.profile(R3.L(G + (A - G) * a), sd, lo, hi), col))""",
    """    for (a, variant), col in zip(CAND, ((255, 200, 120), (255, 140, 90))):
        W, how = candidate(tile, a, variant, A, G)
        series.append(('--road-opacity %.3f (%s)' % (a, how.lower()),
                       R3.profile(R3.L(W), sd, lo, hi), col))""")

sub("""                    '(-8,8). The two orange lines are SIMULATED, not baked.',""",
    """                    '(-8,8). The two orange lines are the shipped switch at '
                    'two settings, baked.',""")

io.open(P, 'w', encoding='utf-8', newline='').write(s)
print('r3_pics.py now prefers the baked variants')
