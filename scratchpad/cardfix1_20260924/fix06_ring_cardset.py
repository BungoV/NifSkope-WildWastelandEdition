# CARDFIX1 step 5 (IMPOSTORRING1), part 3: the NifSkope card reader (src/impostorcard.*) reads a ring set:
# `views` V + `grid` [V,1] on a card or a cardArray, frames in ONE row, frameOffset 2*V long. `oct` stays 0
# for a ring, so nothing that lays out an N x N grid can take a ring for one; cols()/rows() are the layout.
ROOT = 'E:/Projects/NifskopeWWE-cardfix1/'
T = chr(9)


def patch(path, edits):
    b = open(ROOT + path, 'rb').read()
    cr = b.count(b'\r')
    s = b.decode('utf-8')
    for old, new in edits:
        c = s.count(old)
        assert c == 1, (path, old[:80], c)
        s = s.replace(old, new)
    out = s.encode('utf-8')
    assert out.count(b'\r') == cr, path
    open(ROOT + path, 'wb').write(out)
    print('patched', path)


patch('src/impostorcard.h', [
    (T + 'int oct = 0;' + T * 3 + '//!< N, the grid side (spec 225)\n',
     T + 'int oct = 0;' + T * 3 + '//!< N, the grid side (spec 225); 0 on a ring set\n'
     + T + '/*! THE HORIZON RING (lane CARDFIX1 step 5, IMPOSTORRING1; bungo 2026-09-23:\n'
     + T + ' *  "22.5 degrees per take"). > 0 = the set\'s `views` V: V frames in ONE row,\n'
     + T + ' *  frame v photographed from azimuth 360*v/V at elevation 0 (the aggregate\'s\n'
     + T + ' *  layout, docs/LODGEN_LODM_FORMAT.md 3a). `oct` is then 0, so nothing that\n'
     + T + ' *  lays out an N x N grid can take a ring for one; cols()/rows() are the\n'
     + T + ' *  sheet\'s layout either way, and frame (i, j) is at index j*cols() + i. */\n'
     + T + 'int views = 0;\n'
     + T + 'int cols() const { return views > 0 ? views : oct; }\n'
     + T + 'int rows() const { return views > 0 ? 1 : oct; }\n'
     + T + 'bool ring() const { return views > 0; }\n'),
])

patch('src/impostorcard.cpp', [
    ('void readFrameOffsets( const QJsonObject & o, ImpostorCardSet & set )\n{\n',
     '/* THE HORIZON RING (CARDFIX1 step 5): `views` V and `grid` [V,1], together or\n'
     ' * not at all, and never beside `oct` -- a set that declares both layouts\n'
     ' * declares neither. False = refused, with the reason in set.error. */\n'
     'bool readRing( const QJsonObject & o, ImpostorCardSet & set, const QString & lodmPath )\n{\n'
     + T + 'set.views = o.value( QStringLiteral( "views" ) ).toInt( 0 );\n'
     + T + 'if ( set.views <= 0 && !o.contains( QStringLiteral( "grid" ) ) )\n'
     + T * 2 + 'return true;\n'
     + T + 'const QJsonArray g = o.value( QStringLiteral( "grid" ) ).toArray();\n'
     + T + 'if ( set.views < 4 || set.views > 64 || g.size() != 2 || g.at( 0 ).toInt() != set.views\n'
     + T * 2 + '|| g.at( 1 ).toInt() != 1 ) {\n'
     + T * 2 + 'set.error = QString( "%1: a ring set needs views 4..64 and grid [views,1];"\n'
     + T * 4 + '" it says views %2, grid %3 entries" ).arg( lodmPath ).arg( set.views ).arg( g.size() );\n'
     + T * 2 + 'return false;\n'
     + T + '}\n'
     + T + 'if ( set.oct != 0 ) {\n'
     + T * 2 + 'set.error = QString( "%1: declares BOTH oct %2 and views %3 -- one layout per set" )\n'
     + T * 4 + '.arg( lodmPath ).arg( set.oct ).arg( set.views );\n'
     + T * 2 + 'return false;\n'
     + T + '}\n'
     + T + 'return true;\n'
     '}\n\n'
     'void readFrameOffsets( const QJsonObject & o, ImpostorCardSet & set )\n{\n'),
    (T + 'if ( !v.isArray() || set.oct <= 0 )\n' + T * 2 + 'return;\n'
     + T + 'const QJsonArray a = v.toArray();\n' + T + 'const int want = 2 * set.oct * set.oct;\n',
     T + 'if ( !v.isArray() || set.cols() <= 0 )\n' + T * 2 + 'return;\n'
     + T + 'const QJsonArray a = v.toArray();\n' + T + 'const int want = 2 * set.cols() * set.rows();\n'),
    ('if ( frameOffset.isEmpty() || oct <= 0 || i < 0 || j < 0 || i >= oct || j >= oct )\n',
     'if ( frameOffset.isEmpty() || cols() <= 0 || i < 0 || j < 0 || i >= cols() || j >= rows() )\n'),
    (T + 'const int k = 2 * ( j * oct + i );\n', T + 'const int k = 2 * ( j * cols() + i );\n'),
    (T + 'out << QString( "grid: %1x%1 = %2 frames" ).arg( oct ).arg( oct * oct );\n',
     T + 'if ( ring() )\n'
     + T * 2 + 'out << QString( "grid: RING of %1 views at 360/%1 degrees, one row = %1 frames" ).arg( views );\n'
     + T + 'else\n'
     + T * 2 + 'out << QString( "grid: %1x%1 = %2 frames" ).arg( oct ).arg( oct * oct );\n'),
    (T * 2 + 'set.oct  = card.value( "oct" ).toInt( 0 );\n',
     T * 2 + 'set.oct  = card.value( "oct" ).toInt( 0 );\n'
     + T * 2 + 'if ( !readRing( card, set, lodmPath ) )\n' + T * 3 + 'return set;\n'),
    (T * 2 + 'set.oct  = arr.value( "oct" ).toInt( 0 );\n',
     T * 2 + 'set.oct  = arr.value( "oct" ).toInt( 0 );\n'
     + T * 2 + 'if ( !readRing( arr, set, lodmPath ) )\n' + T * 3 + 'return set;\n'),  # applied as `return;`, fixed by hand (sed) the same minute
    (T + 'if ( set.oct < ImpostorOct::kMinGrid || set.oct > ImpostorOct::kMaxGrid ) {\n',
     T + 'if ( !set.ring() && ( set.oct < ImpostorOct::kMinGrid || set.oct > ImpostorOct::kMaxGrid ) ) {\n'),
])
