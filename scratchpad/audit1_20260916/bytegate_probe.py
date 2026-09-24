"""AUDIT1 step 1: WHERE two files that must be byte-identical differ.
Prints one verdict line per pair: the count of differing bytes, the first and
last differing offset, and the first eight differing offsets with both values.
"""
import sys

T = 'C:/Users/bungo/AppData/Local/Temp/'
PAIRS = [
    ('terrain sheet',
     T + 'Lodgen_PANEL1_base/textures/terrain/Commonwealth/Commonwealth.4.-20.24.DDS',
     T + 'Lodgen_PANEL1_cli_sweep/tex/Commonwealth.4.-20.24.DDS'),
    ('lodi',
     T + 'Lodgen_PANEL1_base/FO4CSLOD/Commonwealth/Commonwealth.lodi',
     T + 'Lodgen_PANEL1_cli_sweep/FO4CSLOD/Commonwealth/Commonwealth.lodi'),
]

for name, a, b in PAIRS:
    A = open(a, 'rb').read()
    B = open(b, 'rb').read()
    if len(A) != len(B):
        print('%s: %d vs %d bytes' % (name, len(A), len(B)))
        continue
    d = [i for i in range(len(A)) if A[i] != B[i]]
    if not d:
        print('%s: identical' % name)
        continue
    head = ', '.join('0x%X %02X/%02X' % (i, A[i], B[i]) for i in d[:8])
    print('%s: %d of %d bytes differ, first 0x%X, last 0x%X; %s'
          % (name, len(d), len(A), d[0], d[-1], head))
sys.exit(0)
