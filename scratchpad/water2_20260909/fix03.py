#!/usr/bin/env python3
"""fix03.py -- the vexing parse again, one layer deeper.

`std::vector<std::vector<T>> v( size_t( n ), std::vector<T>() );` is STILL a
function declaration: the second argument reads as an unnamed parameter of
function type `std::vector<T>()`. Adding a fill value only works when the value
cannot be read as a parameter declaration, which `quint8( 0 )` cannot and
`std::vector<T>()` can.

`resize` is the form that has no second reading at all.
"""

P = 'src/lodtfile.cpp'
raw = open(P, 'rb').read()
assert raw.count(b'\r') == 0, 'src is LF-only'
s = raw.decode('utf-8')

FIXES = [
    ('\tstd::vector<std::vector<qint32>> adj( size_t( nComp ), std::vector<qint32>() );',
     '\tstd::vector<std::vector<qint32>> adj;\n\tadj.resize( size_t( nComp ) );'),
    ('\tstd::vector<std::vector<Lower>> lower( size_t( nBodies ), std::vector<Lower>() );',
     '\tstd::vector<std::vector<Lower>> lower;\n\tlower.resize( size_t( nBodies ) );'),
    ('\t\tstd::vector<std::vector<qint32>> bucket( size_t( bx ) * size_t( by ) );',
     '\t\tstd::vector<std::vector<qint32>> bucket;\n\t\tbucket.resize( size_t( bx ) * size_t( by ) );'),
    ('\t\tstd::vector<std::vector<qint32>> group;',
     '\t\tstd::vector<std::vector<qint32>> group;'),
]

for old, new in FIXES:
    n = s.count(old)
    assert n >= 1, 'anchor missing: %r' % old
    s = s.replace(old, new)
    print('  %d x %s' % (n, old.strip()[:66]))

out = s.encode('utf-8')
assert out.count(b'\r') == 0
open(P, 'wb').write(out)
print('ok, %d bytes' % len(out))
