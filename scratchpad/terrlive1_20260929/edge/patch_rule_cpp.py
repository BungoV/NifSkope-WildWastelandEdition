"""Lane TERRLIVE1 rule paint: one-shot patch of src/lodgen.cpp (sources are LF)."""
import sys

P = 'E:/Projects/NifskopeWWE-terrlive1/src/lodgen.cpp'
s = open(P, 'rb').read().decode('utf-8')
assert '\r\n' not in s


def once(a, b):
    global s
    n = s.count(a)
    if n != 1:
        sys.exit('anchor count %d: %r' % (n, a[:80]))
    s = s.replace(a, b)


# A. the rule plane on the stage
once('''	std::vector<float> stampNormal;
	std::vector<float> stampW;
	bool cover = false;
};
''', '''	std::vector<float> stampNormal;
	std::vector<float> stampW;
	/*! THE RULE COLOUR (lane TERRLIVE1, `--outside-paint rule`): ARGB per
	 *  texel, A 0 = no rule sample here. Empty when the switch is off; the fill
	 *  reads it in place of vanilla's diffuse and the driver frees it. */
	std::vector<quint32> rule;
	bool cover = false;
};
''')

# B. the tile bake takes the rule map
once('''	const LodgenObjectHeightField * objField = nullptr,
	LodgenObjectAoCensus * objCensus = nullptr )
{
	const int S = content + 2 * border;
	const float upt = float( dim ) * 4096.0f / float( content );

	/* THE ROAD PLANE''', '''	const LodgenObjectHeightField * objField = nullptr,
	LodgenObjectAoCensus * objCensus = nullptr,
	const LodgenRuleMap * rule = nullptr,
	const std::function<bool( float, float )> * ruleNeed = nullptr )
{
	const int S = content + 2 * border;
	const float upt = float( dim ) * 4096.0f / float( content );
	if ( rule )
		out.rule.assign( size_t( S ) * S, 0u );

	/* THE ROAD PLANE''')

# C. move sampleLtex above the land branch (a pure move: a lambda definition runs nothing)
i0 = s.index('				auto sampleLtex = [&]( quint32 ltex, float * hOut, FloatVector4 * mOut,', s.index('static bool lodgenBakeVtTile('))
end_mark = '\t\t\t\t\treturn avg + ( fp - avg ) * kDetail;\n\t\t\t\t};\n'
i1 = s.index(end_mark, i0) + len(end_mark)
lam = s[i0:i1]
s = s[:i0] + s[i1:]
anchor = '''			float coverTint[3] = { 0.0f, 0.0f, 0.0f };
			if ( haveLand[ci] ) {
'''
assert s.count(anchor) == 1
lam2 = '\n'.join((l[1:] if l.startswith('\t') else l) for l in lam.split('\n'))
note = ('\t\t\t/* Defined here, above the land branch, since lane TERRLIVE1\'s rule paint\n'
        '\t\t\t * (2026-09-29) samples it outside the painted ground too; it was moved,\n'
        '\t\t\t * not changed. */\n')
s = s.replace(anchor, '\t\t\tfloat coverTint[3] = { 0.0f, 0.0f, 0.0f };\n' + note + lam2
              + '\t\t\tif ( haveLand[ci] ) {\n')

# D. the rule colour, after ours is written
once('''			out.colour[size_t( j ) * S + i] = 0xFF000000U
				| ( quint32( qBound( 0, int( color[0] * 255.0f + 0.5f ), 255 ) ) << 16 )
				| ( quint32( qBound( 0, int( color[1] * 255.0f + 0.5f ), 255 ) ) << 8 )
				| quint32( qBound( 0, int( color[2] * 255.0f + 0.5f ), 255 ) );
''', '''			out.colour[size_t( j ) * S + i] = 0xFF000000U
				| ( quint32( qBound( 0, int( color[0] * 255.0f + 0.5f ), 255 ) ) << 16 )
				| ( quint32( qBound( 0, int( color[1] * 255.0f + 0.5f ), 255 ) ) << 8 )
				| quint32( qBound( 0, int( color[2] * 255.0f + 0.5f ), 255 ) );
			/* THE RULE COLOUR (lane TERRLIVE1, `--outside-paint rule`), only where
			 * the fill will read it: the rule map's textures, mixed by its
			 * weights, each through the same land lookup as ours (warp, hex,
			 * mip bias, detail), then the same shading, grade and variation.
			 * No cover tint and no vertex colour: the ground outside has none. */
			if ( rule && ( !ruleNeed || ( *ruleNeed )( wx, wy ) ) ) {
				quint8 rIds[8];
				float rW[8];
				const int rn = rule->mixAt( wx, wy, rIds, rW );
				if ( rn > 0 ) {
					float rc[3] = { 0.0f, 0.0f, 0.0f };
					for ( int k = 0; k < rn; k++ ) {
						const FloatVector4 t = sampleLtex( rule->palette.at( rIds[k] ), nullptr, nullptr, nullptr );
						for ( int ch = 0; ch < 3; ch++ )
							rc[ch] += t[ch] * rW[k];
					}
					FloatVector4 rcol( rc[0], rc[1], rc[2], 1.0f );
					if ( eroField && g_landShade != 0.0f ) {
						const float dL = g_landShade * eroField->creviceAt( wx, wy, upt )
							* lodgenErosion() / 255.0f;
						for ( int k = 0; k < 3; k++ )
							rcol[k] = qBound( 0.0f, rcol[k] + dL, 1.0f );
					}
					if ( g_landGrade != 1.0f )
						for ( int k = 0; k < 3; k++ )
							rcol[k] *= g_landGrade;
					if ( lodgenLandMacro() )
						rcol = lodgenLandMacroApply( rcol, double( wx ), double( wy ) );
					out.rule[size_t( j ) * S + i] = 0xFF000000U
						| ( quint32( qBound( 0, int( rcol[0] * 255.0f + 0.5f ), 255 ) ) << 16 )
						| ( quint32( qBound( 0, int( rcol[1] * 255.0f + 0.5f ), 255 ) ) << 8 )
						| quint32( qBound( 0, int( rcol[2] * 255.0f + 0.5f ), 255 ) );
				}
			}
''')

# E. fill census field
once('''	qint64 paintedQuads = 0, texelsVanilla = 0;
''', '''	qint64 paintedQuads = 0, texelsVanilla = 0;
	qint64 texelsRule = 0;                      //!< lane TERRLIVE1: texels that blended toward the rule colour
''')

# F. the fill reads the rule colour first
once('''			float v[3];
			if ( !lodgenVtFillSample( rgb, have, gx0, gy0, ww, wh, wx, wy, v ) ) {
				F.texelsNoVanilla++;
				continue;
			}
			quint32 & px = out.colour[size_t( j ) * S + i];''', '''			float v[3];
			/* the rule paint (lane TERRLIVE1), when the tile carries it: it takes
			 * vanilla's place in the same blend; where it has no sample, vanilla */
			const quint32 rpx = out.rule.empty() ? 0u : out.rule[size_t( j ) * S + i];
			if ( rpx >> 24 ) {
				v[0] = float( ( rpx >> 16 ) & 0xFF ) / 255.0f;
				v[1] = float( ( rpx >> 8 ) & 0xFF ) / 255.0f;
				v[2] = float( rpx & 0xFF ) / 255.0f;
				F.texelsRule++;
			} else if ( !lodgenVtFillSample( rgb, have, gx0, gy0, ww, wh, wx, wy, v ) ) {
				F.texelsNoVanilla++;
				continue;
			}
			quint32 & px = out.colour[size_t( j ) * S + i];''')

# G. the rule map functions, after the fill report
RULE = open('E:/Projects/NifskopeWWE-terrlive1/scratchpad/terrlive1_20260929/edge/rule_impl.cpp.txt', 'rb').read().decode('utf-8')
once('''/* ===== THE TERRAIN OPTION AND THE GROUND DECALS (lane TERRLIVE1, 2026-09-29) =====''',
     RULE + '''/* ===== THE TERRAIN OPTION AND THE GROUND DECALS (lane TERRLIVE1, 2026-09-29) =====''')

# H. the driver: build + write the map after the fill's setup
once('''				return lodgenBakeVtTile( world, dataRoot, bc, fitCover, fitLand, fitMask, false,
					x0, y0, dim, c, b, st, roadSet.get(), &fitRoads, objField.get(), &fitObj );
			} );
	}
''', '''				return lodgenBakeVtTile( world, dataRoot, bc, fitCover, fitLand, fitMask, false,
					x0, y0, dim, c, b, st, roadSet.get(), &fitRoads, objField.get(), &fitObj );
			} );
	}

	/* THE RULE PAINT OUTSIDE (lane TERRLIVE1, `--outside-paint rule`): the
	 * id/weight map, written to its own new file, then read by every finest
	 * tile that the fill reaches. The coarser levels are filtered from those
	 * tiles, so Hybrid's far levels carry the rule paint with no extra bytes. */
	LodgenRuleMap ruleMap;
	bool ruleOn = false;
	QStringList ruleLog;
	std::function<bool( float, float )> ruleNeed;
	if ( opts.outsideRule ) {
		if ( !fill.on )
			return fail( QStringLiteral( "--outside-paint rule needs --vt-fill-vanilla (the rule matches vanilla's diffuse)" ) );
		QElapsedTimer rt;
		rt.start();
		QString rwhy;
		if ( !lodgenRuleBuild( world, dataRoot, bc, fill, levels[0].west, levels[0].south, levels[0].east,
				levels[0].north, ruleMap, &ruleLog, &rwhy ) )
			return fail( QStringLiteral( "outside rule: " ) + rwhy );
		const QString rpath = dir + QStringLiteral( "/" ) + ws + QStringLiteral( ".lodr" );
		const qint64 rbytes = lodgenRuleWrite( rpath, ruleMap, &rwhy );
		if ( rbytes < 0 )
			return fail( QStringLiteral( "outside rule: " ) + rwhy );
		ruleLog << QString( "outsideRule file=%1 bytes=%2 buildMs=%3" ).arg( QFileInfo( rpath ).fileName() )
			.arg( rbytes ).arg( rt.elapsed() );
		ruleOn = true;
		const int rq = int( std::ceil( fill.band / 2048.0f ) ) + 1;
		struct Memo { qint64 key = 0; bool valid = false; bool need = false; };
		auto memo = std::make_shared<Memo>();
		ruleNeed = [&fill, rq, memo]( float wx, float wy ) -> bool {
			const int qx = int( std::floor( wx / 2048.0f ) ), qy = int( std::floor( wy / 2048.0f ) );
			const qint64 k = LodgenVtFill::key( qx, qy );
			if ( memo->valid && k == memo->key )
				return memo->need;
			bool need = false;
			for ( int dy = -rq; dy <= rq && !need; dy++ )
				for ( int dx = -rq; dx <= rq && !need; dx++ )
					need = !fill.isPaintedQ( qx + dx, qy + dy );
			memo->key = k;
			memo->valid = true;
			memo->need = need;
			return need;
		};
	}
''')

once('''				wantEmissive, cellX0, cellY0, levels[0].dim, content, border, row[size_t( tx )],
				roadSet.get(), &roadCensus, objField.get(), &objCensus ) )
				return fail( QString( "could not bake tile (%1,%2)" ).arg( tx ).arg( ty ) );
			if ( fill.on )
				lodgenVtFillTile( fill, cellX0, cellY0, levels[0].dim, content, border,
					row[size_t( tx )] );
''', '''				wantEmissive, cellX0, cellY0, levels[0].dim, content, border, row[size_t( tx )],
				roadSet.get(), &roadCensus, objField.get(), &objCensus,
				ruleOn ? &ruleMap : nullptr, ruleOn ? &ruleNeed : nullptr ) )
				return fail( QString( "could not bake tile (%1,%2)" ).arg( tx ).arg( ty ) );
			if ( fill.on )
				lodgenVtFillTile( fill, cellX0, cellY0, levels[0].dim, content, border,
					row[size_t( tx )] );
			std::vector<quint32>().swap( row[size_t( tx )].rule );
''')

once('''			r << ( fill.on ? lodgenVtFillReport( fill )
				: QStringLiteral( "vanillaFill off: fewer than 2 overlap cells with a vanilla tile to fit on" ) );
''', '''			r << ( fill.on ? lodgenVtFillReport( fill )
				: QStringLiteral( "vanillaFill off: fewer than 2 overlap cells with a vanilla tile to fit on" ) );
		if ( ruleOn ) {
			for ( const QString & l : ruleLog )
				r << l;
			r << QString( "outsideRule texelsRule=%1" ).arg( fill.texelsRule );
		}
''')

open(P, 'wb').write(s.encode('utf-8'))
print('patched')
