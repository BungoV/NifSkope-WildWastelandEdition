# The sheet the bake writes beside a .lodl is an `_msn`: a MODEL-SPACE normal
# map.  A shader block that does not say so has the renderer read it as a
# tangent-space map, which is why the lit terrain came back about 40 percent
# darker than the .BTR of the same cells (measured: mean luma 70.6 against
# 121.1, mean |dColour| 50.5).  The .BTR of the same bake carries
# Shader Flags 1 = 0x80401000 (Model_Space_Normals set, Specular clear) and
# Shader Flags 2 = 3 (ZBuffer_Write | LOD_Landscape); a sheet-lit tile takes the
# same three bits and nothing else.  The no-sheets arm is untouched, so the
# module-off byte identity of gate (a) cannot move.
P = 'E:/Projects/NifskopeWildWastelandEdition/src/btdterrain.cpp'
s = open(P, 'rb').read().decode('utf-8')
before = len(s)

old = '''			QString diffuse = s.diffuse;
			QString normal = QStringLiteral( "#FFFF8080" );
			if ( sheetIndex >= 0 && sheetIndex < int( s.tileDiffuse.size() )
				&& !s.tileDiffuse[size_t( sheetIndex )].isEmpty() ) {
				// the bake's own colour sheet and its _msn, by name, through
				// the resource stack -- not a second copy of the colours
				diffuse = s.tileDiffuse[size_t( sheetIndex )];
				if ( sheetIndex < int( s.tileNormal.size() )
					&& !s.tileNormal[size_t( sheetIndex )].isEmpty() )
					normal = s.tileNormal[size_t( sheetIndex )];
			}
			nif->set<QString>( nif->getIndex( iTexArray, 0 ), diffuse );
			nif->set<QString>( nif->getIndex( iTexArray, 1 ), normal );
'''
new = '''			QString diffuse = s.diffuse;
			QString normal = QStringLiteral( "#FFFF8080" );
			bool sheetNormal = false;
			if ( sheetIndex >= 0 && sheetIndex < int( s.tileDiffuse.size() )
				&& !s.tileDiffuse[size_t( sheetIndex )].isEmpty() ) {
				// the bake's own colour sheet and its _msn, by name, through
				// the resource stack -- not a second copy of the colours
				diffuse = s.tileDiffuse[size_t( sheetIndex )];
				if ( sheetIndex < int( s.tileNormal.size() )
					&& !s.tileNormal[size_t( sheetIndex )].isEmpty() ) {
					normal = s.tileNormal[size_t( sheetIndex )];
					sheetNormal = true;
				}
			}
			nif->set<QString>( nif->getIndex( iTexArray, 0 ), diffuse );
			nif->set<QString>( nif->getIndex( iTexArray, 1 ), normal );
			if ( sheetNormal ) {
				/* The sheet's normal map is an `_msn`, a MODEL-SPACE map. Read
				 * as a tangent-space one it lights the land far too dark (mean
				 * luma 70.6 against the .BTR's 121.1). The .BTR of the same
				 * bake says so with bit 12 of Shader Flags 1, with the specular
				 * bit clear (the two are documented as incompatible), and marks
				 * itself LOD landscape in Shader Flags 2. A sheet-lit tile
				 * takes those three bits and leaves the rest alone. */
				quint32 sf1 = nif->get<quint32>( iShader, "Shader Flags 1" );
				sf1 = ( sf1 | 0x1000u ) & ~0x1u;
				nif->set<quint32>( iShader, "Shader Flags 1", sf1 );
				const quint32 sf2 = nif->get<quint32>( iShader, "Shader Flags 2" );
				nif->set<quint32>( iShader, "Shader Flags 2", sf2 | 0x2u );
			}
'''
assert s.count(old) == 1, 'anchor %d' % s.count(old)
s = s.replace(old, new)
open(P, 'wb').write(s.encode('utf-8'))
print('btdterrain.cpp %d -> %d bytes' % (before, len(s)))
