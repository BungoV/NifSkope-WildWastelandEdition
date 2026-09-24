"""AUDIT1 step 7: splice section 1 into the report, before section 2.

Every piece is written LF-only and the target is LF-only (measured: 0 CR in all
six files before the splice). The assert after the join is the whole point --
lane AUDIT1 already put 210 CRs into this document once, which is a MISTAKES
entry -- so the write is refused if the count moves.
"""
import io
import os
import sys
import tempfile

D = 'E:/Projects/NifskopeWildWastelandEdition/scratchpad/audit1_20260916'
P = D + '/lane_audit1_report.md'
PIECES = ['section1_board.md', 'section1_reds.md', 'section1_standing.md',
          'section1_known.md', 'section1_new.md']
ANCHOR = '## 2. A fresh end-to-end bake on three fixture regions'


def main():
    s = io.open(P, encoding='utf-8', newline='').read()
    if s.count(ANCHOR) != 1:
        print('ABORT: the section 2 anchor appears %d times' % s.count(ANCHOR))
        return 1
    if '### 1.1 The board' in s:
        print('ABORT: section 1 is already in the report')
        return 1
    body = []
    for fn in PIECES:
        t = io.open(os.path.join(D, fn), encoding='utf-8', newline='').read()
        if t.count('\r'):
            print('ABORT: %s carries %d CR' % (fn, t.count('\r')))
            return 1
        body.append(t.rstrip('\n'))
    block = '\n\n'.join(body) + '\n\n'
    out = s.replace(ANCHOR, block + ANCHOR, 1)
    if out.count('\r') != s.count('\r'):
        print('ABORT: CR count moved %d -> %d' % (s.count('\r'), out.count('\r')))
        return 1
    f = tempfile.NamedTemporaryFile('w', encoding='utf-8', newline='', dir=D,
                                    delete=False, suffix='.tmp')
    f.write(out)
    f.close()
    os.replace(f.name, P)
    print('report: %d -> %d bytes, %d -> %d lines, CR %d'
          % (len(s), len(out), s.count('\n'), out.count('\n'), out.count('\r')))
    return 0


if __name__ == '__main__':
    sys.exit(main())
