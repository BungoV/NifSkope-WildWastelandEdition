# CARDFIX1 step 5 (IMPOSTORRING1), part 2: lodgen reads a `ring V` sidecar and writes the ring into the
# card .lodm (`views` + `grid:[V,1]`, NO `oct` key -- an N x N reader then finds no grid and refuses the
# set by that key's name), the card arrays carry ring sets (their own group, `views`/`grid` on the array),
# and the aggregate intake refuses a ring set BY NAME (lodgenaggregate.cpp composes from N x N grids only
# and is not this lane's file).
ROOT = 'E:/Projects/NifskopeWWE-cardfix1/'
P = 'src/lodgen.cpp'
T = chr(9)

b = open(ROOT + P, 'rb').read()
cr = b.count(b'\r')
s = b.decode('utf-8')


def rep(old, new, n=1):
    global s
    c = s.count(old)
    assert c == n, (old[:80], c, n)
    s = s.replace(old, new)


# the card struct
rep(T + 'int oct = 0, octTileW = 0, octTileH = 0;\n',
    T + 'int oct = 0, octTileW = 0, octTileH = 0;\n'
    + T + '//! THE HORIZON RING (lane CARDFIX1 step 5, IMPOSTORRING1; bungo 2026-09-23: "22.5 degrees\n'
    + T + '//! per take"). > 0 = the sidecar said `ring V`: the sheet is V frames in ONE row, frame v at\n'
    + T + '//! azimuth 360*v/V and elevation 0 (docs/LODGEN_LODM_FORMAT.md 3a). `oct` then carries V too,\n'
    + T + '//! so every "this base has a sheet" test holds; whatever lays frames out reads octCols() and\n'
    + T + '//! octRows(), never `oct` squared.\n'
    + T + 'int ringViews = 0;\n'
    + T + 'int octCols() const { return ringViews > 0 ? ringViews : oct; }\n'
    + T + 'int octRows() const { return ringViews > 0 ? 1 : oct; }\n')

# the sidecar parse
rep('} else if ( line[0] == QLatin1String( "oct" ) && line.size() >= 11 ) {\n'
    + T * 4 + '// oct N tileW tileH halfW halfH cx cy cz depthspan family [base]\n'
    + T * 4 + 'card.octPbr = ( line[10] == QLatin1String( "pbr" ) );\n'
    + T * 4 + 'card.oct = line[1].toInt();\n',
    '} else if ( ( line[0] == QLatin1String( "oct" ) || line[0] == QLatin1String( "ring" ) )\n'
    + T * 4 + '&& line.size() >= 11 ) {\n'
    + T * 4 + '// oct N tileW tileH halfW halfH cx cy cz depthspan family [base] [conv]\n'
    + T * 4 + '// ring V tileW tileH ... the same fields; V frames in one row (CARDFIX1 step 5)\n'
    + T * 4 + 'card.octPbr = ( line[10] == QLatin1String( "pbr" ) );\n'
    + T * 4 + 'card.oct = line[1].toInt();\n'
    + T * 4 + 'card.ringViews = ( line[0] == QLatin1String( "ring" ) ) ? card.oct : 0;\n')

# the per-frame offsets onto the layout
rep(T * 2 + 'if ( card.oct >= 2 && !rawFrameOff.isEmpty() ) {\n'
    + T * 3 + 'card.octFrameOff.fill( 0.0f, 2 * card.oct * card.oct );\n',
    T * 2 + 'if ( card.oct >= 2 && !rawFrameOff.isEmpty() ) {\n'
    + T * 3 + 'const int cols = card.octCols(), rows = card.octRows();\n'
    + T * 3 + 'card.octFrameOff.fill( 0.0f, 2 * cols * rows );\n')
rep('if ( fi < 0 || fj < 0 || fi >= card.oct || fj >= card.oct )',
    'if ( fi < 0 || fj < 0 || fi >= cols || fj >= rows )')
rep('card.octFrameOff[2 * ( fj * card.oct + fi )] = rawFrameOff[k + 2];\n'
    + T * 4 + 'card.octFrameOff[2 * ( fj * card.oct + fi ) + 1] = rawFrameOff[k + 3];\n',
    'card.octFrameOff[2 * ( fj * cols + fi )] = rawFrameOff[k + 2];\n'
    + T * 4 + 'card.octFrameOff[2 * ( fj * cols + fi ) + 1] = rawFrameOff[k + 3];\n')

# the card .lodm: the layout key
rep(T * 5 + 'oc.insert( QStringLiteral( "oct" ), card.oct );\n',
    T * 5 + '/* THE LAYOUT. A grid card says `oct` N (N x N frames, hemi-octahedral);\n'
    + T * 5 + ' * a RING card says `views` V and `grid` [V,1] -- the aggregate\'s own keys\n'
    + T * 5 + ' * (docs/LODGEN_LODM_FORMAT.md 3a) -- and NO `oct`, so a reader that knows\n'
    + T * 5 + ' * only the grid finds no grid and refuses the set by that key\'s name\n'
    + T * 5 + ' * instead of reading sixteen frames as a 16 x 16 sheet. frameOffset is\n'
    + T * 5 + ' * then 2*V numbers, frame v at index v. */\n'
    + T * 5 + 'if ( card.ringViews > 0 ) {\n'
    + T * 6 + 'oc.insert( QStringLiteral( "views" ), card.ringViews );\n'
    + T * 6 + 'oc.insert( QStringLiteral( "grid" ), QJsonArray{ card.ringViews, 1 } );\n'
    + T * 5 + '} else {\n'
    + T * 6 + 'oc.insert( QStringLiteral( "oct" ), card.oct );\n'
    + T * 5 + '}\n')

# the aggregate intake refuses a ring set by name
rep(T + 'int noSet = 0, notOrtho = 0, noOct = 0;\n',
    T + 'int noSet = 0, notOrtho = 0, noOct = 0, ringSet = 0;\n')
rep(T * 2 + 'if ( !c.valid || c.oct <= 1 ) {\n' + T * 3 + 'noSet++;\n' + T * 3 + 'return;\n' + T * 2 + '}\n',
    T * 2 + 'if ( !c.valid || c.oct <= 1 ) {\n' + T * 3 + 'noSet++;\n' + T * 3 + 'return;\n' + T * 2 + '}\n'
    + T * 2 + '/* A HORIZON-RING set (CARDFIX1 step 5). The aggregate composites from N x N\n'
    + T * 2 + ' * grid frames (lodgenaggregate.cpp, cardFrameDir); reading V ring frames as a\n'
    + T * 2 + ' * V x V grid would composite the wrong views. Refused and counted by name;\n'
    + T * 2 + ' * teaching the aggregate the ring is owed (not this lane\'s file). */\n'
    + T * 2 + 'if ( c.ringViews > 0 ) {\n' + T * 3 + 'ringSet++;\n' + T * 3 + 'return;\n' + T * 2 + '}\n')
rep('.arg( out.size() ).arg( noSet ).arg( cardDir ).arg( notOrtho ).arg( noOct );\n',
    '.arg( out.size() ).arg( noSet ).arg( cardDir ).arg( notOrtho ).arg( noOct );\n'
    + T * 2 + 'if ( ringSet > 0 )\n'
    + T * 3 + '*notes << QString( "aggregate cards: refused %1 horizon-ring set(s) by name -- the aggregate"\n'
    + T * 4 + '" composites N x N grid frames only" ).arg( ringSet );\n')

# the card arrays
rep('struct Group { bool pbr = false; int w = 0,', 'struct Group { bool pbr = false, ring = false; int w = 0,')
rep(T * 2 + 'const int oct = card.value( QStringLiteral( "oct" ) ).toInt();\n',
    T * 2 + '/* A RING card (CARDFIX1 step 5) says `views` V and `grid` [V,1], never `oct`:\n'
    + T * 2 + ' * V frames in one row. `oct` below is then V, and the sheet height is one frame. */\n'
    + T * 2 + 'const int ringViews = card.value( QStringLiteral( "views" ) ).toInt();\n'
    + T * 2 + 'const int oct = ringViews > 0 ? ringViews : card.value( QStringLiteral( "oct" ) ).toInt();\n'
    + T * 2 + 'const int octRows = ringViews > 0 ? 1 : oct;\n')
rep('|| alb.width() != oct * fw || alb.height() != oct * fh ) {',
    '|| alb.width() != oct * fw || alb.height() != octRows * fh ) {')
rep('const QString key = QString( "%1|%2x%3" ).arg( lm.pbr ? QStringLiteral( "pbr" ) : QStringLiteral( "legacy" ) ).arg( alb.width() ).arg( alb.height() );\n',
    'const QString key = QString( "%1|%2x%3" ).arg( lm.pbr ? QStringLiteral( "pbr" ) : QStringLiteral( "legacy" ) ).arg( alb.width() ).arg( alb.height() )\n'
    + T * 3 + '+ ( ringViews > 0 ? QStringLiteral( "|ring" ) : QString() );\n')
rep(T * 2 + 'g.oct = oct;\n', T * 2 + 'g.oct = oct;\n' + T * 2 + 'g.ring = ringViews > 0;\n')
rep(T * 2 + 'arr.insert( QStringLiteral( "oct" ), g.oct );\n',
    T * 2 + 'if ( g.ring ) {\n'
    + T * 3 + '// a ring array: the aggregate\'s layout keys, and no `oct` (CARDFIX1 step 5)\n'
    + T * 3 + 'arr.insert( QStringLiteral( "views" ), g.oct );\n'
    + T * 3 + 'arr.insert( QStringLiteral( "grid" ), QJsonArray{ g.oct, 1 } );\n'
    + T * 2 + '} else {\n'
    + T * 3 + 'arr.insert( QStringLiteral( "oct" ), g.oct );\n'
    + T * 2 + '}\n')

out = s.encode('utf-8')
assert out.count(b'\r') == cr
open(ROOT + P, 'wb').write(out)
print('patched', P)
