import io
p = 'src/lodifile.cpp'
s = open(p, encoding='utf-8', newline='').read()


def rep(old, new, n=1):
    global s
    assert s.count(old) == n, (s.count(old), old[:80])
    s = s.replace(old, new)


rep("""constexpr int H_OFF_VAO = 0xF4, H_VAOBYTES = 0xFC, H_RESERVED_F4 = 0xF4;""",
    """constexpr int H_OFF_VAO = 0xF4, H_VAOBYTES = 0xFC, H_RESERVED_F4 = 0xF4;
/*! v7 (2026-09-18, lane LODIV7): the 256-byte header was FULL -- 0xF1..0xF3 was
 *  all that was left of it, three bytes where v7 needs twenty-four. So a v7
 *  file's header BLOCK is 512 bytes and the new words live in the second half:
 *  0x100 the group table's offset, 0x108 its count, 0x10C its stride, then
 *  0x110 the vertex-sky stream's offset and 0x118 its size. The pad is then
 *  0x11C..0x1FF. This costs no file bytes and moves no payload -- the first
 *  payload is 4,096-aligned, so 0x100..0xFFF was already zero pad in every
 *  .lodi ever written -- and on a v3..v6 file the header crc window is still
 *  exactly the 256 bytes it always was. */
constexpr int H_OFF_GROUP = 0x100, H_GROUPCOUNT = 0x108, H_GROUPSTRIDE = 0x10C;
constexpr int H_OFF_VSKY = 0x110, H_VSKYBYTES = 0x118, H_RESERVED_11C = 0x11C;""")

rep("""	/* THE VERSION IS THE MODULE'S OFF SWITCH (lodifile.h, and contract 11""",
    """	/* v7 (a): THE GROUP TABLE. The emitter hands in a GLOBAL groupKey an
	 * instance; the writer turns those into ids DENSE PER CHUNK from 0, because
	 * only the writer knows the sort and the chunk partition. A component that
	 * straddles a chunk line therefore becomes two groups, one a side -- the
	 * bake never sees such a house whole, and inventing a joint across the seam
	 * would be a number with nothing behind it. */
	std::vector<quint16> grp;
	quint32 groupsTotal = 0, groupedPlacements = 0, largestGroup = 0, singletonGroups = 0;
	if ( set.group ) {
		if ( !set.vertexAo )
			return fail( QStringLiteral( "group table without vertex AO: version 7 is a superset of version 6" ) );
		grp.assign( n, 0 );
		for ( size_t ci = 0; ci < chunks.size(); ci++ ) {
			const LodiChunk & c = chunks[ci];
			if ( c.instanceCount == 0 )
				continue;
			// key -> dense id, in the sorted order, so the ids are a function of the file
			std::unordered_map<quint32, quint32> dense;
			std::vector<quint32> members;
			for ( quint32 i = 0; i < c.instanceCount; i++ ) {
				const quint32 key = set.instances[order[c.instanceFirst + i]].groupKey;
				quint32 id;
				if ( key == LODI_GROUP_ALONE ) {
					id = quint32( members.size() );
					members.push_back( 0 );
				} else {
					auto it = dense.find( key );
					if ( it == dense.end() ) {
						id = quint32( members.size() );
						dense.emplace( key, id );
						members.push_back( 0 );
					} else {
						id = it->second;
					}
				}
				if ( id > 65535 )
					return fail( QString( "chunk %1 needs %2 groups; the group word is a u16 and stops at 65,536" )
						.arg( ci ).arg( quint64( id ) + 1 ) );
				members[id]++;
				grp[c.instanceFirst + i] = quint16( id );
			}
			groupsTotal += quint32( members.size() );
			for ( quint32 m : members ) {
				largestGroup = std::max( largestGroup, m );
				if ( m == 1 )
					singletonGroups++;
				else
					groupedPlacements += m;
			}
		}
	}
	/* v7 (b): THE VERTEX-SKY STREAM, s4.8's layout byte for byte. */
	std::vector<quint8> vsky;
	if ( set.vertexSky ) {
		if ( !set.vertexAo )
			return fail( QStringLiteral( "vertex sky without vertex AO: version 7 is a superset of version 6" ) );
		quint64 total = 0;
		for ( size_t i = 0; i < n; i++ )
			total += set.instances[order[i]].vertexSky.size();
		if ( total > 0xFFFFFFFFull - 4ull * ( n + 1 ) )
			return fail( QString( "vertex-sky stream of %1 bytes does not fit a u32 size word" ).arg( total ) );
		vsky.resize( 4 * ( n + 1 ) + size_t( total ) );
		quint32 cursor = 0;
		for ( size_t i = 0; i <= n; i++ ) {
			std::memcpy( &vsky[4 * i], &cursor, 4 );
			if ( i == n )
				break;
			const LodiSrcInstance & r = set.instances[order[i]];
			/* A placement's two streams stand for the SAME vertices of the SAME
			 * mesh. A length that disagrees means one of the two casts ran on a
			 * different mesh, and that is a refusal, not a shrug. */
			if ( !r.vertexSky.empty() && r.vertexSky.size() != r.vertexAo.size() )
				return fail( QString( "instance %1 (%2) has %3 sky bytes but %4 AO bytes; the two streams are one vertex population" )
					.arg( i ).arg( r.baseName ).arg( r.vertexSky.size() ).arg( r.vertexAo.size() ) );
			if ( !r.vertexSky.empty() )
				std::memcpy( &vsky[4 * ( n + 1 ) + cursor], r.vertexSky.data(), r.vertexSky.size() );
			cursor += quint32( r.vertexSky.size() );
		}
	}
	/* THE VERSION IS THE MODULE'S OFF SWITCH (lodifile.h, and contract 11""")

rep("""	if ( set.vertexAo ) {
		h.version = LODI_VERSION_VERTEX_AO;
		h.vertexAoBytes = quint32( vao.size() );
	}""",
    """	if ( set.vertexAo ) {
		h.version = LODI_VERSION_VERTEX_AO;
		h.vertexAoBytes = quint32( vao.size() );
	}
	/* v7: the version rises when EITHER table is present, so `--lodi-v6` -- both
	 * switches off -- leaves the line above and every byte below untouched. */
	if ( set.group || set.vertexSky ) {
		h.version = LODI_VERSION_GROUP_SKY;
		if ( set.group ) {
			h.groupCount = groupsTotal;
			h.groupStride = LODI_GROUP_STRIDE;
		}
		if ( set.vertexSky )
			h.vertexSkyBytes = quint32( vsky.size() );
	}""")

rep("""	QByteArray file;
	file.resize( LODI_HEADER_BYTES );
	std::memset( file.data(), 0, LODI_HEADER_BYTES );""",
    """	const quint32 headerBytes = lodiHeaderBytes( h.version );
	QByteArray file;
	file.resize( qsizetype( headerBytes ) );
	std::memset( file.data(), 0, size_t( headerBytes ) );""")

rep("""	if ( !vao.empty() )
		h.offVertexAo = payload( vao.data(), quint64( vao.size() ) );
	h.fileBytes""",
    """	if ( !vao.empty() )
		h.offVertexAo = payload( vao.data(), quint64( vao.size() ) );
	// v7: the group table then the sky stream, after everything, so nothing moves
	if ( !grp.empty() )
		h.offGroup = payload( grp.data(), quint64( grp.size() ) * sizeof( quint16 ) );
	if ( !vsky.empty() )
		h.offVertexSky = payload( vsky.data(), quint64( vsky.size() ) );
	h.fileBytes""")

rep("""	h.indexCrc32 = lodvCrc32( reinterpret_cast<const unsigned char *>( vao.data() ), qsizetype( vao.size() ), h.indexCrc32 );
""",
    """	h.indexCrc32 = lodvCrc32( reinterpret_cast<const unsigned char *>( vao.data() ), qsizetype( vao.size() ), h.indexCrc32 );
	/* v7: the group table then the sky stream join last, in that order. Both
	 * absent, zero bytes fold in and a v3..v6 file's CRC does not move. */
	h.indexCrc32 = lodvCrc32( reinterpret_cast<const unsigned char *>( grp.data() ), qsizetype( grp.size() * sizeof( quint16 ) ), h.indexCrc32 );
	h.indexCrc32 = lodvCrc32( reinterpret_cast<const unsigned char *>( vsky.data() ), qsizetype( vsky.size() ), h.indexCrc32 );
""")

rep("""	if ( !vao.empty() ) {
		putLE<quint64>( file, H_OFF_VAO, h.offVertexAo );
		putLE<quint32>( file, H_VAOBYTES, h.vertexAoBytes );
	}
	h.headerCrc32 = lodvCrc32( reinterpret_cast<const unsigned char *>( file.constData() ) + H_PLUGIN, LODI_HEADER_BYTES - H_PLUGIN );""",
    """	if ( !vao.empty() ) {
		putLE<quint64>( file, H_OFF_VAO, h.offVertexAo );
		putLE<quint32>( file, H_VAOBYTES, h.vertexAoBytes );
	}
	if ( !grp.empty() ) {
		putLE<quint64>( file, H_OFF_GROUP, h.offGroup );
		putLE<quint32>( file, H_GROUPCOUNT, h.groupCount );
		putLE<quint16>( file, H_GROUPSTRIDE, h.groupStride );
	}
	if ( !vsky.empty() ) {
		putLE<quint64>( file, H_OFF_VSKY, h.offVertexSky );
		putLE<quint32>( file, H_VSKYBYTES, h.vertexSkyBytes );
	}
	/* The crc window is 0x10 .. headerBytes - 1. On a v3..v6 file headerBytes is
	 * 256 and the window is the one it always was, which is what keeps a v6
	 * bake byte-identical across this lane. */
	h.headerCrc32 = lodvCrc32( reinterpret_cast<const unsigned char *>( file.constData() ) + H_PLUGIN, qsizetype( headerBytes ) - H_PLUGIN );""")

rep("""		stats->aggregateViews = h.aggregateViews;
		stats->version = h.version;""",
    """		stats->aggregateViews = h.aggregateViews;
		stats->groups = groupsTotal;
		stats->groupedPlacements = groupedPlacements;
		stats->largestGroup = largestGroup;
		stats->singletonGroups = singletonGroups;
		stats->vertexSkyBytes = h.vertexSkyBytes;
		stats->vertexSkyPlacements = 0;
		for ( size_t i = 0; i < n; i++ )
			if ( !set.instances[i].vertexSky.empty() )
				stats->vertexSkyPlacements++;
		stats->version = h.version;""")

open(p, 'w', encoding='utf-8', newline='').write(s)
print('writer spliced, %d lines' % s.count('\n'))
