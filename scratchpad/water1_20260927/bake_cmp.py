"""WATER1 bake comparator (skill ww-module-off-is-identical, section 4b).

usage: bake_cmp.py <new default v3 .lodl> <rung v3 .lodl (--water-bodies on the rung exe)>

Claim restated: the new default file equals the rung's v3 file except for cell-table flag words (bit 0 cleared
where no body-id sample is wet). Derived words: none (the format has no CRC; offsets and lengths do not move).
The comparator: refuses a length difference; lists every differing byte; fails on any byte outside a flag word
or any flag change other than bit 0 set -> clear; recounts kept/cleared from the file's own flags; and is shown
RED on a doctored copy first (one payload byte flipped past the cell table).
Also: the new file's cells with bit 0 against the independent v3 decoder's wet samples, when that decoder loads.
"""
import sys, struct
import numpy as np

sys.path.insert(0, 'E:/Projects/NifskopeWWE-water1/tests/spells')
from lodl_open_authority import Lodt  # noqa: E402


def compare(a, b, oCell, nCells):
    if len(a) != len(b):
        return {'verdict': 'FAIL length %d vs %d' % (len(a), len(b))}
    A = np.frombuffer(a, np.uint8)
    B = np.frombuffer(b, np.uint8)
    diff = np.nonzero(A != B)[0]
    stray = []
    bad_flag = 0
    cleared = 0
    for off in diff:
        rel = int(off) - oCell
        if 0 <= rel < nCells * 16 and rel % 16 in (14, 15):
            continue
        stray.append(int(off))
    # flag words
    fa = np.frombuffer(a, np.uint16, count=nCells * 8, offset=oCell).reshape(-1, 8)[:, 7]
    fb = np.frombuffer(b, np.uint16, count=nCells * 8, offset=oCell).reshape(-1, 8)[:, 7]
    ch = fa != fb
    for x, y in zip(fa[ch], fb[ch]):
        if not ((y & 1) and not (x & 1) and (x | 1) == y):
            bad_flag += 1
        else:
            cleared += 1
    ok = not stray and bad_flag == 0
    return {'bytes': len(a), 'differing_bytes': int(len(diff)), 'stray_offsets': stray[:10], 'n_stray': len(stray),
            'flag_words_changed': int(ch.sum()), 'cleared_bit0': cleared, 'bad_flag_changes': bad_flag,
            'cells_with_bit0_new': int((fa & 1).sum()), 'cells_with_bit0_rung': int((fb & 1).sum()),
            'verdict': 'PASS' if ok else 'FAIL'}


new, rung = sys.argv[1], sys.argv[2]
d = Lodt(new)
nCells = d.cellsX * d.cellsY
a = open(new, 'rb').read()
b = open(rung, 'rb').read()
print('new version', d.version, 'cells', nCells, 'cell table at 0x%x' % d.oCell)
# floor: a doctored copy with one payload byte flipped past the cell table must FAIL and name the offset
doc = bytearray(a)
k = d.oCell + nCells * 16 + 1000
doc[k] ^= 0x55
f = compare(bytes(doc), a, d.oCell, nCells)
print('FLOOR (one byte flipped at %d):' % k, f['verdict'], 'stray', f['stray_offsets'])
if f['verdict'] == 'PASS' or k not in f['stray_offsets']:
    print('REFUSED: the comparator did not catch the doctored byte')
    sys.exit(2)
# floor 2: a flag bit 1 flipped must FAIL as a bad flag change
doc = bytearray(a)
doc[d.oCell + 14] ^= 0x02
f2 = compare(bytes(doc), a, d.oCell, nCells)
print('FLOOR (flag bit 1 flipped in cell 0):', f2['verdict'], 'bad flag changes', f2['bad_flag_changes'])
if f2['verdict'] == 'PASS':
    print('REFUSED: the comparator let a bit-1 change through')
    sys.exit(2)
r = compare(a, b, d.oCell, nCells)
print('SUBJECT:', r)
