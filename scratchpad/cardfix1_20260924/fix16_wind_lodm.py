# CARDFIX1 step 6 (IMPOSTORWIND1 job 3, sway A), part 3: the .lodm says where a card's sway came from.
# REPORT.md s3: `lodm: 2` only on card / card-array files containing at least one `sway: "model"` set;
# all-synthetic files stay v1 byte for byte; a SOURCE claiming 2 is refused by name. New keys: `sway`,
# `leafAmplitude`, `leafFrequency` (the base's STAT DNAM / TREE CNAM, already read into EsmLodBase;
# both absent = xEdit's default 1). On an array: lists parallel to `layers`.
# The aggregate (src/lodgenaggregate.cpp) is not this lane's file and still writes v1: owed, named.
# Both files LF-only.
ROOT = 'E:/Projects/NifskopeWWE-cardfix1/src/'
T = chr(9)


def patch(name, edits):
    b = open(ROOT + name, 'rb').read()
    assert b.count(b'\r') == 0, name
    s = b.decode('utf-8')
    for old, new in edits:
        c = s.count(old)
        assert c == 1, (name, old[:70], c)
        s = s.replace(old, new)
    out = s.encode('utf-8')
    assert out.count(b'\r') == 0
    open(ROOT + name, 'wb').write(out)
    print('patched', name)


patch('lodgen.cpp', [
    # the struct
    (T + 'float octEmissiveScale = 1.0f;\n',
     T + 'float octEmissiveScale = 1.0f;\n'
     + T + '//! THE SWAY SOURCE (IMPOSTORWIND1 sway A, CARDFIX1 step 6): the sidecar\'s `sway model`\n'
     + T + '//! line -- _n.A is the tree\'s own wind weight x height -- and the base\'s leaf amplitude\n'
     + T + '//! and frequency for it. `sway synthetic` or no line: the synthetic law, and the .lodm\n'
     + T + '//! stays version 1 byte for byte.\n'
     + T + 'bool swayModel = false;\n'
     + T + 'float leafAmplitude = 1.0f, leafFrequency = 1.0f;\n'),
    # the signature
    ('const LodgenCard & lodgenCard( const QString & dir, quint32 formID,\n'
     + T + 'QHash<quint32, LodgenCard> & cache, int auxDiv )\n{\n',
     'const LodgenCard & lodgenCard( const QString & dir, quint32 formID,\n'
     + T + 'QHash<quint32, LodgenCard> & cache, int auxDiv, const EsmLodBase * base = nullptr )\n{\n'),
    # the meta line
    (T * 3 + '} else if ( line[0] == QLatin1String( "emissive" ) && line.size() >= 2 ) {\n',
     T * 3 + '} else if ( line[0] == QLatin1String( "sway" ) && line.size() >= 2 ) {\n'
     + T * 4 + '// where _n.A came from: the model\'s own wind weight, or the synthetic law\n'
     + T * 4 + 'card.swayModel = ( line[1] == QLatin1String( "model" ) );\n'
     + T * 3 + '} else if ( line[0] == QLatin1String( "emissive" ) && line.size() >= 2 ) {\n'),
    # the card .lodm
    (T * 5 + 'root.insert( QStringLiteral( "lodm" ), 1 );\n'
     + T * 5 + 'root.insert( QStringLiteral( "family" ), card.octPbr ? QStringLiteral( "pbr" ) : QStringLiteral( "legacy" ) );\n'
     + T * 5 + 'root.insert( QStringLiteral( "kind" ), QStringLiteral( "card" ) );\n',
     T * 5 + '/* VERSION 2 ONLY WHERE THE SWAY IS THE MODEL\'S (IMPOSTORWIND1 sway A):\n'
     + T * 5 + ' * `sway` "model" plus the base\'s leaf amplitude and frequency. A\n'
     + T * 5 + ' * synthetic set writes none of it and stays version 1, byte for byte. */\n'
     + T * 5 + 'root.insert( QStringLiteral( "lodm" ), card.swayModel ? 2 : 1 );\n'
     + T * 5 + 'root.insert( QStringLiteral( "family" ), card.octPbr ? QStringLiteral( "pbr" ) : QStringLiteral( "legacy" ) );\n'
     + T * 5 + 'root.insert( QStringLiteral( "kind" ), QStringLiteral( "card" ) );\n'
     + T * 5 + 'if ( card.swayModel ) {\n'
     + T * 6 + 'root.insert( QStringLiteral( "sway" ), QStringLiteral( "model" ) );\n'
     + T * 6 + 'root.insert( QStringLiteral( "leafAmplitude" ), double( card.leafAmplitude ) );\n'
     + T * 6 + 'root.insert( QStringLiteral( "leafFrequency" ), double( card.leafFrequency ) );\n'
     + T * 5 + '}\n'),
    # the aggregate's collection and the chunk builder pass the base
    (T * 2 + 'const LodgenCard & c = lodgenCard( cardDir, baseId, cache, auxDiv );\n',
     T * 2 + 'const LodgenCard & c = lodgenCard( cardDir, baseId, cache, auxDiv, &b );\n'),
    (T * 3 + 'const LodgenCard & card = lodgenCard( opts.impostorDir, r.base, cardCache, opts.cardAuxDiv );\n',
     T * 3 + 'const LodgenCard & card = lodgenCard( opts.impostorDir, r.base, cardCache, opts.cardAuxDiv, &base );\n'),
    # the card array: per layer
    (T + 'struct Layer { QString id, lodmGame, source, projection, conv; float halfW = 0, halfH = 0, span = 0, emissiveScale = 1.0f; Vector3 center; QJsonArray frameOff; QJsonObject coverage; };\n',
     T + 'struct Layer { QString id, lodmGame, source, projection, conv; float halfW = 0, halfH = 0, span = 0, emissiveScale = 1.0f; Vector3 center; QJsonArray frameOff; QJsonObject coverage;\n'
     + T * 2 + 'bool swayModel = false; double leafAmplitude = 1.0, leafFrequency = 1.0; };\n'),
    (T * 2 + 'l.emissiveScale = lm.emissiveScale;\n',
     T * 2 + 'l.emissiveScale = lm.emissiveScale;\n'
     + T * 2 + '// and where its sway came from (IMPOSTORWIND1 sway A), with the base\'s leaf numbers\n'
     + T * 2 + 'l.swayModel = lm.root.value( QStringLiteral( "sway" ) ).toString() == QLatin1String( "model" );\n'
     + T * 2 + 'l.leafAmplitude = lm.root.value( QStringLiteral( "leafAmplitude" ) ).toDouble( 1.0 );\n'
     + T * 2 + 'l.leafFrequency = lm.root.value( QStringLiteral( "leafFrequency" ) ).toDouble( 1.0 );\n'),
    (T * 2 + 'root.insert( QStringLiteral( "lodm" ), 1 );\n'
     + T * 2 + 'root.insert( QStringLiteral( "family" ), g.pbr ? QStringLiteral( "pbr" ) : QStringLiteral( "legacy" ) );\n'
     + T * 2 + 'root.insert( QStringLiteral( "kind" ), QStringLiteral( "cardArray" ) );\n',
     T * 2 + '// version 2 only when a layer\'s sway is its model\'s (IMPOSTORWIND1 sway A)\n'
     + T * 2 + 'bool anySwayModel = false;\n'
     + T * 2 + 'for ( const Layer & L : g.layers )\n'
     + T * 3 + 'anySwayModel = anySwayModel || L.swayModel;\n'
     + T * 2 + 'root.insert( QStringLiteral( "lodm" ), anySwayModel ? 2 : 1 );\n'
     + T * 2 + 'root.insert( QStringLiteral( "family" ), g.pbr ? QStringLiteral( "pbr" ) : QStringLiteral( "legacy" ) );\n'
     + T * 2 + 'root.insert( QStringLiteral( "kind" ), QStringLiteral( "cardArray" ) );\n'),
    (T * 2 + '// one multiple per layer, parallel to `layers`, as the mesh arrays have it\n'
     + T * 2 + 'arr.insert( QStringLiteral( "emissiveScale" ), scales );\n',
     T * 2 + '// one multiple per layer, parallel to `layers`, as the mesh arrays have it\n'
     + T * 2 + 'arr.insert( QStringLiteral( "emissiveScale" ), scales );\n'
     + T * 2 + '// the sway source and leaf numbers, parallel to `layers` too -- version 2 only\n'
     + T * 2 + 'if ( anySwayModel ) {\n'
     + T * 3 + 'QJsonArray sw, amp, frq;\n'
     + T * 3 + 'for ( const Layer & L : g.layers ) {\n'
     + T * 4 + 'sw.append( L.swayModel ? QStringLiteral( "model" ) : QStringLiteral( "synthetic" ) );\n'
     + T * 4 + 'amp.append( L.leafAmplitude );\n'
     + T * 4 + 'frq.append( L.leafFrequency );\n'
     + T * 3 + '}\n'
     + T * 3 + 'arr.insert( QStringLiteral( "sway" ), sw );\n'
     + T * 3 + 'arr.insert( QStringLiteral( "leafAmplitude" ), amp );\n'
     + T * 3 + 'arr.insert( QStringLiteral( "leafFrequency" ), frq );\n'
     + T * 2 + '}\n'),
])

# the base's leaf numbers, taken on the cache miss (both 0 = no DNAM / CNAM = xEdit's default 1)
b = open(ROOT + 'lodgen.cpp', 'rb').read().decode('utf-8')
old = (T + 'LodgenCard card;\n'
       + T + 'const QString id = QString( "%1" ).arg( formID, 8, 16, QChar( \'0\' ) );\n'
       + T + 'const QString metaPath = dir + "/" + id + QStringLiteral( ".txt" );\n')
assert b.count(old) == 1, b.count(old)
new = (T + 'LodgenCard card;\n'
       + T + 'if ( base && ( base->leafAmplitude != 0.0f || base->leafFrequency != 0.0f ) ) {\n'
       + T * 2 + 'card.leafAmplitude = base->leafAmplitude;\n'
       + T * 2 + 'card.leafFrequency = base->leafFrequency;\n'
       + T + '}\n'
       + T + 'const QString id = QString( "%1" ).arg( formID, 8, 16, QChar( \'0\' ) );\n'
       + T + 'const QString metaPath = dir + "/" + id + QStringLiteral( ".txt" );\n')
out = b.replace(old, new).encode('utf-8')
assert out.count(b'\r') == 0
open(ROOT + 'lodgen.cpp', 'wb').write(out)
print('patched lodgen.cpp (leaf numbers)')

patch('io/lodmfile.cpp', [
    (T + 'if ( m.root.value( QStringLiteral( "lodm" ) ).toInt( 0 ) != int( LODM_VERSION ) ) {\n'
     + T * 2 + 'm.error = QStringLiteral( "payload is not a lodm 1 object" );\n'
     + T * 2 + 'return m;\n'
     + T + '}\n',
     T + '/* PAYLOAD VERSION 2 is the CARD family\'s (IMPOSTORWIND1 sway A, CARDFIX1 step 6):\n'
     + T + ' * a card, card array or aggregate whose _n.A is a model\'s own wind weight says\n'
     + T + ' * `lodm` 2 and `sway` "model". Nothing else may claim it: a source (a material)\n'
     + T + ' * is version 1 and is refused BY NAME if it says 2. */\n'
     + T + '{\n'
     + T * 2 + 'const int pv = m.root.value( QStringLiteral( "lodm" ) ).toInt( 0 );\n'
     + T * 2 + 'const QString k = m.root.value( QStringLiteral( "kind" ) ).toString( QStringLiteral( "source" ) );\n'
     + T * 2 + 'const bool cardFamily = k == QLatin1String( "card" ) || k == QLatin1String( "cardArray" )\n'
     + T * 3 + '|| k == QLatin1String( "aggregate" );\n'
     + T * 2 + 'if ( pv == 2 && !cardFamily ) {\n'
     + T * 3 + 'm.error = QStringLiteral( "lodm 2 is the card family\'s version (card, cardArray, aggregate: the "\n'
     + T * 4 + '"model sway); a \\"%1\\" .lodm is lodm 1" ).arg( k );\n'
     + T * 3 + 'return m;\n'
     + T * 2 + '}\n'
     + T * 2 + 'if ( pv != int( LODM_VERSION ) && pv != 2 ) {\n'
     + T * 3 + 'm.error = QStringLiteral( "payload is not a lodm 1 object (nor a card family lodm 2)" );\n'
     + T * 3 + 'return m;\n'
     + T * 2 + '}\n'
     + T + '}\n'),
])
