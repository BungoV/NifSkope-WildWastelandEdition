/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#include "spells/normaltransfer.h"

#include "model/nifmodel.h"
#include "nifsnapshot.h"
#include "skeletontools.h"

#include <QCoreApplication>

#include <cmath>

/*! @file normaltransfer.cpp Copy one mesh's normals onto another.
 *
 * Blender's Data Transfer modifier is the reference, its mapping list included,
 * because that is the vocabulary anyone doing this already has.
 *
 * Two divergences, both forced by the format rather than chosen. A NIF stores
 * ONE normal per vertex, not one per face corner, so every mapping lands on the
 * vertex -- "corner" here means the corner's vertex. And both meshes are read
 * through their world transforms, so a donor posed differently from the target
 * still lines up; Blender leaves that to the object transforms.
 *
 * Lives apart from the spell so the CLI can run the same code, which is also how
 * it gets tested: a modal dialog cannot be driven headlessly, and a mapping
 * nobody can check is a mapping nobody should trust.
 */

namespace NormalTransfer
{

//! Quantised position key, for grouping the vertices that sit in one place.
//! A thousandth of a unit: finer than any seam, coarser than float noise.
static quint64 posKey( const Vector3 & p )
{
	auto q = []( float f ) { return qint64( std::llround( double( f ) * 1000.0 ) ); };
	return quint64( ( q( p[0] ) * 73856093 ) ^ ( q( p[1] ) * 19349663 ) ^ ( q( p[2] ) * 83492791 ) );
}

//! Closest point on a triangle, with its barycentric coordinates.
static Vector3 closestOnTri( const Vector3 & p, const Vector3 & a, const Vector3 & b,
	const Vector3 & c, float bary[3] )
{
	const Vector3 ab = b - a, ac = c - a, ap = p - a;
	const float d1 = Vector3::dotproduct( ab, ap ), d2 = Vector3::dotproduct( ac, ap );
	if ( d1 <= 0.0f && d2 <= 0.0f ) { bary[0] = 1; bary[1] = 0; bary[2] = 0; return a; }
	const Vector3 bp = p - b;
	const float d3 = Vector3::dotproduct( ab, bp ), d4 = Vector3::dotproduct( ac, bp );
	if ( d3 >= 0.0f && d4 <= d3 ) { bary[0] = 0; bary[1] = 1; bary[2] = 0; return b; }
	const float vc = d1 * d4 - d3 * d2;
	if ( vc <= 0.0f && d1 >= 0.0f && d3 <= 0.0f ) {
		const float v = ( d1 - d3 != 0.0f ) ? d1 / ( d1 - d3 ) : 0.0f;
		bary[0] = 1 - v; bary[1] = v; bary[2] = 0;
		return a + ab * v;
	}
	const Vector3 cp = p - c;
	const float d5 = Vector3::dotproduct( ab, cp ), d6 = Vector3::dotproduct( ac, cp );
	if ( d6 >= 0.0f && d5 <= d6 ) { bary[0] = 0; bary[1] = 0; bary[2] = 1; return c; }
	const float vb = d5 * d2 - d1 * d6;
	if ( vb <= 0.0f && d2 >= 0.0f && d6 <= 0.0f ) {
		const float w = ( d2 - d6 != 0.0f ) ? d2 / ( d2 - d6 ) : 0.0f;
		bary[0] = 1 - w; bary[1] = 0; bary[2] = w;
		return a + ac * w;
	}
	const float va = d3 * d6 - d5 * d4;
	if ( va <= 0.0f && ( d4 - d3 ) >= 0.0f && ( d5 - d6 ) >= 0.0f ) {
		const float w = ( ( d4 - d3 ) + ( d5 - d6 ) != 0.0f )
			? ( d4 - d3 ) / ( ( d4 - d3 ) + ( d5 - d6 ) ) : 0.0f;
		bary[0] = 0; bary[1] = 1 - w; bary[2] = w;
		return b + ( c - b ) * w;
	}
	const float denom = va + vb + vc;
	const float v = ( denom != 0.0f ) ? vb / denom : 0.0f;
	const float w = ( denom != 0.0f ) ? vc / denom : 0.0f;
	bary[0] = 1 - v - w; bary[1] = v; bary[2] = w;
	return a + ab * v + ac * w;
}


//! Moller-Trumbore, and deliberately not one-sided: a projection that only
//! looked forward would miss a donor sitting just inside the target's surface.
static bool rayHitsTri( const Vector3 & o, const Vector3 & d, const Vector3 & a,
	const Vector3 & b, const Vector3 & c, float & tOut, float bary[3] )
{
	const Vector3 e1 = b - a, e2 = c - a;
	const Vector3 pv = Vector3::crossproduct( d, e2 );
	const float det = Vector3::dotproduct( e1, pv );
	if ( std::fabs( det ) < 1.0e-9f )
		return false;
	const float inv = 1.0f / det;
	const Vector3 tv = o - a;
	const float u = Vector3::dotproduct( tv, pv ) * inv;
	if ( u < -1.0e-4f || u > 1.0001f )
		return false;
	const Vector3 qv = Vector3::crossproduct( tv, e1 );
	const float v = Vector3::dotproduct( d, qv ) * inv;
	if ( v < -1.0e-4f || u + v > 1.0001f )
		return false;
	tOut = Vector3::dotproduct( e2, qv ) * inv;
	bary[0] = 1.0f - u - v; bary[1] = u; bary[2] = v;
	return true;
}


Mesh read( const NifModel * nif, int block )
{
	Mesh m;
	QModelIndex iShape = nif->getBlockIndex( block );
	const Transform world = skeletonWorldTransform( nif, block );

	QModelIndex iVD = nif->getIndex( iShape, "Vertex Data" );
	if ( iVD.isValid() && nif->rowCount( iVD ) > 0 ) {
		const int nv = nif->rowCount( iVD );
		m.pos.resize( nv );
		m.nrm.resize( nv );
		for ( int v = 0; v < nv; v++ ) {
			QModelIndex row = nif->getIndex( iVD, v );
			m.pos[v] = world * nif->get<Vector3>( row, "Vertex" );
			m.nrm[v] = world.rotation * nif->get<Vector3>( row, "Normal" );
		}
		for ( const Triangle & t : nif->getArray<Triangle>( iShape, "Triangles" ) )
			m.tris.append( { int( t[0] ), int( t[1] ), int( t[2] ) } );
	} else {
		QModelIndex iData = nif->getBlockIndex( nif->getLink( iShape, "Data" ) );
		if ( !iData.isValid() )
			return m;
		const QVector<Vector3> verts = nif->getArray<Vector3>( iData, "Vertices" );
		const QVector<Vector3> norms = nif->getArray<Vector3>( iData, "Normals" );
		if ( verts.isEmpty() || norms.size() != verts.size() )
			return m;
		m.pos.resize( verts.size() );
		m.nrm.resize( verts.size() );
		for ( int v = 0; v < verts.size(); v++ ) {
			m.pos[v] = world * verts.at( v );
			m.nrm[v] = world.rotation * norms.at( v );
		}
		for ( const Triangle & t : nif->getArray<Triangle>( iData, "Triangles" ) )
			m.tris.append( { int( t[0] ), int( t[1] ), int( t[2] ) } );
	}

	m.incident.resize( m.pos.size() );
	for ( int t = 0; t < m.tris.size(); t++ ) {
		const auto & tr = m.tris.at( t );
		for ( int c = 0; c < 3; c++ )
			if ( tr[c] < m.incident.size() )
				m.incident[tr[c]].append( t );
	}
	return m;
}


QVector<Vector3> map( const Mesh & src, const Mesh & tgt, int mapping, float mix )
{
	QVector<Vector3> result( tgt.pos.size() );

	// source vertices grouped by position, so a seam can be judged as the whole
	// seam rather than as whichever of its halves the search happened to reach
	QHash<quint64, QVector<int>> coincident;
	if ( mapping == NearestCornerBestNormal ) {
		for ( int i = 0; i < src.pos.size(); i++ )
			coincident[posKey( src.pos.at( i ) )].append( i );
	}

	// target face normals, needed only by "best matching face normal"
	QVector<Vector3> tgtFaceNormal;
	if ( mapping == 2 ) {
		tgtFaceNormal.resize( tgt.pos.size() );
		for ( const auto & tr : tgt.tris ) {
			const Vector3 fn = Vector3::crossproduct( tgt.pos.at( tr[1] ) - tgt.pos.at( tr[0] ),
													  tgt.pos.at( tr[2] ) - tgt.pos.at( tr[0] ) );
			for ( int c = 0; c < 3; c++ )
				tgtFaceNormal[tr[c]] += fn;
		}
	}

	for ( int v = 0; v < tgt.pos.size(); v++ ) {
		const Vector3 & p = tgt.pos.at( v );
		Vector3 n = tgt.nrm.at( v );

		if ( mapping == 0 && v < src.nrm.size() ) {
			n = src.nrm.at( v );
		} else if ( mapping != 0 ) {
			// nearest source vertex, then refine among the faces it belongs
			// to — the same shape of search the rigging transfer uses
			int nv = -1;
			float best = 1.0e30f;
			for ( int i = 0; i < src.pos.size(); i++ ) {
				const float d = ( src.pos.at( i ) - p ).squaredLength();
				if ( d < best ) { best = d; nv = i; }
			}
			if ( nv < 0 ) {
				result[v] = n;
				continue;
			}
			const QVector<int> & faces = src.incident.at( nv );

			if ( mapping == 1 ) {
				/* Every corner AT that position, not only the ones the nearest
				 * vertex happens to share a face with.
				 *
				 * A seam is two or more vertices in the same place carrying
				 * different normals, and the nearest-vertex search returns
				 * whichever of them it saw first. Judging "best matching" only
				 * among that one's faces therefore judges one side of the seam,
				 * and on a mesh transferred onto ITSELF — where the answer has to
				 * be the normal it already has — that came out 127 degrees wrong.
				 */
				float bestDot = -2.0f;
				Vector3 pick = src.nrm.at( nv );
				const Vector3 & at = src.pos.at( nv );
				for ( int sv : coincident.value( posKey( at ) ) ) {
					if ( ( src.pos.at( sv ) - at ).squaredLength() > 1.0e-8f )
						continue;
					const float dt = Vector3::dotproduct( src.nrm.at( sv ), tgt.nrm.at( v ) );
					if ( dt > bestDot ) { bestDot = dt; pick = src.nrm.at( sv ); }
				}
				for ( int t : faces )
					for ( int c = 0; c < 3; c++ ) {
						const int sv = src.tris.at( t )[c];
						const float dt = Vector3::dotproduct( src.nrm.at( sv ), tgt.nrm.at( v ) );
						if ( dt > bestDot ) { bestDot = dt; pick = src.nrm.at( sv ); }
					}
				n = pick;
			} else if ( mapping == 2 ) {
				float bestDot = -2.0f;
				Vector3 pick = src.nrm.at( nv );
				for ( int t : faces ) {
					const auto & tr = src.tris.at( t );
					const Vector3 fn = Vector3::crossproduct( src.pos.at( tr[1] ) - src.pos.at( tr[0] ),
															  src.pos.at( tr[2] ) - src.pos.at( tr[0] ) );
					const float dt = Vector3::dotproduct( fn, tgtFaceNormal.at( v ) );
					if ( dt <= bestDot )
						continue;
					bestDot = dt;
					int nearest = tr[0];
					float nd = 1.0e30f;
					for ( int c = 0; c < 3; c++ ) {
						const float d = ( src.pos.at( tr[c] ) - p ).squaredLength();
						if ( d < nd ) { nd = d; nearest = tr[c]; }
					}
					pick = src.nrm.at( nearest );
				}
				n = pick;
			} else {
				// the three that need the closest point on a face
				int bt = -1;
				float bd = 1.0e30f, bbary[3] = { 1, 0, 0 };
				for ( int t : faces ) {
					const auto & tr = src.tris.at( t );
					float bary[3];
					const Vector3 cpt = closestOnTri( p, src.pos.at( tr[0] ), src.pos.at( tr[1] ),
													  src.pos.at( tr[2] ), bary );
					const float d = ( cpt - p ).squaredLength();
					if ( d < bd ) {
						bd = d; bt = t;
						bbary[0] = bary[0]; bbary[1] = bary[1]; bbary[2] = bary[2];
					}
				}
				if ( mapping == 5 ) {
					// project along this vertex's own normal; the nearest face
					// stays as the fallback when the ray leaves the surface
					float bestT = 1.0e30f;
					Vector3 dir = tgt.nrm.at( v );
					if ( dir.squaredLength() > 1.0e-12f ) {
						dir.normalize();
						for ( int t : faces ) {
							const auto & tr = src.tris.at( t );
							float tt, bary[3];
							if ( !rayHitsTri( p, dir, src.pos.at( tr[0] ), src.pos.at( tr[1] ),
											  src.pos.at( tr[2] ), tt, bary ) )
								continue;
							if ( std::fabs( tt ) < std::fabs( bestT ) ) {
								bestT = tt; bt = t;
								bbary[0] = bary[0]; bbary[1] = bary[1]; bbary[2] = bary[2];
							}
						}
					}
				}
				if ( bt < 0 ) {
					n = src.nrm.at( nv );
				} else {
					const auto & tr = src.tris.at( bt );
					if ( mapping == 3 ) {
						int nearest = tr[0];
						float nd = 1.0e30f;
						for ( int c = 0; c < 3; c++ ) {
							const float d = ( src.pos.at( tr[c] ) - p ).squaredLength();
							if ( d < nd ) { nd = d; nearest = tr[c]; }
						}
						n = src.nrm.at( nearest );
					} else {
						n = src.nrm.at( tr[0] ) * bbary[0]
						  + src.nrm.at( tr[1] ) * bbary[1]
						  + src.nrm.at( tr[2] ) * bbary[2];
					}
				}
			}
		}

		if ( n.squaredLength() < 1.0e-12f )
			n = tgt.nrm.at( v );
		n.normalize();
		if ( mix < 1.0f ) {
			Vector3 blended = tgt.nrm.at( v ) * ( 1.0f - mix ) + n * mix;
			if ( blended.squaredLength() > 1.0e-12f ) {
				blended.normalize();
				n = blended;
			}
		}
		result[v] = n;
	}
	return result;
}


Mesh combine( const QVector<Mesh> & parts )
{
	Mesh m;
	if ( parts.size() == 1 )
		return parts.first();
	int verts = 0, tris = 0;
	for ( const Mesh & p : parts ) {
		verts += p.pos.size();
		tris += p.tris.size();
	}
	m.pos.reserve( verts );
	m.nrm.reserve( verts );
	m.tris.reserve( tris );
	for ( const Mesh & p : parts ) {
		const int base = m.pos.size();
		m.pos += p.pos;
		m.nrm += p.nrm;
		for ( const auto & t : p.tris )
			m.tris.append( { t[0] + base, t[1] + base, t[2] + base } );
	}
	m.incident.resize( m.pos.size() );
	for ( int t = 0; t < m.tris.size(); t++ ) {
		const auto & tr = m.tris.at( t );
		for ( int c = 0; c < 3; c++ )
			if ( tr[c] < m.incident.size() )
				m.incident[tr[c]].append( t );
	}
	return m;
}

int apply( NifModel * nif, int targetBlock, const QVector<Vector3> & normals )
{
	if ( !nif || normals.isEmpty() )
		return 0;
	QModelIndex iTarget = nif->getBlockIndex( targetBlock );
	if ( !iTarget.isValid() )
		return 0;
	// back into the shape's own space: read() brought both meshes into world
	const Transform tWorld = skeletonWorldTransform( nif, targetBlock );
	const Matrix toLocal = tWorld.rotation.inverted();
	int written = 0;
	nifSnapshotOp( nif, QCoreApplication::translate( "NormalTransfer", "Transfer Normals" ), [&]() {
		QModelIndex iVD = nif->getIndex( iTarget, "Vertex Data" );
		if ( iVD.isValid() && nif->rowCount( iVD ) > 0 ) {
			for ( int v = 0; v < normals.size() && v < nif->rowCount( iVD ); v++ )
				if ( nif->set<ByteVector3>( nif->getIndex( iVD, v ), "Normal", toLocal * normals.at( v ) ) )
					written++;
		} else {
			QModelIndex iData = nif->getBlockIndex( nif->getLink( iTarget, "Data" ) );
			QVector<Vector3> norms = nif->getArray<Vector3>( iData, "Normals" );
			for ( int v = 0; v < norms.size() && v < normals.size(); v++ ) {
				norms[v] = toLocal * normals.at( v );
				written++;
			}
			nif->setArray<Vector3>( iData, "Normals", norms );
		}
	} );
	return written;
}

QString mappingName( int mapping )
{
	static const char * const names[MappingCount] = {
		QT_TRANSLATE_NOOP( "NormalTransfer", "Topology" ),
		QT_TRANSLATE_NOOP( "NormalTransfer", "Nearest Corner and Best Matching Normal" ),
		QT_TRANSLATE_NOOP( "NormalTransfer", "Nearest Corner and Best Matching Face Normal" ),
		QT_TRANSLATE_NOOP( "NormalTransfer", "Nearest Corner of Nearest Face" ),
		QT_TRANSLATE_NOOP( "NormalTransfer", "Nearest Face Interpolated" ),
		QT_TRANSLATE_NOOP( "NormalTransfer", "Projected Face Interpolated" )
	};
	if ( mapping < 0 || mapping >= MappingCount )
		return QCoreApplication::translate( "NormalTransfer", "(unknown mapping)" );
	return QCoreApplication::translate( "NormalTransfer", names[mapping] );
}

} // namespace NormalTransfer
