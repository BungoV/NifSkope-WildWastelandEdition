# lodgen_cardlink ID red: compare the kept plain bakes (new exe cbdbffe7 vs rung before_cardlink1)
# after taking the colour stream out with w4_gate.strip(); report every remaining byte by table.
import sys, struct
sys.argv = ['x', '.', '.']
exec(open('w4_gate.py').read().split('\ndef pair(d):')[0])
K = 'cardlink_keep/'; P = 'FO4CSLOD/Commonwealth/Commonwealth.'
A = rd(K + 'idn_rung/' + P + 'lodo'); B = rd(K + 'idn_new/' + P + 'lodo')
print('versions', A[4], B[4], 'colourRows', struct.unpack_from('<I', B, 0xD4)[0] if B[4] == 5 else 0)
S = strip(B); d, dl = diffs(A, S)
offV = struct.unpack_from('<Q', A, 0x98)[0]; nV = struct.unpack_from('<I', A, 0x60)[0]
inV = [x for x in d if offV <= x < offV + nV * 16]
print('.lodo stripped: %d bytes differ, length %+d; %d in the vertex table' % (len(d), dl, len(inV)))
for x in d[:12]:
    r, c = divmod(x - offV, 16)
    print('  0x%X' % x, ('vertex row %d byte %d' % (r, c)) if x in inV else 'outside vertices', A[x], '->', S[x])
di, dli = diffs(rd(K + 'idn_rung/' + P + 'lodi'), rd(K + 'idn_new/' + P + 'lodi'))
print('.lodi: %d differ, length %+d, at %s' % (len(di), dli, ['0x%X' % x for x in di]))
exec('def attributed' + open('w4_gate.py').read().split('\ndef attributed')[1].split('\ndef g1(')[0])
exec(open('w4_gate.py').read().split('ATTRIBUTED = ')[0][-1:] + 'ATTRIBUTED = ' + open('w4_gate.py').read().split('ATTRIBUTED = ')[1].split('\n')[0])
B2, applied = attributed(A, B); d2, dl2 = diffs(A, strip(B2))
print('after the W4 attribution (%s): %d bytes differ %s, length %+d' % (applied, len(d2), ['0x%X' % x for x in d2], dl2))
