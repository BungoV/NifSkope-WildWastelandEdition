p = 'src/lodifile.cpp'
s = open(p, encoding='utf-8', newline='').read()


def rep(old, new, n=1):
    global s
    assert s.count(old) == n, (s.count(old), old[:90])
    s = s.replace(old, new)


# ---- version acceptance + the crc window ----
rep("""	if ( h.version != LODI_VERSION && h.version != LODI_VERSION_AGGREGATE
		&& h.version != LODI_VERSION_PLACEMENT_AO && h.version != LODI_VERSION_VERTEX_AO )
		return refuse( QString( "version %1; this reader knows %2, %3, %4 and %5" )
			.arg( h.version ).arg( LODI_VERSION ).arg( LODI_VERSION_AGGREGATE )
			.arg( LODI_VERSION_PLACEMENT_AO ).arg( LODI_VERSION_VERTEX_AO ) );
	h.headerCrc32 = getLE<quint32>( p + H_HCRC );
	const quint32 hcrc = lodvCrc32( p + H_PLUGIN, LODI_HEADER_BYTES - H_PLUGIN );""",
    """	if ( h.version != LODI_VERSION && h.version != LODI_VERSION_AGGREGATE
		&& h.version != LODI_VERSION_PLACEMENT_AO && h.version != LODI_VERSION_VERTEX_AO
		&& h.version != LODI_VERSION_GROUP_SKY )
		return refuse( QString( "version %1; this reader knows %2, %3, %4, %5 and %6" )
			.arg( h.version ).arg( LODI_VERSION ).arg( LODI_VERSION_AGGREGATE )
			.arg( LODI_VERSION_PLACEMENT_AO ).arg( LODI_VERSION_VERTEX_AO )
			.arg( LODI_VERSION_GROUP_SKY ) );
	/* v7's header BLOCK is 512 bytes. On every older version this is still 256
	 * and the crc window below is the one it always was. */
	const quint32 headerBytes = lodiHeaderBytes( h.version );
	if ( file.size() < qsizetype( headerBytes ) )
		return refuse( QString( "%1 bytes, shorter than the %2-byte version-%3 header" )
			.arg( file.size() ).arg( headerBytes ).arg( h.version ) );
	h.headerCrc32 = getLE<quint32>( p + H_HCRC );
	const quint32 hcrc = lodvCrc32( p + H_PLUGIN, qsizetype( headerBytes ) - H_PLUGIN );""")

# ---- v6/v5 flags gain v7 ----
rep("""	// v6 is a superset of v5: every v5 rule below runs on it too
	const bool v6 = ( h.version == LODI_VERSION_VERTEX_AO );
	const bool v5 = ( h.version == LODI_VERSION_PLACEMENT_AO ) || v6;""",
    """	// each version is a superset of the one below: every rule runs on the ones above
	const bool v7 = ( h.version == LODI_VERSION_GROUP_SKY );
	const bool v6 = ( h.version == LODI_VERSION_VERTEX_AO ) || v7;
	const bool v5 = ( h.version == LODI_VERSION_PLACEMENT_AO ) || v6;""")

# ---- v7 header words, read inside the v6 branch ----
rep("""			if ( h.offVertexAo == 0 || h.vertexAoBytes < 4ull * ( quint64( h.instanceCount ) + 1 ) )
				return refuse( QString( "version 6 with no vertex-AO blob (offset %1, %2 bytes; the offsets alone take %3). "
					"The blob is what version 6 IS: a bake without it is written at version 5" )
					.arg( h.offVertexAo ).arg( h.vertexAoBytes ).arg( 4ull * ( quint64( h.instanceCount ) + 1 ) ) );
		}""",
    """			if ( h.offVertexAo == 0 || h.vertexAoBytes < 4ull * ( quint64( h.instanceCount ) + 1 ) )
				return refuse( QString( "version 6 with no vertex-AO blob (offset %1, %2 bytes; the offsets alone take %3). "
					"The blob is what version 6 IS: a bake without it is written at version 5" )
					.arg( h.offVertexAo ).arg( h.vertexAoBytes ).arg( 4ull * ( quint64( h.instanceCount ) + 1 ) ) );
		}
		if ( v7 ) {
			/* v7: the group table and the vertex-sky stream. Version 7 IS one of
			 * the two; a bake with neither is written at version 6, which is
			 * what makes `--lodi-v6` byte-identical. */
			h.offGroup = getLE<quint64>( p + H_OFF_GROUP );
			h.groupCount = getLE<quint32>( p + H_GROUPCOUNT );
			h.groupStride = getLE<quint16>( p + H_GROUPSTRIDE );
			h.offVertexSky = getLE<quint64>( p + H_OFF_VSKY );
			h.vertexSkyBytes = getLE<quint32>( p + H_VSKYBYTES );
			if ( h.offGroup == 0 && h.offVertexSky == 0 )
				return refuse( QStringLiteral( "version 7 carrying neither a group table (header 0x100) nor a "
					"vertex-sky stream (header 0x110). Version 7 IS one of the two; a bake with neither is "
					"written at version 6, which is what makes the way back byte-identical" ) );
			if ( h.offGroup ) {
				if ( h.groupStride != LODI_GROUP_STRIDE )
					return refuse( QString( "groupStride %1; this reader knows %2" )
						.arg( h.groupStride ).arg( LODI_GROUP_STRIDE ) );
				if ( h.groupCount == 0 && h.instanceCount != 0 )
					return refuse( QStringLiteral( "a group table is present but groupCount is 0; every placement "
						"is in some group, if only its own" ) );
			} else if ( h.groupCount != 0 || h.groupStride != 0 ) {
				return refuse( QString( "no group table (header 0x100 is 0) but groupCount %1 / groupStride %2 "
					"say otherwise" ).arg( h.groupCount ).arg( h.groupStride ) );
			}
			if ( h.offVertexSky ) {
				if ( h.vertexSkyBytes < 4ull * ( quint64( h.instanceCount ) + 1 ) )
					return refuse( QString( "vertex-sky stream of %1 bytes cannot hold its own %2 offset words" )
						.arg( h.vertexSkyBytes ).arg( quint64( h.instanceCount ) + 1 ) );
			} else if ( h.vertexSkyBytes != 0 ) {
				return refuse( QString( "no vertex-sky stream (header 0x110 is 0) but vertexSkyBytes is %1" )
					.arg( h.vertexSkyBytes ) );
			}
		} else if ( getLE<quint64>( p + H_OFF_GROUP ) != 0 || getLE<quint64>( p + H_OFF_VSKY ) != 0 ) {
			/* A v5 or v6 file whose header block ends at 0x100 cannot be
			 * carrying these words. Named, not left to the pad sweep, for the
			 * same reason the v5 words are (see just below). */
			return refuse( QString( "version %1 carrying version-7 header words (group table at 0x100 = %2, "
				"vertex-sky stream at 0x110 = %3). Versions 3 to 6 have a 256-byte header and end at 0x100" )
				.arg( h.version ).arg( getLE<quint64>( p + H_OFF_GROUP ) ).arg( getLE<quint64>( p + H_OFF_VSKY ) ) );
		}""")

# ---- the pad sweep ----
rep("""	for ( int i = padFrom; i < ( v6 ? H_RESERVED_F4 : int( LODI_HEADER_BYTES ) ); i++ )
		if ( p[i] != 0 )
			return refuse( QString( "reserved header byte at 0x%1 is not zero" ).arg( i, 2, 16, QChar( '0' ) ) );""",
    """	/* The v6 pad is 0xF1..0xF3 alone; on v7 the sweep continues over the second
	 * half of the block, 0x11C..0x1FF, which is the reserved room this lane did
	 * not spend. */
	for ( int i = padFrom; i < ( v6 ? H_RESERVED_F4 : int( LODI_HEADER_BYTES ) ); i++ )
		if ( p[i] != 0 )
			return refuse( QString( "reserved header byte at 0x%1 is not zero" ).arg( i, 2, 16, QChar( '0' ) ) );
	if ( v7 )
		for ( int i = H_RESERVED_11C; i < int( LODI_HEADER_BYTES_V7 ); i++ )
			if ( p[i] != 0 )
				return refuse( QString( "reserved header byte at 0x%1 is not zero" ).arg( i, 3, 16, QChar( '0' ) ) );""")

# ---- tabs ----
rep("""	int iAgg = -1, iPao = -1, iVao = -1;   //!< where the optional payloads landed in `tabs`, or -1""",
    """	int iAgg = -1, iPao = -1, iVao = -1, iGrp = -1, iVsky = -1;   //!< where the optional payloads landed in `tabs`, or -1""")

rep("""	if ( v6 ) {
		iVao = int( tabs.size() );
		tabs.push_back( { "vertex-AO blob", h.offVertexAo, quint64( h.vertexAoBytes ) } );
	}
	quint64 prevEnd = LODI_HEADER_BYTES;""",
    """	if ( v6 ) {
		iVao = int( tabs.size() );
		tabs.push_back( { "vertex-AO blob", h.offVertexAo, quint64( h.vertexAoBytes ) } );
	}
	// v7: the group table then the sky stream, in the order the writer laid them
	if ( v7 && h.offGroup ) {
		iGrp = int( tabs.size() );
		tabs.push_back( { "group table", h.offGroup, quint64( h.instanceCount ) * sizeof( quint16 ) } );
	}
	if ( v7 && h.offVertexSky ) {
		iVsky = int( tabs.size() );
		tabs.push_back( { "vertex-sky stream", h.offVertexSky, quint64( h.vertexSkyBytes ) } );
	}
	quint64 prevEnd = headerBytes;""")

rep("""		if ( iVao >= 0 )
			icrc = lodvCrc32( p + tabs[iVao].off, qsizetype( tabs[iVao].bytes ), icrc );
		if ( icrc != h.indexCrc32 )""",
    """		if ( iVao >= 0 )
			icrc = lodvCrc32( p + tabs[iVao].off, qsizetype( tabs[iVao].bytes ), icrc );
		// v7: the group table then the sky stream, exactly as the writer folds them
		if ( iGrp >= 0 )
			icrc = lodvCrc32( p + tabs[iGrp].off, qsizetype( tabs[iGrp].bytes ), icrc );
		if ( iVsky >= 0 )
			icrc = lodvCrc32( p + tabs[iVsky].off, qsizetype( tabs[iVsky].bytes ), icrc );
		if ( icrc != h.indexCrc32 )""")

# ---- payload read: group table + sky stream ----
rep("""		T.vertexAo.resize( size_t( dataBytes ) );
		if ( dataBytes ) std::memcpy( T.vertexAo.data(), vb + 4 * nOff, size_t( dataBytes ) );
	}
""",
    """		T.vertexAo.resize( size_t( dataBytes ) );
		if ( dataBytes ) std::memcpy( T.vertexAo.data(), vb + 4 * nOff, size_t( dataBytes ) );
	}
	if ( iGrp >= 0 ) {
		T.group.resize( h.instanceCount );
		if ( h.instanceCount ) std::memcpy( T.group.data(), p + h.offGroup, tabs[iGrp].bytes );
		/* THE DENSE RULE, and it is checked whether or not `payloadCheck` is on,
		 * because a group id out of its chunk's range is not a slow consistency
		 * question -- it is a value a viewer would hash into a colour. A chunk
		 * holding C groups uses exactly {0 .. C-1} and uses every one of them. */
		quint64 sum = 0;
		for ( size_t ci = 0; ci < T.chunks.size(); ci++ ) {
			const LodiChunk & c = T.chunks[ci];
			if ( c.instanceCount == 0 )
				continue;
			if ( quint64( c.instanceFirst ) + c.instanceCount > h.instanceCount )
				continue;   // the chunk walk below reports this one properly
			std::vector<bool> used( c.instanceCount, false );
			quint32 hi = 0;
			for ( quint32 i = 0; i < c.instanceCount; i++ ) {
				const quint16 g = T.group[c.instanceFirst + i];
				if ( g >= c.instanceCount )
					return refuse( QString( "group id %1 at instance %2 is past chunk %3's %4 placements; ids are "
						"dense per chunk from 0" ).arg( g ).arg( c.instanceFirst + i ).arg( ci ).arg( c.instanceCount ) );
				used[g] = true;
				hi = std::max( hi, quint32( g ) );
			}
			for ( quint32 g = 0; g <= hi; g++ )
				if ( !used[g] )
					return refuse( QString( "chunk %1 uses group ids up to %2 but never uses %3; ids are dense "
						"per chunk from 0" ).arg( ci ).arg( hi ).arg( g ) );
			sum += quint64( hi ) + 1;
		}
		if ( sum != quint64( h.groupCount ) )
			return refuse( QString( "groupCount %1 but the chunks' group counts sum to %2" )
				.arg( h.groupCount ).arg( sum ) );
	}
	if ( iVsky >= 0 ) {
		/* v7: the same three offset rules s4.8 states for the AO stream, asked
		 * of the sky stream in the same words. */
		const size_t nOff = size_t( h.instanceCount ) + 1;
		T.vertexSkyFirst.resize( nOff );
		const unsigned char * sb = p + h.offVertexSky;
		for ( size_t i = 0; i < nOff; i++ ) {
			T.vertexSkyFirst[i] = getLE<quint32>( sb + 4 * i );
			if ( i && T.vertexSkyFirst[i] < T.vertexSkyFirst[i - 1] )
				return refuse( QString( "vertex-sky offset %1 (%2) is below offset %3 (%4)" )
					.arg( i ).arg( T.vertexSkyFirst[i] ).arg( i - 1 ).arg( T.vertexSkyFirst[i - 1] ) );
		}
		const quint64 dataBytes = quint64( h.vertexSkyBytes ) - 4ull * nOff;
		if ( T.vertexSkyFirst[0] != 0 || quint64( T.vertexSkyFirst[nOff - 1] ) != dataBytes )
			return refuse( QString( "vertex-sky offsets run %1..%2 but the stream carries %3 sky bytes after its %4 offsets" )
				.arg( T.vertexSkyFirst[0] ).arg( T.vertexSkyFirst[nOff - 1] ).arg( dataBytes ).arg( nOff ) );
		T.vertexSky.resize( size_t( dataBytes ) );
		if ( dataBytes ) std::memcpy( T.vertexSky.data(), sb + 4 * nOff, size_t( dataBytes ) );
		/* The two streams stand for one vertex population. A placement with a
		 * sky slice and an AO slice of a different length means one of the two
		 * casts ran on a different mesh. */
		if ( iVao >= 0 )
			for ( size_t i = 0; i < size_t( h.instanceCount ); i++ ) {
				const quint32 sn = T.vertexSkyFirst[i + 1] - T.vertexSkyFirst[i];
				const quint32 an = T.vertexAoFirst[i + 1] - T.vertexAoFirst[i];
				if ( sn && sn != an )
					return refuse( QString( "instance %1 has %2 sky bytes but %3 AO bytes; the two streams are "
						"one vertex population" ).arg( i ).arg( sn ).arg( an ) );
			}
	}
""")

open(p, 'w', encoding='utf-8', newline='').write(s)
print('reader spliced, %d lines' % s.count('\n'))
