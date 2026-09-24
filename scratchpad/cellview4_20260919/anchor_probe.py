#!/usr/bin/env python3
"""Count the candidate anchor prefixes in their real files before hookup.py is
written.  Per skill ww-anchored-hookup section 5a: type only a PREFIX, let the
file supply the tabs and the trailing comment, and print repr() beside the
count because that is the only rendering in which a tab and four spaces differ.
"""
import io
import sys

CAND = [
    ('NifSkope.pro', 'src/cellground.cpp'),
    ('NifSkope.pro', 'src/cellground.h'),
    ('src/esmdata.h', 'struct EsmLandLayer'),
    ('src/esmdata.h', 'float opacity[17][17];'),
    ('src/esmdata.cpp', 'const int quadrant = int( f.readUInt8() );'),
    ('src/esmdata.cpp', 'layer.ltex = ltex;'),
    ('src/cellview.cpp', '#include "cellground.h"'),
    ('src/cellview.cpp', 'float chan[3] = { 1.0f, 1.0f, 1.0f };'),
    ('src/cellview.cpp', 'QVector<Bucket> groundBuckets;'),
    ('src/cellview.cpp', 'if ( spec.terrain ) {'),
    ('src/cellview.cpp', 'nif->set<int>( iAlpha, "Flags", 4844 );'),
    ('src/cellview.cpp', 'QModelIndex iRoot = nif->insertNiBlock('),
    ('src/cellview.cpp', '|| m.endsWith( QLatin1String( "markerx.nif" ) )'),
    ('src/cellview.cpp', 'if ( b.withColour )'),
    ('src/cellview.cpp', 'ByteColor4( FloatVector4( o.chan[0]'),
]

ROOT = 'E:/Projects/NifskopeWildWastelandEdition/'


def main():
    for path, prefix in CAND:
        try:
            raw = io.open(ROOT + path, 'rb').read()
        except OSError as e:                         # noqa: BLE001
            print('%-18s MISSING %s' % (path, e))
            continue
        text = raw.decode('utf-8', 'replace')
        crlf = raw.count(b'\r\n')
        lines = text.split(chr(10))
        hits = [l for l in lines if l.strip().startswith(prefix)]
        flag = 'OK  ' if len(hits) == 1 else '**%d' % len(hits)
        print('%s %-16s %-46s' % (flag, path, prefix[:46]))
        for h in hits[:3]:
            print('        %r' % h.rstrip(chr(13))[:96])
        if len(hits) == 1:
            print('        endswith-CR=%s  file CRLF lines=%d'
                  % (hits[0].endswith(chr(13)), crlf))
    return 0


if __name__ == '__main__':
    sys.exit(main())
