"""Lane TERRLIVE1 rule paint: src/nifcli.cpp plumbing (sources are LF)."""
import sys

P = 'E:/Projects/NifskopeWWE-terrlive1/src/nifcli.cpp'
s = open(P, 'rb').read().decode('utf-8')
assert '\r\n' not in s
pairs = [
(r'''		else if ( t == QLatin1String( "--vt-fill-vanilla" ) ) lgVt.vanillaFill = true;
''', r'''		else if ( t == QLatin1String( "--vt-fill-vanilla" ) ) lgVt.vanillaFill = true;
		else if ( t == QLatin1String( "--outside-paint" ) ) {
			const QString v = next();
			if ( v == QLatin1String( "rule" ) )
				lgVt.outsideRule = true;
			else if ( v == QLatin1String( "vanilla" ) )
				lgVt.outsideRule = false;
			else {
				err() << "error: --outside-paint takes vanilla or rule, not \"" << v << "\"" << Qt::endl;
				err().flush();
				return 2;
			}
		}
'''),
(r'''		else if ( t == QLatin1String( "--decal-check" ) ) {''', r'''		/* Lane TERRLIVE1: read a rule paint map (`<ws>.lodr`) back -- magic,
		 * sizes, CRC and every id inside the palette (lodgenRuleRead). */
		else if ( t == QLatin1String( "--rule-check" ) ) {
			QString path = next();
			if ( QFileInfo( path ).isDir() ) {
				const QStringList f = QDir( path ).entryList( { QStringLiteral( "*.lodr" ) }, QDir::Files, QDir::Name );
				path = f.isEmpty() ? QString() : QDir( path ).filePath( f.first() );
			}
			LodgenRuleMap m;
			QString e;
			if ( path.isEmpty() || !lodgenRuleRead( path, m, &e ) ) {
				err() << "error: " << ( path.isEmpty() ? QStringLiteral( "no .lodr in that folder" ) : e ) << Qt::endl;
				err().flush();
				return 1;
			}
			qint64 set = 0, pairs = 0;
			QVector<qint64> use( m.palette.size(), 0 );
			for ( size_t s = 0; s < m.a.size(); s++ ) {
				if ( m.a[s] == 0xFF )
					continue;
				set++;
				use[m.a[s]]++;
				if ( m.b[s] != m.a[s] && m.w[s] != 255 ) {
					pairs++;
					use[m.b[s]]++;
				}
			}
			int unused = 0;
			for ( qint64 u : use )
				unused += u == 0 ? 1 : 0;
			out() << "rule-check " << path << ": OK, cells " << m.cellMinX << "," << m.cellMinY << " + "
				  << m.cellsX << "x" << m.cellsY << ", grid " << m.nx() << "x" << m.ny() << " at "
				  << m.spacing() << " u, samples set " << set << " (pairs " << pairs << "), palette "
				  << m.palette.size() << " (unused " << unused << "), band " << m.band << " u, "
				  << QFileInfo( path ).size() << " bytes" << Qt::endl;
			out().flush();
			return 0;
		}
		else if ( t == QLatin1String( "--decal-check" ) ) {'''),
(r'''		  << "  lodgen --decal-check <file.lodd|.lodg|dir>  read a decal pair back\n"
''', r'''		  << "  lodgen --decal-check <file.lodd|.lodg|dir>  read a decal pair back\n"
		  << "  lodgen --rule-check <file.lodr|dir>   read a rule paint map back\n"
'''),
(r'''		  << "         [--vt-fill-vanilla]              blend the ground no LAND record paints\n"
''', r'''		  << "         [--outside-paint vanilla|rule]   the ground outside our painted area:\n"
		  << "                                          vanilla's LOD colour (default) or the\n"
		  << "                                          worldspace's own landscape textures by\n"
		  << "                                          slope, height and colour (<ws>.lodr);\n"
		  << "                                          rule needs --vt-fill-vanilla\n"
		  << "         [--vt-fill-vanilla]              blend the ground no LAND record paints\n"
'''),
(r'''		censusOut( dReport );
		if ( gLgTerrainOption == LodgenTerrainOption::Dynamic ) {
			censusOut( QStringLiteral( "vt: none -- terrain option dynamic writes no pyramid" ) );
			return 0;
		}
''', r'''		censusOut( dReport );
		if ( vo.outsideRule && !vo.vanillaFill ) {
			err() << "error: --outside-paint rule needs --vt-fill-vanilla" << Qt::endl;
			return 2;
		}
		if ( gLgTerrainOption == LodgenTerrainOption::Dynamic ) {
			censusOut( QStringLiteral( "vt: none -- terrain option dynamic writes no pyramid" ) );
			if ( vo.outsideRule ) {
				QStringList rlog;
				if ( !lodgenBakeOutsideRule( vtWorld, vRoot, vtDir, vo, &rlog, &verr ) ) {
					err() << "error: outside rule: " << verr << Qt::endl;
					return 1;
				}
				for ( const QString & l : rlog )
					censusOut( l );
			}
			return 0;
		}
'''),
(r'''		if ( !vtDir.isEmpty() )
			lodbSetTerrainOption( lodgenTerrainOptionName( gLgTerrainOption ) );
''', r'''		if ( !vtDir.isEmpty() ) {
			lodbSetTerrainOption( lodgenTerrainOptionName( gLgTerrainOption ) );
			lodbSetOutsideRule( vtOpts.outsideRule );
		}
		if ( !vtDir.isEmpty() && vtOpts.outsideRule && !vtOpts.vanillaFill ) {
			err() << "error: --outside-paint rule needs --vt-fill-vanilla" << Qt::endl;
			return 2;
		}
'''),
(r'''			bool vtOk = gLgTerrainOption == LodgenTerrainOption::Dynamic;
			if ( vtOk )
				censusOut( QStringLiteral( "vt: none -- terrain option dynamic writes no pyramid" ) );
			else {''', r'''			bool vtOk = gLgTerrainOption == LodgenTerrainOption::Dynamic;
			if ( vtOk ) {
				censusOut( QStringLiteral( "vt: none -- terrain option dynamic writes no pyramid" ) );
				if ( vo.outsideRule ) {
					QStringList rlog;
					vtOk = lodgenBakeOutsideRule( world,
						dataRoot.isEmpty() ? QStringLiteral( "E:/Tools/Fallout 4/DataUnpacked/Data" ) : dataRoot,
						vtDir, vo, &rlog, &vterr );
					for ( const QString & l : rlog )
						censusOut( l );
				}
			} else {'''),
]
for a, b in pairs:
    if s.count(a) != 1:
        sys.exit('count %d for %r' % (s.count(a), a[:70]))
    s = s.replace(a, b)
open(P, 'wb').write(s.encode('utf-8'))
print('patched')
