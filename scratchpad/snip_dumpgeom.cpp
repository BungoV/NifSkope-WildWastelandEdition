		/* --dump-geometry is the other question a gate has to ask of a written
		 * chunk: what does each shape WEIGH, and do the invariants the file
		 * itself asserts still hold? One `G` line per shape and one `i` line
		 * listing that shape's object identity indices, so a harness can weigh
		 * and diff two builds of the same chunk without a NIF reader of its
		 * own. Everything printed is a RAW COUNT — the comparisons belong to
		 * the harness, and every count here can be non-zero on a real file:
		 *
		 *   G <block> alpha <0|1> scale <s> verts <n> tris <n> segs <n>
		 *     segbad <n> outofchunk <n> sphereout <n> aabbout <n>
		 *     ids <n> layers <n>
		 *   i <block> <id> <id> ...
		 *
		 * segbad counts triangles whose CENTROID falls in a different cell
		 * from the segment they are listed under (only asked when the shape
		 * has one segment per cell); outofchunk counts centroids outside the
		 * chunk's own 4096-unit miniature square; sphereout and aabbout count
		 * vertices outside the shape's bounding sphere and its node's
		 * multi-bound box. The tolerance is a half-precision ulp at 4096
		 * (4 miniature units), because a chunk's positions are halves while
		 * the bounds that contain them were computed from floats. */
		if ( !lgDumpGeometry.isEmpty() ) {
			NifModel dnif;
			if ( !loadNif( dnif, lgDumpGeometry ) ) {
				err().flush();
				return 1;
			}
			const QStringList nameParts = QFileInfo( lgDumpGeometry ).fileName().split( QChar( '.' ) );
			const int gdim = nameParts.size() >= 5 ? nameParts[1].toInt() : 0;
			constexpr float EPS = 4.0f;                       // one half-float ulp at 4096
			out() << "# lodgen dump-geometry 1 " << lgDumpGeometry << " dim " << gdim << Qt::endl;
			int shapes = 0;
			qint64 totalVerts = 0, totalTris = 0;
			for ( int b = 0; b < dnif.getBlockCount(); b++ ) {
				const QModelIndex iShape = dnif.getBlockIndex( b );
				if ( !dnif.blockInherits( iShape, "BSTriShape" ) )
					continue;
				const QModelIndex iVD = dnif.getIndex( iShape, "Vertex Data" );
				const int nv = int( dnif.get<quint32>( iShape, "Num Vertices" ) );
				if ( !iVD.isValid() || nv <= 0 )
					continue;
				const BSVertexDesc desc( dnif.get<BSVertexDesc>( iShape, "Vertex Desc" ) );
				const bool full = ( desc.GetFlags() & VertexFlags::VF_FULLPREC );
				const bool colors = ( desc.GetFlags() & VertexFlags::VF_COLORS );
				const bool uv2 = ( desc.GetFlags() & VertexFlags::VF_UV_2 );
				const float scale = dnif.get<float>( iShape, "Scale" );
				QVector<Vector3> pos( nv );
				QSet<int> ids, layers;
				for ( int v = 0; v < nv; v++ ) {
					const QModelIndex row = dnif.index( v, 0, iVD );
					pos[v] = full ? dnif.get<Vector3>( row, "Vertex" )
						: Vector3( dnif.get<HalfVector3>( row, "Vertex" ) );
					if ( colors ) {
						const ByteColor4 c = dnif.get<ByteColor4>( row, "Vertex Colors" );
						ids.insert( int( c[0] * 255.0f + 0.5f ) + int( c[1] * 255.0f + 0.5f ) * 256 );
					}
					if ( uv2 ) {
						const Vector2 t = dnif.get<HalfVector2>( row, "UV 2" );
						layers.insert( qRound( t[1] ) );
					}
				}
				const QVector<Triangle> tris = dnif.getArray<Triangle>( dnif.getIndex( iShape, "Triangles" ) );
				const QModelIndex iSegs = dnif.getIndex( iShape, "Segment" );
				const int ns = iSegs.isValid() ? dnif.rowCount( iSegs ) : 0;
				// which segment each triangle is listed under
				QVector<int> segOf( tris.size(), 0 );
				for ( int s = 0; s < ns; s++ ) {
					const QModelIndex seg = dnif.index( s, 0, iSegs );
					const int start = int( dnif.get<quint32>( seg, "Start Index" ) ) / 3;
					const int count = int( dnif.get<quint32>( seg, "Num Primitives" ) );
					for ( int t = start; t < start + count && t < tris.size(); t++ )
						segOf[t] = s;
				}
				const int cells = gdim > 0 ? gdim * gdim : 0;
				const float cellSpan = gdim > 0 ? 4096.0f / float( gdim ) : 4096.0f;
				int segbad = 0, outofchunk = 0;
				for ( int t = 0; t < tris.size(); t++ ) {
					const Vector3 c = ( pos[tris[t].v1()] + pos[tris[t].v2()] + pos[tris[t].v3()] ) * ( 1.0f / 3.0f );
					if ( c[0] < -EPS || c[1] < -EPS || c[0] > 4096.0f + EPS || c[1] > 4096.0f + EPS )
						outofchunk++;
					if ( ns == cells && cells > 1 ) {
						const int lx = qBound( 0, int( c[0] / cellSpan ), gdim - 1 );
						const int ly = qBound( 0, int( c[1] / cellSpan ), gdim - 1 );
						if ( ly * gdim + lx != segOf[t] )
							segbad++;
					}
				}
				int sphereout = 0, aabbout = 0;
				const QModelIndex iBound = dnif.getIndex( iShape, "Bounding Sphere" );
				if ( iBound.isValid() ) {
					const Vector3 c = dnif.get<Vector3>( iBound, "Center" );
					const float r = dnif.get<float>( iBound, "Radius" );
					for ( const Vector3 & p : pos )
						if ( ( p - c ).length() > r + EPS )
							sphereout++;
				}
				const QModelIndex iNode = dnif.getBlockIndex( dnif.getParent( b ) );
				const QModelIndex iMB = iNode.isValid()
					? dnif.getBlockIndex( dnif.getLink( iNode, "Multi Bound" ) ) : QModelIndex();
				const QModelIndex iBox = iMB.isValid() ? dnif.getBlockIndex( dnif.getLink( iMB, "Data" ) ) : QModelIndex();
				if ( iBox.isValid() ) {
					const Vector3 bp = dnif.get<Vector3>( iBox, "Position" ), be = dnif.get<Vector3>( iBox, "Extent" );
					const float tol = EPS * qMax( 1.0f, scale );
					for ( const Vector3 & p : pos ) {
						const Vector3 w = p * scale;
						if ( w[0] < bp[0] - be[0] - tol || w[0] > bp[0] + be[0] + tol
							|| w[1] < bp[1] - be[1] - tol || w[1] > bp[1] + be[1] + tol
							|| w[2] < bp[2] - be[2] - tol || w[2] > bp[2] + be[2] + tol )
							aabbout++;
					}
				}
				out() << "G " << b
					<< " alpha " << ( dnif.getBlockIndex( dnif.getLink( iShape, "Alpha Property" ) ).isValid() ? 1 : 0 )
					<< " scale " << scale
					<< " verts " << nv
					<< " tris " << tris.size()
					<< " segs " << ns
					<< " segbad " << segbad
					<< " outofchunk " << outofchunk
					<< " sphereout " << sphereout
					<< " aabbout " << aabbout
					<< " ids " << ids.size()
					<< " layers " << layers.size() << Qt::endl;
				if ( !ids.isEmpty() ) {
					QList<int> sorted = ids.values();
					std::sort( sorted.begin(), sorted.end() );
					out() << "i " << b;
					for ( int id : sorted )
						out() << " " << id;
					out() << Qt::endl;
				}
				shapes++;
				totalVerts += nv;
				totalTris += tris.size();
			}
			out() << "total shapes " << shapes << " verts " << totalVerts << " tris " << totalTris << Qt::endl;
			out().flush();
			err().flush();
			return shapes ? 0 : 1;
		}
