
/*! Far-ring proxy meshes: the ratio for one ring. Ring 0 (dim 4) is what the
 *  player walks up to, and its chunks are the ones every byte-identity gate
 *  stands on, so it is never simplified whatever is set here. */
float lodgenSimplifyRatio( const LodgenSimplifyOptions & opts, int dim )
{
	if ( !opts.enabled )
		return 1.0f;
	switch ( dim ) {
	case 8:
		return opts.ratio8;
	case 16:
		return opts.ratio16;
	case 32:
		return opts.ratio32;
	default:
		return 1.0f;
	}
}

/*! Far-ring proxy meshes, run LAST after the merge: rings 2 and 3 ship at a
 *  fraction of their triangles with the same textures, which is what every
 *  engine since 2017 does with a far cluster. The merge has already made one
 *  shape per material, so "one proxy per cluster" is the same operation as
 *  simplifying that shape in place.
 *
 *  WHAT A SIMPLIFIER MUST NOT LOSE HERE. Every vertex of a chunk carries six
 *  channels the stock engine never reads and FO4CS does
 *  (docs/LODGEN_VERTEX_PACKING.md): the object identity index in vertex
 *  colours R+G, baked AO in B, the sway weight in A, sky visibility in UV2.x,
 *  the texture-array layer in UV2.y, and the ground-contact blend in Eye Data.
 *  Two of those are INDICES, not quantities: an interpolated identity is a
 *  different object and an interpolated layer is a different texture.
 *
 *  So the cut is made per GROUP, keyed by (identity index, array layer):
 *
 *   - meshoptimizer never creates a vertex -- the surviving set is a SUBSET of
 *     the original one -- so no channel is ever interpolated, whatever the
 *     metric does;
 *   - grouping by identity means no collapse can weld two objects together,
 *     and because a group is asked for at least two triangles and its
 *     originals are restored if the simplifier returns none, no object can
 *     vanish from the chunk;
 *   - grouping by layer means a merged shape that spans layers (the manifest's
 *     `A <block> -1`) still has one layer per group, so UV2.y is constant
 *     inside every collapse.
 *
 *  The four quantities ride along as weighted attributes, together with the
 *  normal and the UV, so the metric keeps them meaningful rather than merely
 *  intact. The weights are fractions of the error bound, so one knob scales
 *  the whole metric.
 *
 *  WHAT IS NOT CUT. A shape with an alpha property keeps every triangle: a
 *  cut-out card is four vertices that spell a silhouette and a collapse spends
 *  the silhouette to save nothing. That covers the impostor quads and the
 *  crossed quads inside vanilla's own tree LOD models alike; the impostor
 *  cards are excluded a second time, by object index off the manifest's `C`
 *  lines, so the rule is checkable rather than incidental. Groups at or under
 *  `minTris` keep every triangle too.
 *
 *  AFTERWARDS. Segments are regrouped by the cell of each triangle's CENTROID
 *  (the generator assigns them per placement; after a cut the placement is no
 *  longer the unit), the vertex array is compacted to the survivors, and the
 *  bounding sphere and the node's multi-bound AABB are recomputed from what is
 *  left. The manifest is not touched: no row's meaning changed. */
bool lodgenSimplifyFarRings( const QStringList & btoPaths, const LodgenSimplifyOptions & opts,
	QString * report, QString * error )
{
	auto fail = [error]( const QString & message ) {
		if ( error )
			*error = message;
		return false;
	};
	struct Vtx { Vector3 pos, nrm, tan; Vector2 uv, uv2; float bx = 0, by = 0, bz = 0, eye = 0; Color4 col; };
	int chunks = 0, shapesCut = 0, shapesKept = 0, groupsKept = 0, groupsRestored = 0;
	qint64 triIn = 0, triOut = 0, vtxIn = 0, vtxOut = 0;
	float worstErrorWorld = 0.0f;

	for ( const QString & path : btoPaths ) {
		/* The ring is the chunk's own dim, and its name carries it:
		 * <ws>.<dim>.<x>.<y>.BTO. The shape's Scale carries it too and is
		 * cross-checked below -- the error bound and the segment grid both
		 * depend on it, so a file whose two answers disagree is left alone
		 * rather than cut on a guess. */
		const QStringList nameParts = QFileInfo( path ).fileName().split( QChar( '.' ) );
		const int dim = nameParts.size() >= 5 ? nameParts[1].toInt() : 0;
		const float ratio = lodgenSimplifyRatio( opts, dim );
		if ( dim <= 4 || ratio >= 1.0f || ratio <= 0.0f )
			continue;
		NifModel nif;
		if ( !nif.loadFromFile( path ) )
			continue;

		// the impostor cards standing in this chunk, by object index
		QSet<quint32> cardIndices;
		{
			QFile mf( path + QStringLiteral( ".manifest.txt" ) );
			if ( mf.open( QIODevice::ReadOnly | QIODevice::Text ) ) {
				while ( !mf.atEnd() ) {
					const QString line = QString::fromUtf8( mf.readLine() ).trimmed();
					if ( !line.startsWith( QLatin1String( "C " ) ) )
						continue;
					const QStringList t = line.split( QChar( ' ' ), Qt::SkipEmptyParts );
					if ( t.size() >= 2 )
						cardIndices.insert( t[1].toUInt() );
				}
			}
		}

		/* The bound: errorWorld world units at ring 0, times dim/4 for this
		 * ring, then divided by dim because a chunk shape's vertices are
		 * miniatures at Scale = dim. The two cancel, which is the point --
		 * the same on-screen error at every ring is a constant in the file's
		 * own units. */
		const float localBound = opts.errorWorld * ( float( dim ) / 4.0f ) / float( dim );
		const float cellSpan = 4096.0f / float( dim );   // one cell, in miniature units
		bool changed = false;
		bool sawShape = false;

		for ( int b = 0; b < nif.getBlockCount(); b++ ) {
			const QModelIndex iShape = nif.getBlockIndex( b );
			if ( !nif.isNiBlock( iShape, "BSSubIndexTriShape" ) )
				continue;
			const int parent = nif.getParent( b );
			const QModelIndex iNode = nif.getBlockIndex( parent );
			if ( !iNode.isValid() || !nif.isNiBlock( iNode, "BSMultiBoundNode" ) )
				continue;
			sawShape = true;
			if ( nif.getBlockIndex( nif.getLink( iShape, "Alpha Property" ) ).isValid() ) {
				shapesKept++;
				continue;
			}
			if ( qRound( nif.get<float>( iShape, "Scale" ) ) != dim ) {
				fprintf( stderr, "lodgen: simplify: %s block %d: scale %g is not the ring's dim %d, shape kept\n",
					QFileInfo( path ).fileName().toLocal8Bit().constData(), b,
					double( nif.get<float>( iShape, "Scale" ) ), dim );
				shapesKept++;
				continue;
			}

			const BSVertexDesc desc = nif.get<BSVertexDesc>( iShape, "Vertex Desc" );
			const bool fullPrec = ( ( desc.Value() >> 44 ) & 0x400 ) != 0;
			const QModelIndex iVD = nif.getIndex( iShape, "Vertex Data" );
			const int nv = int( nif.get<quint32>( iShape, "Num Vertices" ) );
			if ( nv < 3 || !iVD.isValid() ) {
				shapesKept++;
				continue;
			}
			const QModelIndex row0 = nif.index( 0, 0, iVD );
			const bool hasCol = nif.getIndex( row0, "Vertex Colors" ).isValid();
			const bool hasUv2 = nif.getIndex( row0, "UV 2" ).isValid();
			const bool hasEye = nif.getIndex( row0, "Eye Data" ).isValid();
			QVector<Vtx> verts( nv );
			for ( int v = 0; v < nv; v++ ) {
				const QModelIndex row = nif.index( v, 0, iVD );
				Vtx & o = verts[v];
				o.pos = fullPrec ? nif.get<Vector3>( row, "Vertex" ) : Vector3( nif.get<HalfVector3>( row, "Vertex" ) );
				o.uv = Vector2( nif.get<HalfVector2>( row, "UV" ) );
				o.nrm = Vector3( nif.get<ByteVector3>( row, "Normal" ) );
				o.tan = Vector3( nif.get<ByteVector3>( row, "Tangent" ) );
				o.bx = nif.get<float>( row, "Bitangent X" );
				o.by = nif.get<float>( row, "Bitangent Y" );
				o.bz = nif.get<float>( row, "Bitangent Z" );
				if ( hasCol )
					o.col = Color4( nif.get<ByteColor4>( row, "Vertex Colors" ) );
				if ( hasUv2 )
					o.uv2 = Vector2( nif.get<HalfVector2>( row, "UV 2" ) );
				if ( hasEye )
					o.eye = nif.get<float>( row, "Eye Data" );
			}
			const QVector<Triangle> tris = nif.getArray<Triangle>( nif.getIndex( iShape, "Triangles" ) );
			if ( tris.isEmpty() ) {
				shapesKept++;
				continue;
			}
			const QModelIndex iSegs = nif.getIndex( iShape, "Segment" );
			const int ns = iSegs.isValid() ? nif.rowCount( iSegs ) : 0;
			if ( ns > 1 && ns != dim * dim ) {
				fprintf( stderr, "lodgen: simplify: %s block %d: %d segments is neither 1 nor %dx%d, shape kept\n",
					QFileInfo( path ).fileName().toLocal8Bit().constData(), b, ns, dim, dim );
				shapesKept++;
				continue;
			}

			/* (identity index, array layer) per triangle. Both are constant
			 * over a triangle by construction: a triangle comes from one
			 * placement's one source shape, which carries one index and sits
			 * on one layer. */
			auto keyOf = [&]( quint16 v ) {
				const Vtx & o = verts[v];
				const quint32 id = hasCol
					? ( quint32( qBound( 0, qRound( o.col.red() * 255.0f ), 255 ) )
						| ( quint32( qBound( 0, qRound( o.col.green() * 255.0f ), 255 ) ) << 8 ) )
					: 0U;
				const quint32 layer = hasUv2 ? quint32( qBound( 0, qRound( o.uv2[1] ), 0xFFFF ) ) : 0U;
				return id | ( layer << 16 );
			};
			QHash<quint32, QVector<int>> groups;
			QVector<quint32> order;
			for ( int t = 0; t < tris.size(); t++ ) {
				const quint32 key = keyOf( tris[t].v1() );
				if ( !groups.contains( key ) )
					order.append( key );
				groups[key].append( t );
			}

			/* Weights as fractions of the error bound: a full unit swing of an
			 * attribute costs that share of the geometric budget, so one knob
			 * scales position and appearance together. UV first because a
			 * swimming texture is the artefact a distant chunk shows soonest;
			 * the baked channels are quantities, and a quarter-bound is enough
			 * to stop a collapse that would flatten one. UV2.y (the layer) is
			 * NOT an attribute: it is in the group key, so it never varies
			 * inside a collapse. */
			std::vector<float> weights;
			for ( int k = 0; k < 3; k++ )
				weights.push_back( localBound * 0.5f );          // normal
			for ( int k = 0; k < 2; k++ )
				weights.push_back( localBound * 1.0f );          // UV
			if ( hasUv2 )
				weights.push_back( localBound * 0.25f );         // sky visibility, UV2.x
			if ( hasCol ) {
				weights.push_back( localBound * 0.25f );         // baked AO, colour B
				weights.push_back( localBound * 0.25f );         // sway weight, colour A
			}
			if ( hasEye )
				weights.push_back( localBound * 0.25f );         // ground contact
			const size_t attrCount = weights.size();

			QVector<Triangle> outTris;
			outTris.reserve( tris.size() );
			std::vector<int> stamp( size_t( nv ), -1 ), localOf( size_t( nv ), 0 );
			int groupId = 0;
			bool cutSomething = false;
			for ( const quint32 key : order ) {
				const QVector<int> & gtris = groups[key];
				const bool isCard = cardIndices.contains( key & 0xFFFFU );
				if ( isCard || gtris.size() <= opts.minTris ) {
					for ( int t : gtris )
						outTris.append( tris[t] );
					groupsKept++;
					continue;
				}
				groupId++;
				std::vector<float> gpos, gattr;
				std::vector<unsigned int> gidx, gvert;
				gidx.reserve( size_t( gtris.size() ) * 3 );
				for ( int t : gtris ) {
					const quint16 corner[3] = { tris[t].v1(), tris[t].v2(), tris[t].v3() };
					for ( int k = 0; k < 3; k++ ) {
						const quint16 gv = corner[k];
						if ( stamp[gv] != groupId ) {
							stamp[gv] = groupId;
							localOf[gv] = int( gvert.size() );
							gvert.push_back( gv );
							const Vtx & o = verts[gv];
							gpos.push_back( o.pos[0] );
							gpos.push_back( o.pos[1] );
							gpos.push_back( o.pos[2] );
							gattr.push_back( o.nrm[0] );
							gattr.push_back( o.nrm[1] );
							gattr.push_back( o.nrm[2] );
							gattr.push_back( o.uv[0] );
							gattr.push_back( o.uv[1] );
							if ( hasUv2 )
								gattr.push_back( o.uv2[0] );
							if ( hasCol ) {
								gattr.push_back( o.col.blue() );
								gattr.push_back( o.col.alpha() );
							}
							if ( hasEye )
								gattr.push_back( o.eye );
						}
						gidx.push_back( (unsigned int) localOf[gv] );
					}
				}
				/* At least two triangles asked of every group, so nothing this
				 * pass touches can leave the chunk: the identity set before
				 * the cut is the identity set after it. */
				const size_t targetIdx = qMax<size_t>( 6,
					size_t( qMax( 1, qRound( double( gtris.size() ) * double( ratio ) ) ) ) * 3 );
				if ( targetIdx >= gidx.size() ) {
					for ( int t : gtris )
						outTris.append( tris[t] );
					groupsKept++;
					continue;
				}
				std::vector<unsigned int> simplified( gidx.size() );
				float resultError = 0.0f;
				const size_t count = meshopt_simplifyWithAttributes( simplified.data(),
					gidx.data(), gidx.size(), gpos.data(), gvert.size(), 12,
					gattr.data(), attrCount * sizeof( float ), weights.data(), attrCount,
					nullptr, targetIdx, localBound, meshopt_SimplifyErrorAbsolute, &resultError );
				if ( count < 3 || ( count % 3 ) != 0 || count > gidx.size() ) {
					/* The simplifier gave nothing usable back. Keep the group
					 * whole: an object that vanishes is a worse answer than an
					 * object that costs its triangles. */
					for ( int t : gtris )
						outTris.append( tris[t] );
					groupsRestored++;
					continue;
				}
				for ( size_t i = 0; i + 2 < count; i += 3 )
					outTris.append( Triangle( quint16( gvert[simplified[i]] ),
						quint16( gvert[simplified[i + 1]] ), quint16( gvert[simplified[i + 2]] ) ) );
				worstErrorWorld = qMax( worstErrorWorld, resultError * float( dim ) );
				cutSomething = cutSomething || count < gidx.size();
			}
			if ( !cutSomething ) {
				shapesKept++;
				continue;
			}

			// segments by the cell of each triangle's centroid
			QVector<QVector<Triangle>> segTris( qMax( 1, ns ) );
			for ( const Triangle & t : outTris ) {
				int seg = 0;
				if ( ns == dim * dim ) {
					const Vector3 c = ( verts[t.v1()].pos + verts[t.v2()].pos + verts[t.v3()].pos ) * ( 1.0f / 3.0f );
					const int lx = qBound( 0, int( c[0] / cellSpan ), dim - 1 );
					const int ly = qBound( 0, int( c[1] / cellSpan ), dim - 1 );
					seg = ly * dim + lx;
				}
				segTris[seg].append( t );
			}
			QVector<Triangle> finalTris;
			QVector<QPair<int, int>> segRuns;
			finalTris.reserve( outTris.size() );
			for ( const QVector<Triangle> & st : segTris ) {
				segRuns.append( qMakePair( finalTris.size(), st.size() ) );
				finalTris += st;
			}

			// compact the vertex array to the survivors, in draw order
			QVector<int> remap( nv, -1 );
			QVector<Vtx> newVerts;
			newVerts.reserve( nv );
			for ( Triangle & t : finalTris ) {
				quint16 corner[3] = { t.v1(), t.v2(), t.v3() };
				for ( int k = 0; k < 3; k++ ) {
					if ( remap[corner[k]] < 0 ) {
						remap[corner[k]] = int( newVerts.size() );
						newVerts.append( verts[corner[k]] );
					}
					corner[k] = quint16( remap[corner[k]] );
				}
				t = Triangle( corner[0], corner[1], corner[2] );
			}

			triIn += tris.size();
			triOut += finalTris.size();
			vtxIn += nv;
			vtxOut += newVerts.size();
			shapesCut++;
			changed = true;

			// write it back
			const quint32 numVerts = quint32( newVerts.size() ), numTris = quint32( finalTris.size() );
			nif.set<quint32>( iShape, "Num Vertices", numVerts );
			nif.set<quint32>( iShape, "Num Triangles", numTris );
			nif.set<quint32>( iShape, "Data Size", numVerts * quint32( desc.GetVertexSize() ) + numTris * 6 );
			nif.setState( BaseModel::Processing );
			nif.updateArraySize( iVD );
			float mnx = 3.4e38f, mny = 3.4e38f, mnz = 3.4e38f, mxx = -3.4e38f, mxy = -3.4e38f, mxz = -3.4e38f;
			for ( int v = 0; v < newVerts.size(); v++ ) {
				const QModelIndex row = nif.index( v, 0, iVD );
				const Vtx & o = newVerts[v];
				mnx = qMin( mnx, o.pos[0] ); mny = qMin( mny, o.pos[1] ); mnz = qMin( mnz, o.pos[2] );
				mxx = qMax( mxx, o.pos[0] ); mxy = qMax( mxy, o.pos[1] ); mxz = qMax( mxz, o.pos[2] );
				if ( fullPrec )
					nif.set<Vector3>( row, "Vertex", o.pos );
				else
					nif.set<HalfVector3>( row, "Vertex", HalfVector3( o.pos ) );
				nif.set<HalfVector2>( row, "UV", HalfVector2( o.uv ) );
				nif.set<ByteVector3>( row, "Normal", ByteVector3( o.nrm ) );
				nif.set<ByteVector3>( row, "Tangent", ByteVector3( o.tan ) );
				nif.set<float>( row, "Bitangent X", o.bx );
				nif.set<float>( row, "Bitangent Y", o.by );
				nif.set<float>( row, "Bitangent Z", o.bz );
				if ( hasCol )
					nif.set<ByteColor4>( row, "Vertex Colors", ByteColor4( FloatVector4( o.col.red(), o.col.green(), o.col.blue(), o.col.alpha() ) ) );
				if ( hasUv2 )
					nif.set<HalfVector2>( row, "UV 2", HalfVector2( o.uv2 ) );
				if ( hasEye )
					nif.set<float>( row, "Eye Data", o.eye );
			}
			const QModelIndex iTris = nif.getIndex( iShape, "Triangles" );
			nif.updateArraySize( iTris );
			nif.setArray<Triangle>( iTris, finalTris );
			nif.set<quint32>( iShape, "Num Primitives", numTris );
			if ( iSegs.isValid() ) {
				nif.set<quint32>( iShape, "Num Segments", quint32( segRuns.size() ) );
				nif.set<quint32>( iShape, "Total Segments", quint32( segRuns.size() ) );
				nif.updateArraySize( iSegs );
				for ( int s = 0; s < segRuns.size(); s++ ) {
					const QModelIndex seg = nif.index( s, 0, iSegs );
					nif.set<quint32>( seg, "Start Index", quint32( segRuns[s].first * 3 ) );
					nif.set<quint32>( seg, "Num Primitives", quint32( segRuns[s].second ) );
					nif.set<quint32>( seg, "Parent Array Index", 0xFFFFFFFFU );
				}
			}
			setBound( &nif, iShape, mnx, mny, mnz, mxx, mxy, mxz );
			nif.restoreState();
			// the node's AABB is in the chunk's own frame: miniatures x dim
			const QModelIndex iMB = nif.getBlockIndex( nif.getLink( iNode, "Multi Bound" ) );
			const QModelIndex iBox = iMB.isValid() ? nif.getBlockIndex( nif.getLink( iMB, "Data" ) ) : QModelIndex();
			if ( iBox.isValid() ) {
				nif.set<Vector3>( iBox, "Position", Vector3( ( mnx + mxx ) * 0.5f * float( dim ),
					( mny + mxy ) * 0.5f * float( dim ), ( mnz + mxz ) * 0.5f * float( dim ) ) );
				nif.set<Vector3>( iBox, "Extent", Vector3( ( mxx - mnx ) * 0.5f * float( dim ),
					( mxy - mny ) * 0.5f * float( dim ), ( mxz - mnz ) * 0.5f * float( dim ) ) );
			}
		}
		if ( sawShape )
			chunks++;
		if ( changed && !nif.saveToFile( path ) )
			return fail( QString( "could not rewrite %1" ).arg( path ) );
	}

	if ( report )
		*report = QString( "%1 shapes cut in %2 chunks: %3 -> %4 triangles, %5 -> %6 vertices"
			" (%7 shapes and %8 groups kept whole, %9 restored; worst error %10 units)" )
			.arg( shapesCut ).arg( chunks ).arg( triIn ).arg( triOut ).arg( vtxIn ).arg( vtxOut )
			.arg( shapesKept ).arg( groupsKept ).arg( groupsRestored )
			.arg( double( worstErrorWorld ), 0, 'f', 1 );
	if ( error )
		error->clear();
	return true;
}
