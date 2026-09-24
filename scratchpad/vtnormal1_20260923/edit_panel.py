import sys
P = 'E:/Projects/NifskopeWildWastelandEdition/src/lodgenmanager.cpp'
U = 'E:/Projects/NifskopeWildWastelandEdition/src/nifskope_ui.cpp'
s = open(P, 'rb').read().decode('utf-8')


def sub(old, new, count=1):
	global s
	n = s.count(old)
	if n != count:
		sys.exit('anchor found %d times, want %d:\n%s' % (n, count, old[:300]))
	s = s.replace(old, new)


sub("""			vtFinestBox->addItem( tr( "2 cells per tile (32 units a texel)" ), 2 );
			vtFinestBox->addItem( tr( "1 cell per tile (16 units a texel, full)" ), 1 );
			vtFinestBox->setCurrentIndex(
				settings.value( QStringLiteral( "LodGeneration/vtFinest" ), 2 ).toInt() == 1 ? 1 : 0 );
			vtFinestBox->setToolTip( tr( "The densest level the pyramid carries. Two cells a tile is exactly\\n"
				"the density of vanilla's finest terrain ring; one cell is twice that\\n"
				"and four times the files." ) );
			wwMatchFieldStyle( vtFinestBox );
			vtFinestLabel = f.add( page, tr( "Finest level" ), vtFinestBox );""",
"""			/* THE FINEST TEXEL DENSITY, ONE ROW (lane VTNORMAL1, bungo's ruling
			 * 2026-09-23 09:4x: three values, 16 the default). The item data is
			 * world units a texel; vtOptions() turns it into the finest level
			 * and the tile content, the same pairs `--vt-density` names. The
			 * old "Tile content" row is gone: it is decided here now. */
			vtFinestBox->addItem( tr( "32 units a texel" ), 32 );
			vtFinestBox->addItem( tr( "16 units a texel" ), 16 );
			vtFinestBox->addItem( tr( "8 units a texel" ), 8 );
			{
				const int d = settings.value( QStringLiteral( "LodGeneration/vtDensity" ), 16 ).toInt();
				vtFinestBox->setCurrentIndex( d == 32 ? 0 : ( d == 8 ? 2 : 1 ) );
			}
			vtFinestBox->setToolTip( tr( "How fine the pyramid's densest level is, in world units a texel.\\n"
				"32 is vanilla's finest terrain ring (about 1.7 GB for the Commonwealth),\\n"
				"16 is twice that on each side (about 6.4 GB), 8 is the upscaled normal\\n"
				"sheets' own density (about 26 GB).\\n"
				"Command line: --vt-density" ) );
			wwMatchFieldStyle( vtFinestBox );
			vtFinestLabel = f.add( page, tr( "Finest texel size" ), vtFinestBox );""")
sub("""			xI( f, "LodgenVtContentSpin", QStringLiteral( "vtContent" ),
				tr( "Tile content" ), 256, 32, 1024,
				tr( "The texel size of one tile's own picture, before its border.\\n"
					"Command line: --vt-content" ) );
""", "")
sub("""			xB( f, "LodgenVtCoverInColorCheck", QStringLiteral( "vtCoverInColor" ),
				tr( "Ground cover in the colour layer" ), false,
				tr( "The ground cover is tinted into the colour tiles instead of being left in\\n"
					"the mask layer for the consumer to apply.\\n"
					"Command line: --vt-cover-in-color / --vt-cover-in-mask" ) );""",
"""			xB( f, "LodgenVtCoverInColorCheck", QStringLiteral( "vtCoverInColor" ),
				tr( "Ground cover in the colour layer" ), false,
				tr( "The ground cover is tinted into the colour tiles instead of being left in\\n"
					"the mask layer for the consumer to apply.\\n"
					"Command line: --vt-cover-in-color / --vt-cover-in-mask" ) );
			xB( f, "LodgenVtHalfAuxCheck", QStringLiteral( "vtHalfAux" ),
				tr( "Half-resolution normal, mask, height and emissive tiles" ), false,
				tr( "Keeps the colour at the finest texel size and halves each side of the\\n"
					"other layers, which is most of the pyramid's size.\\n"
					"Command line: --vt-half-aux" ) );""")
sub("""			for ( const char * k : { "vtContent", "vtBorder", "vtMips", "vtCompress",
					"vtHeight", "vtCoverInColor" } ) {""", """			for ( const char * k : { "vtBorder", "vtMips", "vtCompress",
					"vtHeight", "vtCoverInColor", "vtHalfAux" } ) {""")
sub("""		o.finestDim = vtFinestBox->currentData().toInt() == 1 ? 1 : 2;
		o.content = xi( "vtContent" );""", """		// the density row names a finest-level / content pair (--vt-density)
		const int density = vtFinestBox->currentData().toInt();
		o.finestDim = density == 8 ? 1 : 2;
		o.content = density == 32 ? 256 : 512;
		o.halfAux = xb( "vtHalfAux" );""")
sub("""		s.setValue( QStringLiteral( "LodGeneration/vtFinest" ), vtFinestBox->currentData().toInt() );""",
"""		s.setValue( QStringLiteral( "LodGeneration/vtDensity" ), vtFinestBox->currentData().toInt() );""")
sub("""			xP( f, "LodgenMsnCacheEdit", QStringLiteral( "msnCache" ),
				tr( "Normal cache folder" ), QString(),
				tr( "A folder the decoded vanilla normal tiles are kept in between runs, so the\\n"
					"next bake does not decode them again. Empty means no cache.\\n"
					"Command line: --msn-cache" ), true );""",
"""			/* The tooltip described a decode cache this folder never was (lane
			 * VTNORMAL1): it names the cleaned or upscaled normal sheets, and
			 * since 2026-09-23 they are the pyramid's normal as well as the
			 * chunk sheets'. The row is remembered like every other one. */
			xP( f, "LodgenMsnCacheEdit", QStringLiteral( "msnCache" ),
				tr( "Normal sheets folder" ), QString(),
				tr( "A folder of cleaned or upscaled terrain normal sheets, one\\n"
					"<world>.4.<x>.<y>_msn.DDS per chunk. When set, they are the chunk\\n"
					"sheets' normal and the terrain pyramid's, reduced to its texel size;\\n"
					"a chunk with no sheet keeps the normal from the heights.\\n"
					"Empty means none.\\n"
					"Command line: --msn-cache" ), true );""")
open(P, 'wb').write(s.encode('utf-8'))

u = open(U, 'rb').read().decode('utf-8')
old = """							{ "LodgenVtContentSpin", "vtContent", nullptr,
								"the terrain virtual texture module is off in this gate module set -- the pyramid is worldspace-wide, not one chunk; tests/spells/lodgen_terrain_vt.sh reads the pyramid rows", nullptr, nullptr, nullptr, nullptr, "256" },
"""
assert u.count(old) == 1
u = u.replace(old, "")
old2 = """							{ "LodgenVtCoverInColorCheck", "vtCoverInColor", nullptr,
								"the terrain virtual texture module is off in this gate module set -- the pyramid is worldspace-wide, not one chunk; tests/spells/lodgen_terrain_vt.sh reads the pyramid rows", nullptr, nullptr, nullptr, nullptr, "0" },
"""
assert u.count(old2) == 1
u = u.replace(old2, old2 + """							{ "LodgenVtHalfAuxCheck", "vtHalfAux", nullptr,
								"the terrain virtual texture module is off in this gate module set -- the pyramid is worldspace-wide, not one chunk; tests/spells/lodgen_terrain_vt.sh reads the pyramid rows", nullptr, nullptr, nullptr, nullptr, "0" },
""")
old3 = """							vtFin->setCurrentIndex( 1 );		// one cell a tile"""
assert u.count(old3) == 1
u = u.replace(old3, """							vtFin->setCurrentIndex( 1 );		// 16 units a texel""")
open(U, 'wb').write(u.encode('utf-8'))
print('panel ok')
