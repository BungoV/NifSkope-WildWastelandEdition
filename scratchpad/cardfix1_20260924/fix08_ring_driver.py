# CARDFIX1 step 5 (IMPOSTORRING1), part 5: the card bake driver makes the ring the TREE default.
# CANDIDATES=trees -> RING 16 (16 azimuths at 22.5 degrees, bungo 2026-09-23 04:4x); CANDIDATES=missing ->
# RING 0 (the N x N grid, OCT, unchanged -- the ruling names tree cards). RING=0 / RING=16 force either way.
ROOT = 'E:/Projects/NifskopeWWE-cardfix1/'
P = 'tools/bake_impostor_cards.sh'
T = chr(9)
b = open(ROOT + P, 'rb').read()
cr = b.count(b'\r')
s = b.decode('utf-8')


def rep(old, new):
    global s
    assert s.count(old) == 1, old[:60]
    s = s.replace(old, new)


rep('CANDIDATES="${CANDIDATES:-missing}"\n',
    'CANDIDATES="${CANDIDATES:-missing}"\n'
    '# THE HORIZON RING (lane CARDFIX1 step 5, IMPOSTORRING1). bungo 2026-09-23 04:4x, RULED: "for fo4cs\n'
    '# use the convention was 22.5 degrees per take" -- TREE cards are 16 azimuths at elevation 0, one\n'
    '# row (the aggregate\'s ring layout), instead of the OCT x OCT hemi-octahedral grid. So the tree run\n'
    '# defaults to RING=16 and the empty-slot run keeps the grid; RING=0 forces the grid, RING=16 the ring.\n'
    '# With a ring, OCT is not used for the sheets (the library.txt says which one the set carries).\n'
    'if [ "$CANDIDATES" = trees ]; then RING="${RING:-16}"; else RING="${RING:-0}"; fi\n'
    'case "$RING" in\n'
    + T + '0|16) ;;\n'
    + T + '*) echo "RING must be 16 (the ruled 22.5-degree ring) or 0 (the grid); got \'${RING}\'" >&2; exit 2 ;;\n'
    'esac\n')
rep(T + 'echo "oct ${OCT}"\n', T + 'echo "oct ${OCT}"\n' + T + 'echo "ring ${RING}"\n')
rep(T + '# OCT=N adds the octahedral sheets (N x N views, TILE px each) for FO4CS.\n',
    T + '# OCT=N adds the octahedral sheets (N x N views, TILE px each) for FO4CS;\n'
    + T + '# RING=16 photographs the horizon ring instead (16 views in one row, TILE px each).\n')
rep('WW_IMPOSTOR_OCT="${OCT:-}" WW_IMPOSTOR_TILE="$TILE" \\\n',
    'WW_IMPOSTOR_OCT="${OCT:-}" WW_IMPOSTOR_RING="$RING" WW_IMPOSTOR_TILE="$TILE" \\\n')
out = s.encode('utf-8')
assert out.count(b'\r') == cr
open(ROOT + P, 'wb').write(out)
print('patched', P)
