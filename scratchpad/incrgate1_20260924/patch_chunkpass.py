"""INCRGATE1 step 2a: the ledger API into src/lodgenchunkpass.{h,cpp}.

Reads the moved statics VERBATIM out of src/nifcli.cpp (run before patch_nifcli.py).
Asserts every anchor once and the CR count unchanged (all three files are LF-only)."""
import os, sys
W = 'E:/Projects/NifskopeWWE-incrgate1'
L = W + '/scratchpad/incrgate1_20260924'

def rd(p):
    with open(p, 'rb') as f:
        return f.read().decode('utf-8')

def wr(p, s, cr0):
    assert s.count('\r') == cr0, (p, s.count('\r'), cr0)
    with open(p, 'wb') as f:
        f.write(s.encode('utf-8'))

def one(s, a):
    n = s.count(a)
    assert n == 1, (a[:60], n)
    return s.index(a)

# ---- header
hp = W + '/src/lodgenchunkpass.h'
h = rd(hp); hcr = h.count('\r')
assert 'lodgenIdentityDump' not in h
one(h, '#include <QString>\n')
h = h.replace('#include <QString>\n', '#include <QHash>\n#include <QString>\n')
end = '#endif // LODGENCHUNKPASS_H'
i = one(h, end)
h = h[:i] + rd(L + '/snip_header.txt').lstrip('\n') + '\n' + h[i:]
wr(hp, h, hcr)

# ---- the moved statics, verbatim, out of nifcli
n = rd(W + '/src/nifcli.cpp')
a0 = one(n, '/*! WHERE THE RECORD GOES, composed in ONE place')
a1 = one(n, '/*! `--keep-bto` (lane BTOFREE1, 2026-09-16), a global')
b0 = one(n, '/*! Flags whose TOKEN AND VALUE are both dropped from the digest')
b1s = 'static QString lodgenSwitchDigestOf( const QStringList & a )\n{'
b1 = one(n, b1s)
b1 = n.index('\n}\n', b1) + 3
moved = n[a0:a1] + n[b0:b1]
for old, new in (('static QString lodbRecordPath(', 'QString lodbRecordPath('),
                 ('static QString lodbFindRecord(', 'QString lodbFindRecord('),
                 ('static QString lodgenSwitchDigestOf(', 'QString lodgenSwitchDigestOf(')):
    assert moved.count(old) == 1, old
    moved = moved.replace(old, new)

# ---- the implementation
cp = W + '/src/lodgenchunkpass.cpp'
c = rd(cp); ccr = c.count('\r')
assert 'chunkKey' not in c and 'lodgenIdentityDump' not in c
inc = '#include "lodgenlayout.h"\n'
one(c, inc)
c = c.replace(inc, inc.replace('lodgenlayout', 'lodbfile') + inc + '#include "lodofile.h"\n')
one(c, '#include <QBuffer>\n')
c = c.replace('#include <QBuffer>\n', '#include <QBuffer>\n#include <QCryptographicHash>\n#include <QDateTime>\n')
one(c, '#include <QFileInfo>\n')
c = c.replace('#include <QFileInfo>\n', '#include <QFileInfo>\n#include <QHash>\n')
one(c, '#include <QSaveFile>\n')
c = c.replace('#include <QSaveFile>\n', '#include <QSaveFile>\n#include <QSet>\n')

impl = rd(L + '/snip_impl.txt')
k = one(impl, 'QStringList lodgenIdentityDump(')
head_end = one(impl, 'QString lodbRecordPath(')
intro = impl[:head_end]                      # the section banner
tail = impl[k:]
glue = ('namespace\n{\n\nQString chunkKey( int cx, int cy )\n{\n'
        '\treturn QString( "%1,%2" ).arg( cx ).arg( cy );\n}\n\n} // namespace\n\n')
c = c.rstrip('\n') + '\n' + intro + moved + '\n' + glue + tail
if not c.endswith('\n'):
    c += '\n'
wr(cp, c, ccr)
print('chunkpass patched: h', h.count('\n'), 'lines, cpp', c.count('\n'), 'lines')
