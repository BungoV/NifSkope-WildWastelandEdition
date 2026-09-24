#!/usr/bin/env python3
"""fix02.py -- the most vexing parse, seven times.

`std::vector<T> v( size_t( n ) );` declares a FUNCTION taking a size_t and
returning a vector, because `size_t( n )` is a valid parameter declaration.
Every one of these was written as a one-argument construction and every one
compiled as a declaration; the errors land on the first USE of the name, twenty
lines away, which is why they all arrived at once.

Brace initialisation would change the meaning (it would build a one-element
vector), so each becomes an explicit `resize`, or a two-argument construction
where the fill value is the type's own zero anyway.
"""

P = 'src/lodtfile.cpp'
raw = open(P, 'rb').read()
assert raw.count(b'\r') == 0, 'src is LF-only'
s = raw.decode('utf-8')

FIXES = [
    ('\tstd::vector<quint8> raw( size_t( tileBytes ) );',
     '\tstd::vector<quint8> raw( size_t( tileBytes ), quint8( 0 ) );'),
    ('\t\tstd::vector<quint16> row( size_t( gw ) );',
     '\t\tstd::vector<quint16> row( size_t( gw ), quint16( 0 ) );'),
    ('\tstd::vector<std::vector<qint32>> adj( size_t( nComp ) );',
     '\tstd::vector<std::vector<qint32>> adj( size_t( nComp ), std::vector<qint32>() );'),
    ('\tstd::vector<std::vector<Lower>> lower( size_t( nBodies ) );',
     '\tstd::vector<std::vector<Lower>> lower( size_t( nBodies ), std::vector<Lower>() );'),
    ('\t\t\tstd::vector<qint64> ord( size_t( nBodies ) );',
     '\t\t\tstd::vector<qint64> ord( size_t( nBodies ), qint64( 0 ) );'),
    ('\t\tstd::vector<int> ord( size_t( n ) );',
     '\t\tstd::vector<int> ord( size_t( n ), 0 );'),
]

for old, new in FIXES:
    n = s.count(old)
    assert n >= 1, 'anchor missing: %r' % old
    s = s.replace(old, new)
    print('  %d x %s' % (n, old.strip()[:60]))

out = s.encode('utf-8')
assert out.count(b'\r') == 0
open(P, 'wb').write(out)
print('ok, %d bytes' % len(out))
