"""W4 part 2: the loader carries the colour channel and the two shader bits; the native loader hands
`rgba` to the library only for a shape with BOTH. The viewer applies it. Anchors asserted once."""
R = 'E:/Projects/NifskopeWWE-seam1/'


def patch(path, pairs):
    s = open(R + path, newline='', encoding='utf-8').read()
    cr = s.count('\r')
    for old, new in pairs:
        n = s.count(old)
        assert n == 1, (path, n, old[:90])
        s = s.replace(old, new)
    assert s.count('\r') == cr
    open(R + path, 'w', newline='', encoding='utf-8').write(s)
    print('ok', path)


patch('src/lodgen.cpp', [
('''	bool matAlphaBlend = false;
	quint8 matAlphaRef = 255;
};

//! Compose a block's transform''',
'''	bool matAlphaBlend = false;
	quint8 matAlphaRef = 255;
	/* `.lodo` v5 (lane SEAM1, W4): whether `col` came from the source's own
	 * colour channel (vertex descriptor bit 0x20) rather than the white fill, and
	 * the two shader bits the game reads it by -- SLSF2 Vertex_Colors (bit 5) and
	 * SLSF1 Vertex_Alpha (bit 3). Read-only facts: the `.BTO` bakes do not look
	 * at them, so their bytes cannot move. */
	bool colStream = false;
	bool vcFlag = false;
	bool vaFlag = false;
};

//! Compose a block's transform'''),
('''			const Transform xf = lodgenWorldTransform( &src, iShape );
			LodSrcShape s;
			QModelIndex iVD = src.getIndex( iShape, "Vertex Data" );''',
'''			const Transform xf = lodgenWorldTransform( &src, iShape );
			LodSrcShape s;
			s.colStream = hasColors;
			QModelIndex iVD = src.getIndex( iShape, "Vertex Data" );'''),
('''				s.ownEmit = ( src.get<quint32>( iShader, "Shader Flags 1" )
					& LOD_OWN_EMIT ) != 0;
				QModelIndex iTexSet''',
'''				s.ownEmit = ( src.get<quint32>( iShader, "Shader Flags 1" )
					& LOD_OWN_EMIT ) != 0;
				s.vcFlag = ( src.get<quint32>( iShader, "Shader Flags 2" ) & ( 1U << 5 ) ) != 0;
				s.vaFlag = ( src.get<quint32>( iShader, "Shader Flags 1" ) & ( 1U << 3 ) ) != 0;
				QModelIndex iTexSet'''),
('''		n.hasAlpha = s.hasAlpha; n.alphaThreshold = s.alphaThreshold;
		out->push_back( std::move( n ) );''',
'''		n.hasAlpha = s.hasAlpha; n.alphaThreshold = s.alphaThreshold;
		/* `.lodo` v5: the colour goes in ONLY where the game applies it -- a
		 * colour channel AND the Vertex_Colors flag (bungo's W4 ruling). RGB and A
		 * as the source stores them; A's meaning rides in `vertexAlpha`. */
		if ( s.colStream && s.vcFlag && s.col.size() == nv ) {
			n.geom.rgba.resize( size_t( nv ) * 4 );
			for ( int v = 0; v < nv; v++ ) {
				const Color4 & c = s.col[v];
				n.geom.rgba[size_t( v ) * 4 + 0] = quint8( qBound( 0, qRound( c.red() * 255.0f ), 255 ) );
				n.geom.rgba[size_t( v ) * 4 + 1] = quint8( qBound( 0, qRound( c.green() * 255.0f ), 255 ) );
				n.geom.rgba[size_t( v ) * 4 + 2] = quint8( qBound( 0, qRound( c.blue() * 255.0f ), 255 ) );
				n.geom.rgba[size_t( v ) * 4 + 3] = quint8( qBound( 0, qRound( c.alpha() * 255.0f ), 255 ) );
			}
			n.geom.vertexAlpha = s.vaFlag;
		}
		out->push_back( std::move( n ) );'''),
])

# the viewer: apply the stream in the render path, and only where the file says so
patch('src/lodinative.cpp', [
('''	float selfAo = 1.0f;
	float sway = 0.0f;
	//! index into LodoLibrary::vertices''',
'''	float selfAo = 1.0f;
	float sway = 0.0f;
	/*! `.lodo` v5: the library colour, 0..1, white when the mesh carries none.
	 *  `rgbaA` is applied as opacity only on a VERTEX_ALPHA mesh, as the game does. */
	float rgba[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
	//! index into LodoLibrary::vertices'''),
('''	//! WW_LODL_AO / WW_LODL_CHANNEL: write the vertices' `chan` as a vertex colour
	bool withColour = false;''',
'''	//! WW_LODL_AO / WW_LODL_CHANNEL: write the vertices' `chan` as a vertex colour
	bool withColour = false;
	/*! `.lodo` v5: the mesh carries a colour stream, so the shape is drawn with
	 *  vertex colours (the source's SLSF2 Vertex_Colors, which is why it has one)
	 *  and, with `vertexAlpha`, vertex alpha. The channel views keep precedence:
	 *  they write `chan`, and this multiplies into it. */
	bool libColour = false;
	bool vertexAlpha = false;'''),
('''			if ( b.withColour )
				nif->set<ByteColor4>( row, "Vertex Colors",
					ByteColor4( FloatVector4( o.chan[0], o.chan[1], o.chan[2], 1.0f ) ) );''',
'''			if ( b.withColour || b.libColour )
				nif->set<ByteColor4>( row, "Vertex Colors",
					ByteColor4( FloatVector4( o.chan[0] * o.rgba[0], o.chan[1] * o.rgba[1], o.chan[2] * o.rgba[2],
						b.vertexAlpha ? o.rgba[3] : 1.0f ) ) );'''),
('''		nif->set<quint32>( iShader, "Shader Flags 2", b.withColour ? 0x25U : 5U );''',
'''		nif->set<quint32>( iShader, "Shader Flags 2", ( b.withColour || b.libColour ) ? 0x25U : 5U );
		// `.lodo` v5: SLSF1 bit 3 Vertex_Alpha, only where the source shader set it
		if ( b.vertexAlpha )
			nif->set<quint32>( iShader, "Shader Flags 1", nif->get<quint32>( iShader, "Shader Flags 1" ) | 0x8U );'''),
('''				o.selfAo = float( lv.selfAO ) / 255.0f;
				o.sway = float( lv.sway ) / 255.0f;
				o.libIndex = quint32( vi );''',
'''				o.selfAo = float( lv.selfAO ) / 255.0f;
				o.sway = float( lv.sway ) / 255.0f;
				if ( ( mesh.flags & LODO_MESH_VERTEX_COLOUR ) && vi < lib.colours.size() )
					for ( int k = 0; k < 4; k++ )
						o.rgba[k] = float( ( lib.colours[vi] >> ( 8 * k ) ) & 0xFF ) / 255.0f;
				o.libIndex = quint32( vi );'''),
('''				Bucket & bk = bit.value();
''',
'''				Bucket & bk = bit.value();
				/* `.lodo` v5: the DRAWN mesh's colour stream, drawn in every view and
				 * multiplied into whatever the channel wrote (white by default). OR'd
				 * over the placements: a vertex with no colour is white, so it is safe. */
				if ( meshRow.flags & LODO_MESH_VERTEX_COLOUR )
					bk.libColour = true;
				if ( meshRow.flags & LODO_MESH_VERTEX_ALPHA )
					bk.vertexAlpha = true;
'''),
('''					o.uv2y = bk.layer >= 0 ? float( bk.layer ) : 0.0f;''',
'''					o.uv2y = bk.layer >= 0 ? float( bk.layer ) : 0.0f;
					for ( int k = 0; k < 4; k++ )
						o.rgba[k] = sv.rgba[k];'''),
])
