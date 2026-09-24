p = 'src/nativeemit.cpp'
s = open(p, encoding='utf-8', newline='').read()


def rep(o, n, c=1):
    global s
    assert s.count(o) == c, (s.count(o), o[:90])
    s = s.replace(o, n)


rep("""		/* v7: the path test, taken HERE because this is the only place the
		 * placement's own model string is in hand. Both separators, because a
		 * path that arrived from an ESM and a path that arrived from a loose
		 * file do not agree on which one Bethesda used. */
		{
			const QString m = p.model;
			const bool arch = m.startsWith( QLatin1String( "architecture\\\\" ), Qt::CaseInsensitive )
				|| m.startsWith( QLatin1String( "architecture/" ), Qt::CaseInsensitive );
			instArch.push_back( arch ? 1 : 0 );
			if ( arch )
				archPlacements++;
		}""",
    """		/* v7: the path test, taken HERE because this is the only place the
		 * placement's own model string is in hand.
		 *
		 * A COMPONENT test and not the prefix test the brief names, because the
		 * prefix test catches NOTHING and that was measured before this line was
		 * written: `p.model` is the model the chunk builder DREW, which for a far
		 * placement is the authored LOD -- `LOD\\Architecture\\Airport\\...` -- so
		 * "starts with architecture\\" matched 0 of 2,449 placements on chunk
		 * 4.4.-12 while 2,026 of the .lodo's 5,956 model strings sit under an
		 * `Architecture` folder. Asking whether any path COMPONENT is
		 * `architecture` catches the LOD path and the source path alike. Both
		 * separators, because a path from an ESM and a path from a loose file do
		 * not agree on which one Bethesda used. */
		{
			bool arch = false;
			for ( const QStringView & comp : QStringView( p.model ).split( QRegularExpression( QStringLiteral( "[\\\\\\\\/]" ) ) ) )
				if ( comp.compare( QLatin1String( "architecture" ), Qt::CaseInsensitive ) == 0 ) {
					arch = true;
					break;
				}
			instArch.push_back( arch ? 1 : 0 );
			if ( arch )
				archPlacements++;
		}""")

open(p, 'w', encoding='utf-8', newline='').write(s)
print('arch path rule spliced')
