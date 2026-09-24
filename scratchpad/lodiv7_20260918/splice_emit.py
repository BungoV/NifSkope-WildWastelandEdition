p = 'src/nativeemit.cpp'
s = open(p, encoding='utf-8', newline='').read()


def rep(old, new, n=1):
    global s
    assert s.count(old) == n, (s.count(old), old[:90])
    s = s.replace(old, new)


# ---- 1. parallel per-instance facts the grouping needs ----
rep("""	std::vector<std::array<int, 3>> instChunk;   //!< v6: (chunkX, chunkY, dim) that lit each instance, parallel to set.instances""",
    """	std::vector<std::array<int, 3>> instChunk;   //!< v6: (chunkX, chunkY, dim) that lit each instance, parallel to set.instances
	std::vector<quint8> instArch;   //!< v7: 1 when the base model path is under the architecture folder
	quint32 archPlacements = 0;     //!< v7 census: how many placements the path prefix catches""")

rep("""		r.baseName = QString( "0x%1 (%2)" ).arg( p.baseForm, 8, 16, QChar( '0' ) ).arg( p.model );
		set.instances.push_back( r );
		instChunk.push_back( { p.chunkX, p.chunkY, p.dim } );""",
    """		r.baseName = QString( "0x%1 (%2)" ).arg( p.baseForm, 8, 16, QChar( '0' ) ).arg( p.model );
		set.instances.push_back( r );
		instChunk.push_back( { p.chunkX, p.chunkY, p.dim } );
		/* v7: the path test, taken HERE because this is the only place the
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
		}""")

# ---- 2. the sky cast, in the same place/perVertex loop ----
rep("""	if ( s.vertexAo && s.placementAo && s.world ) {
		set.vertexAo = true;""",
    """	if ( s.vertexAo && s.placementAo && s.world ) {
		set.vertexAo = true;
		// v7: the sky stream rides the SAME loop, the same scene, the same reach
		set.vertexSky = s.lodiV7;""")

rep("""				place( r, instMesh[i], [&]( const DecodedMesh & d, const std::vector<Vector3> & wp ) {
					r.vertexAo.resize( d.count );
					for ( quint32 v = 0; v < d.count; v++ ) {
						const float * ln = &d.nrm[size_t( v ) * 3];
						Vector3 wn( r.rot[0] * ln[0] + r.rot[1] * ln[1] + r.rot[2] * ln[2],
							r.rot[3] * ln[0] + r.rot[4] * ln[1] + r.rot[5] * ln[2],
							r.rot[6] * ln[0] + r.rot[7] * ln[1] + r.rot[8] * ln[2] );
						wn.normalize();
						const float ao = scene.ambientOcclusion( wp[v], wn, maxT );
						r.vertexAo[v] = quint8( std::lround( std::min( 1.0f, std::max( 0.0f, ao ) ) * 255.0f ) );
					}
				}, false );""",
    """				const bool wantSky = set.vertexSky;
				place( r, instMesh[i], [&]( const DecodedMesh & d, const std::vector<Vector3> & wp ) {
					r.vertexAo.resize( d.count );
					if ( wantSky )
						r.vertexSky.resize( d.count );
					for ( quint32 v = 0; v < d.count; v++ ) {
						const float * ln = &d.nrm[size_t( v ) * 3];
						Vector3 wn( r.rot[0] * ln[0] + r.rot[1] * ln[1] + r.rot[2] * ln[2],
							r.rot[3] * ln[0] + r.rot[4] * ln[1] + r.rot[5] * ln[2],
							r.rot[6] * ln[0] + r.rot[7] * ln[1] + r.rot[8] * ln[2] );
						wn.normalize();
						const float ao = scene.ambientOcclusion( wp[v], wn, maxT );
						r.vertexAo[v] = quint8( std::lround( std::min( 1.0f, std::max( 0.0f, ao ) ) * 255.0f ) );
						if ( wantSky ) {
							/* v7 (bungo: "We need per vertex sky visbility too").
							 * The SAME caster, the SAME scene, the SAME reach in
							 * the SAME miniature units as the AO byte beside it --
							 * `skyVisibility` is normal-independent by design, so
							 * the normal above is not passed to it. */
							const float sk = scene.skyVisibility( wp[v], maxT );
							r.vertexSky[v] = quint8( std::lround( std::min( 1.0f, std::max( 0.0f, sk ) ) * 255.0f ) );
						}
					}
				}, false );""")

# ---- 3. the sky census, beside the AO one ----
rep("""		for ( const LodiSrcInstance & r : set.instances ) {
			if ( r.vertexAo.empty() ) {
				vaoEmpty++;
				continue;
			}
			vaoInstances++;
			vaoBytes += r.vertexAo.size();
			for ( quint8 v : r.vertexAo ) {
				vaoSum += v;
				if ( v < 128 )
					vaoDark++;
			}
		}
	}""",
    """		for ( const LodiSrcInstance & r : set.instances ) {
			if ( r.vertexAo.empty() ) {
				vaoEmpty++;
				continue;
			}
			vaoInstances++;
			vaoBytes += r.vertexAo.size();
			for ( quint8 v : r.vertexAo ) {
				vaoSum += v;
				if ( v < 128 )
					vaoDark++;
			}
		}
		for ( const LodiSrcInstance & r : set.instances ) {
			if ( r.vertexSky.empty() )
				continue;
			vskyInstances++;
			vskyBytes += r.vertexSky.size();
			for ( quint8 v : r.vertexSky ) {
				vskySum += v;
				if ( v >= 128 )
					vskyOpen++;
			}
		}
	}
	/* ---- v7: THE GROUPING. bungo 2026-09-18: "The houses should be one object
	 * each though, for identity".
	 *
	 * THIS RULE IS A PROPOSAL. Every knob it turns is in one struct, right
	 * here, so that ruling on it is a matter of changing four numbers rather
	 * than reading the loop. `identity` is NOT touched: it stays the stock
	 * bake's unique per-placement index, and the group is a SECOND word beside
	 * it (docs s4.9). */
	struct GroupKnobs
	{
		/*! Which placements are allowed to merge at all. Bethesda's houses live
		 *  under Architecture\\; a tree, a car and a fence do not, and a rule that
		 *  merged anything touching anything would weld a street into one id. */
		const char * archPrefix = "architecture\\\\";
		/*! How far apart two boxes may be and still count as one object, in
		 *  WORLD units. 16 is a quarter of a 64-unit Bethesda grid step: wide
		 *  enough for a wall kit whose pieces are authored with a seam, narrow
		 *  enough that two houses across a street (thousands of units) cannot
		 *  reach each other. */
		float touchTolerance = 16.0f;
		/*! The DRAWN MESH's local AABB, placed and re-bounded axis-aligned in
		 *  world -- not the base's bound SPHERE. `LodoBase` carries only
		 *  `boundRadius`, and a sphere of that radius around a long wall's
		 *  centre reaches halfway across the street; `LodoMesh` carries
		 *  `aabbMin`/`aabbExtent`, which is the geometry that is actually
		 *  drawn. Set false to measure the sphere rule instead. */
		bool useBox = true;
		//! Spatial-hash cell, world units. 33k instances makes the O(n^2) pair walk impossible.
		float gridCell = 1024.0f;
	};
	const GroupKnobs KNOB;
	if ( s.lodiV7 ) {
		set.group = true;
		const size_t ni = set.instances.size();
		std::vector<quint32> uf( ni );
		for ( size_t i = 0; i < ni; i++ )
			uf[i] = quint32( i );
		std::function<quint32( quint32 )> find = [&]( quint32 a ) {
			while ( uf[a] != a ) { uf[a] = uf[uf[a]]; a = uf[a]; }
			return a;
		};
		auto join = [&]( quint32 a, quint32 b ) {
			a = find( a ); b = find( b );
			if ( a != b )
				uf[std::max( a, b )] = std::min( a, b );
		};
		/* (i) A SCOL part's group is its SCOL reference's group. The parts of
		 * one static collection ARE one object and always were -- the stock
		 * bake split them only so each could carry its own draw key. */
		{
			std::unordered_map<quint32, quint32> scolFirst;
			for ( size_t i = 0; i < ni; i++ ) {
				if ( set.instances[i].scolPart < 0 )
					continue;
				auto f = scolFirst.find( set.instances[i].refFormId );
				if ( f == scolFirst.end() )
					scolFirst.emplace( set.instances[i].refFormId, quint32( i ) );
				else
					join( f->second, quint32( i ) );
			}
		}
		/* (ii) Architecture placements join a connected component over their
		 * world boxes. Everything else is its own group and stays so. */
		struct Box { float lo[3], hi[3]; };
		std::vector<Box> box( ni );
		std::vector<quint32> arch;
		arch.reserve( archPlacements );
		for ( size_t i = 0; i < ni; i++ ) {
			const LodiSrcInstance & r = set.instances[i];
			Box & b = box[i];
			for ( int k = 0; k < 3; k++ ) { b.lo[k] = r.pos[k]; b.hi[k] = r.pos[k]; }
			if ( i >= instArch.size() || !instArch[i] )
				continue;
			quint16 mid = LODO_NO_MESH;
			if ( r.baseId < lib.bases.size() && r.mnamSlot < 4 )
				mid = lib.bases[r.baseId].rep[r.mnamSlot];
			if ( KNOB.useBox && mid != LODO_NO_MESH && mid < lib.meshes.size() ) {
				// the mesh's eight local corners, placed, then re-bounded axis-aligned
				const LodoMesh & me = lib.meshes[mid];
				bool first = true;
				for ( int c = 0; c < 8; c++ ) {
					float lp[3];
					for ( int k = 0; k < 3; k++ )
						lp[k] = ( me.aabbMin[k] + ( ( c >> k ) & 1 ? me.aabbExtent[k] : 0.0f ) ) * r.scale;
					const float wx = r.rot[0] * lp[0] + r.rot[1] * lp[1] + r.rot[2] * lp[2] + r.pos[0];
					const float wy = r.rot[3] * lp[0] + r.rot[4] * lp[1] + r.rot[5] * lp[2] + r.pos[1];
					const float wz = r.rot[6] * lp[0] + r.rot[7] * lp[1] + r.rot[8] * lp[2] + r.pos[2];
					const float w[3] = { wx, wy, wz };
					for ( int k = 0; k < 3; k++ ) {
						b.lo[k] = first ? w[k] : std::min( b.lo[k], w[k] );
						b.hi[k] = first ? w[k] : std::max( b.hi[k], w[k] );
					}
					first = false;
				}
			} else {
				// the way back: the base's bound sphere at the quantised scale
				const float rad = r.boundRadius * r.scale;
				for ( int k = 0; k < 3; k++ ) { b.lo[k] = r.pos[k] - rad; b.hi[k] = r.pos[k] + rad; }
			}
			arch.push_back( quint32( i ) );
		}
		{
			const float T = KNOB.touchTolerance, G = KNOB.gridCell;
			std::unordered_map<quint64, std::vector<quint32>> grid;
			auto keyOf = []( int gx, int gy ) {
				return ( quint64( quint32( gx ) ) << 32 ) | quint32( gy );
			};
			for ( quint32 i : arch ) {
				const Box & b = box[i];
				const int gx0 = int( std::floor( ( b.lo[0] - T ) / G ) ), gx1 = int( std::floor( ( b.hi[0] + T ) / G ) );
				const int gy0 = int( std::floor( ( b.lo[1] - T ) / G ) ), gy1 = int( std::floor( ( b.hi[1] + T ) / G ) );
				for ( int gy = gy0; gy <= gy1; gy++ )
					for ( int gx = gx0; gx <= gx1; gx++ )
						grid[keyOf( gx, gy )].push_back( i );
			}
			for ( auto & cellIt : grid ) {
				std::vector<quint32> & v = cellIt.second;
				for ( size_t a = 0; a < v.size(); a++ )
					for ( size_t b2 = a + 1; b2 < v.size(); b2++ ) {
						const Box & A = box[v[a]];
						const Box & B = box[v[b2]];
						bool touch = true;
						for ( int k = 0; k < 3 && touch; k++ )
							if ( A.lo[k] - B.hi[k] > T || B.lo[k] - A.hi[k] > T )
								touch = false;
						if ( touch )
							join( v[a], v[b2] );
					}
			}
		}
		/* The key the writer sees. A placement that ended up alone gets the
		 * SENTINEL rather than its own index, so "alone" is a stated state and
		 * not a coincidence of numbering. */
		std::vector<quint32> members( ni, 0 );
		for ( size_t i = 0; i < ni; i++ )
			members[find( quint32( i ) )]++;
		for ( size_t i = 0; i < ni; i++ ) {
			const quint32 root = find( quint32( i ) );
			set.instances[i].groupKey = ( members[root] <= 1 ) ? LODI_GROUP_ALONE : root;
		}
	}""")

open(p, 'w', encoding='utf-8', newline='').write(s)
print('nativeemit spliced, %d lines' % s.count('\n'))
