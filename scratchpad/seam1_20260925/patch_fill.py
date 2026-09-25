import sys
R='E:/Projects/NifskopeWWE-seam1/'
def patch(path, pairs):
    s=open(R+path,newline='').read(); cr=s.count('\r')
    for old,new in pairs:
        n=s.count(old); assert n==1,(path,n,old[:80])
        s=s.replace(old,new)
    assert s.count('\r')==cr
    open(R+path,'w',newline='').write(s); print('ok',path)
blk=open(R+'scratchpad/seam1_20260925/fill_block.cpp',newline='').read()
blk=blk.replace('QString lodgenVtFillReport(','static QString lodgenVtFillReport(')
# north-anchored tile origin in the fit
old_t='''		const int tx = lodgenVtFloorTo( cx - west, dim ) + west, ty = lodgenVtFloorTo( cy - south, dim ) + south;'''
new_t='''		// the driver's own tile grid: west-anchored columns, NORTH-anchored rows
		const int tx = lodgenVtFloorTo( cx - west, dim ) + west;
		const int ty = north + 1 - dim - lodgenVtFloorTo( north - cy, dim );'''
assert blk.count(old_t)==1; blk=blk.replace(old_t,new_t)
patch('src/lodgen.h',[(
'''which is what a Fallout 76 port brings (128 samples a cell, not 32). */
	bool height = false;
''','''which is what a Fallout 76 port brings (128 samples a cell, not 32). */
	bool height = false;
	/*! THE VANILLA-COLOUR FILL (lane SEAM1, docs/LODGEN_TERRAIN_VT.md 2.6): blend
	 *  the ground no LAND record paints toward Bethesda's own LOD diffuse for
	 *  those cells, tone-matched on the overlap. Read at bake time only. OFF by
	 *  default; a bake without it is byte-identical to one before it existed. */
	bool vanillaFill = false;
''')])
patch('src/lodgen.cpp',[
('''
bool lodgenBakeTerrainVt( const EsmWorld & world, const QString & dataRoot,''',
'\n'+blk+'''bool lodgenBakeTerrainVt( const EsmWorld & world, const QString & dataRoot,'''),
('''	if ( opts.cover.cover )
		world.setGrassTintResolver( &lodgenGrassTintResolve, &bc );

	auto writeTile''','''	if ( opts.cover.cover )
		world.setGrassTintResolver( &lodgenGrassTintResolve, &bc );

	/* THE VANILLA-COLOUR FILL (2.6): the painted bitmap and the tone fit, once
	 * for the whole pyramid. The fit's own low-resolution bakes run on COPIES
	 * of the caches that count things, so the report's census lines are the
	 * same with the fill on or off. */
	LodgenVtFill fill;
	if ( opts.vanillaFill ) {
		fill.on = true;
		fill.ws = ws;
		lodgenVtFillPainted( world, fill, levels[0].west, levels[0].south, levels[0].east, levels[0].north );
		LodgenVtMaskCache fitMask = maskCache;
		LodgenVtLandCache fitLand;
		LodgenRoadCensus fitRoads;
		LodgenObjectAoCensus fitObj;
		lodgenVtFillFit( fill, bc, dataRoot, levels[0].west, levels[0].south, levels[0].east,
			levels[0].north, levels[0].dim,
			[&]( int x0, int y0, int dim, int c, int b, LodgenVtStage & st ) {
				return lodgenBakeVtTile( world, dataRoot, bc, opts.cover, fitLand, fitMask, false,
					x0, y0, dim, c, b, st, roadSet.get(), &fitRoads, objField.get(), &fitObj );
			} );
	}

	auto writeTile'''),
('''				roadSet.get(), &roadCensus, objField.get(), &objCensus ) )
				return fail( QString( "could not bake tile (%1,%2)" ).arg( tx ).arg( ty ) );
''','''				roadSet.get(), &roadCensus, objField.get(), &objCensus ) )
				return fail( QString( "could not bake tile (%1,%2)" ).arg( tx ).arg( ty ) );
			if ( fill.on )
				lodgenVtFillTile( fill, bc, dataRoot, cellX0, cellY0, levels[0].dim, content, border,
					row[size_t( tx )] );
'''),
('''		r << QString( "maskLayerRefs %1" ).arg( layerFormsSeen );
''','''		r << QString( "maskLayerRefs %1" ).arg( layerFormsSeen );
		if ( opts.vanillaFill )
			r << ( fill.on ? lodgenVtFillReport( fill )
				: QStringLiteral( "vanillaFill off: fewer than 2 overlap cells with a vanilla tile to fit on" ) );
'''),
])
patch('src/nifcli.cpp',[
('''		else if ( t == QLatin1String( "--vt-height" ) ) lgVt.height = true;
''','''		else if ( t == QLatin1String( "--vt-height" ) ) lgVt.height = true;
		else if ( t == QLatin1String( "--vt-fill-vanilla" ) ) lgVt.vanillaFill = true;
'''),
('''		  << "         [--vt-height]                    a fourth R16 height sheet per tile,\n"
''','''		  << "         [--vt-fill-vanilla]              blend the ground no LAND record paints\n"
		  << "                                          toward Bethesda's own terrain LOD colour\n"
		  << "                                          for those cells (read at bake time from\n"
		  << "                                          the load order, never shipped), tone-\n"
		  << "                                          matched where painted land meets it.\n"
		  << "                                          OFF by default.\n"
		  << "         [--vt-height]                    a fourth R16 height sheet per tile,\n"
'''),
])
patch('src/lodgenmanager.cpp',[
('''			xB( f, "LodgenVtCoverInColorCheck", QStringLiteral( "vtCoverInColor" ),''',
'''			xB( f, "LodgenVtFillVanillaCheck", QStringLiteral( "vtFillVanilla" ),
				tr( "Fill unpainted ground with vanilla's colour" ), false,
				tr( "Ground no landscape record paints is blended toward the game's own\n"
					"terrain LOD colour, matched in tone where painted ground meets it.\n"
					"Command line: --vt-fill-vanilla" ) );
			xB( f, "LodgenVtCoverInColorCheck", QStringLiteral( "vtCoverInColor" ),'''),
('''					"vtHeight", "vtCoverInColor", "vtHalfAux" } ) {''',
'''					"vtHeight", "vtFillVanilla", "vtCoverInColor", "vtHalfAux" } ) {'''),
('''		o.height = xb( "vtHeight" );
''','''		o.height = xb( "vtHeight" );
		o.vanillaFill = xb( "vtFillVanilla" );
'''),
])
