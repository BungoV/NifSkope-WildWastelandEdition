/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENSE BLOCK *****/

#include "cellfarvanilla.h"

#include "gamemanager.h"
#include "lodgen.h"
#include "model/nifmodel.h"
#include "spells/blocks.h"

#include <QBuffer>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>

#include <algorithm>
#include <array>
#include <cmath>

// lane FARLOD1: see cellfarvanilla.h

namespace
{

constexpr float kFvCell = 4096.0f;

int fvFloorDiv( int a, int b )
{
	return ( a >= 0 ) ? a / b : -( ( -a + b - 1 ) / b );
}

//! A chunk file through the resource manager (archives + the game's folders), then the loose resource stack.
bool fvLoad( const QString & path, NifModel & src, QString * why )
{
	QByteArray bytes;
	const QString ext = QStringLiteral( "." ) + QFileInfo( path ).suffix().toLower();
	bool found = Game::GameManager::get_file( bytes, Game::FALLOUT_4, path, "meshes", ext.toLatin1().constData() );
	if ( !found ) {
		const QStringList dirs = Game::GameManager::folders( Game::FALLOUT_4 ) + lodgenResourceSearchPaths();
		for ( const QString & f : dirs ) {
			if ( !QFileInfo( f ).isDir() )
				continue;
			for ( const QString & root : { f, QDir::cleanPath( f + QStringLiteral( "/.." ) ) } ) {
				QFile file( QDir( root ).filePath( path ) );
				if ( file.open( QIODevice::ReadOnly ) ) {
					bytes = file.readAll();
					found = !bytes.isEmpty();
					if ( found )
						break;
				}
			}
			if ( found )
				break;
		}
	}
	if ( !found ) {
		*why = QStringLiteral( "missing" );
		return false;
	}
	QBuffer dev( &bytes );
	const bool ok = dev.open( QIODevice::ReadOnly ) && src.load( dev, path.toLocal8Bit().constData() );
	src.resetState();
	if ( !ok ) {
		*why = QStringLiteral( "read %1 bytes, did not load" ).arg( bytes.size() );
		return false;
	}
	return true;
}

Transform fvWorldOf( const NifModel & src, const QModelIndex & iShape )
{
	Transform t( &src, iShape );
	QModelIndex p = src.getBlockIndex( src.getParent( src.getBlockNumber( iShape ) ) );
	for ( int hop = 0; hop < 64 && p.isValid() && src.blockInherits( p, "NiAVObject" ); hop++ ) {
		t = Transform( &src, p ) * t;
		p = src.getBlockIndex( src.getParent( src.getBlockNumber( p ) ) );
	}
	return t;
}

//! World-unit rectangle [x0, x1) x [y0, y1); empty when x1 <= x0.
struct FvRect
{
	float x0 = 0, y0 = 0, x1 = 0, y1 = 0;
	bool empty() const { return x1 <= x0 || y1 <= y0; }
	bool has( float x, float y ) const { return !empty() && x >= x0 && x < x1 && y >= y0 && y < y1; }
	static FvRect cells( int cx0, int cy0, int cx1, int cy1 )
	{
		FvRect r;
		if ( cx1 < cx0 || cy1 < cy0 )
			return r;
		r.x0 = float( cx0 ) * kFvCell;
		r.y0 = float( cy0 ) * kFvCell;
		r.x1 = float( cx1 + 1 ) * kFvCell;
		r.y1 = float( cy1 + 1 ) * kFvCell;
		return r;
	}
};

//! A clip polygon corner: world x, y and its barycentric weights in the source triangle.
struct FvPt
{
	float x, y, b[3];
};
using FvPoly = std::vector<FvPt>;

//! Sutherland-Hodgman against a*x + b*y + c >= 0.
FvPoly fvClip( const FvPoly & in, float a, float b, float c )
{
	FvPoly out;
	const size_t n = in.size();
	for ( size_t i = 0; i < n; i++ ) {
		const FvPt & p = in[i];
		const FvPt & q = in[( i + 1 ) % n];
		const float dp = a * p.x + b * p.y + c, dq = a * q.x + b * q.y + c;
		if ( dp >= 0 )
			out.push_back( p );
		if ( ( dp >= 0 ) != ( dq >= 0 ) ) {
			const float t = dp / ( dp - dq );
			FvPt r;
			r.x = p.x + ( q.x - p.x ) * t;
			r.y = p.y + ( q.y - p.y ) * t;
			for ( int k = 0; k < 3; k++ )
				r.b[k] = p.b[k] + ( q.b[k] - p.b[k] ) * t;
			out.push_back( r );
		}
	}
	return out;
}

float fvArea( const FvPoly & p )
{
	float a = 0;
	for ( size_t i = 0; i < p.size(); i++ ) {
		const FvPt & u = p[i];
		const FvPt & v = p[( i + 1 ) % p.size()];
		a += u.x * v.y - v.x * u.y;
	}
	return std::fabs( a ) * 0.5f;
}

//! Keep the part inside `r`.
FvPoly fvInside( const FvPoly & p, const FvRect & r )
{
	FvPoly q = fvClip( p, 1, 0, -r.x0 );
	if ( q.size() >= 3 ) q = fvClip( q, -1, 0, r.x1 );
	if ( q.size() >= 3 ) q = fvClip( q, 0, 1, -r.y0 );
	if ( q.size() >= 3 ) q = fvClip( q, 0, -1, r.y1 );
	return q;
}

//! The part outside `r`, as up to four convex pieces (west, east, south band, north band).
void fvOutside( const FvPoly & p, const FvRect & r, std::vector<FvPoly> & out )
{
	if ( r.empty() ) {
		out.push_back( p );
		return;
	}
	FvPoly w = fvClip( p, -1, 0, r.x0 );
	if ( w.size() >= 3 && fvArea( w ) > 1e-3f ) out.push_back( w );
	FvPoly e = fvClip( p, 1, 0, -r.x1 );
	if ( e.size() >= 3 && fvArea( e ) > 1e-3f ) out.push_back( e );
	FvPoly mid = fvClip( p, 1, 0, -r.x0 );
	if ( mid.size() >= 3 ) mid = fvClip( mid, -1, 0, r.x1 );
	if ( mid.size() < 3 )
		return;
	FvPoly s = fvClip( mid, 0, -1, r.y0 );
	if ( s.size() >= 3 && fvArea( s ) > 1e-3f ) out.push_back( s );
	FvPoly n = fvClip( mid, 0, 1, -r.y1 );
	if ( n.size() >= 3 && fvArea( n ) > 1e-3f ) out.push_back( n );
}

struct FvVert
{
	Vector3 p;     // shape-local
	Vector2 uv;
	Vector3 n, t;
	Color4 c;
};

enum FvKind { FvTerrain, FvObjects };

//! One shape's triangles against the ring's region. Terrain: exact clip + seam snap; objects: by centre.
void fvShape( NifModel & src, const QModelIndex & iS, FvKind kind, const FvRect & outer, const FvRect & cut,
	const FvRect & ours, const std::function<bool( float, float, float * )> & landZ, WwFarVanRing & ring,
	int skirt )   // 0 none (water, objects), 1 the cut and ours lines, 2 also the ring's own rim
{
	const quint32 nv = src.get<quint32>( iS, "Num Vertices" );
	const QModelIndex iVD = src.getIndex( iS, "Vertex Data" );
	const QModelIndex iTri = src.getIndex( iS, "Triangles" );
	if ( !nv || !iVD.isValid() || !iTri.isValid() )
		return;
	const QVector<Triangle> tris = src.getArray<Triangle>( iTri );
	const Transform xf = fvWorldOf( src, iS );
	const BSVertexDesc desc = src.get<BSVertexDesc>( iS, "Vertex Desc" );
	const quint16 flags = quint16( ( desc.Value() >> 44 ) & 0xFFFF );
	const bool fullPrec = ( flags & VF_FULLPREC ) != 0;
	std::vector<Vector3> w( nv );
	std::vector<Vector3> loc( nv );
	for ( quint32 v = 0; v < nv; v++ ) {
		const QModelIndex row = src.index( int( v ), 0, iVD );
		loc[v] = fullPrec ? src.get<Vector3>( row, "Vertex" ) : Vector3( src.get<HalfVector3>( row, "Vertex" ) );
		w[v] = xf * loc[v];
	}
	auto keepPt = [&]( float x, float y ) {
		return outer.has( x, y ) && !cut.has( x, y ) && !ours.has( x, y );
	};
	const bool plain = src.isNiBlock( iS, "BSTriShape" ) || src.isNiBlock( iS, "BSMeshLODTriShape" );

	if ( kind == FvObjects || !plain ) {
		// by centre: a shape we cannot re-index (segments) keeps or drops whole by its centre
		if ( !plain ) {
			Vector3 c;
			for ( const Vector3 & p : w )
				c += p;
			c /= float( nv );
			if ( !keepPt( c[0], c[1] ) ) {
				ring.droppedTris += tris.size();
				src.set<quint32>( iS, "Flags", src.get<quint32>( iS, "Flags" ) | 1u );
			} else {
				( kind == FvTerrain ? ring.terrainTris : ring.objectTris ) += tris.size();
			}
			return;
		}
		QVector<Triangle> kept;
		for ( const Triangle & t : tris ) {
			if ( t.v1() >= nv || t.v2() >= nv || t.v3() >= nv )
				continue;
			const Vector3 c = ( w[t.v1()] + w[t.v2()] + w[t.v3()] ) / 3.0f;
			if ( keepPt( c[0], c[1] ) )
				kept << t;
		}
		ring.droppedTris += tris.size() - kept.size();
		ring.objectTris += kept.size();
		if ( kept.size() == tris.size() )
			return;
		const int stride = int( desc.GetVertexSize() );
		src.set<quint32>( iS, "Num Triangles", quint32( kept.size() ) );
		src.set<quint32>( iS, "Data Size", quint32( nv * stride + kept.size() * 6 ) );
		src.updateArraySize( iTri );
		src.setArray<Triangle>( iTri, kept );
		if ( src.isNiBlock( iS, "BSMeshLODTriShape" ) ) {
			src.set<quint32>( iS, "LOD0 Size", quint32( kept.size() ) );
			src.set<quint32>( iS, "LOD1 Size", 0 );
			src.set<quint32>( iS, "LOD2 Size", 0 );
		}
		return;
	}

	// ---- terrain: the exact clip
	const bool hasUv = ( flags & VF_UV ) != 0, hasN = ( flags & VF_NORMAL ) != 0, hasT = ( flags & VF_TANGENT ) != 0;
	const bool hasC = ( flags & VF_COLORS ) != 0;
	std::vector<FvVert> V( nv );
	for ( quint32 v = 0; v < nv; v++ ) {
		const QModelIndex row = src.index( int( v ), 0, iVD );
		FvVert & o = V[v];
		o.p = loc[v];
		o.uv = hasUv ? Vector2( src.get<HalfVector2>( row, "UV" ) ) : Vector2();
		o.n = hasN ? Vector3( src.get<ByteVector3>( row, "Normal" ) ) : Vector3( 0, 0, 1 );
		o.t = hasT ? Vector3( src.get<ByteVector3>( row, "Tangent" ) ) : Vector3( 1, 0, 0 );
		o.c = hasC ? Color4( src.get<ByteColor4>( row, "Vertex Colors" ) ) : Color4( 1, 1, 1, 1 );
	}
	QVector<Triangle> out;
	bool changed = false;
	/* FINALFIX: one vertex per clip point. Two triangles sharing an edge each cut it where a clip line
	 * crosses it, from their own barycentrics -- a float apart, and the hairline between them showed the clear
	 * colour (single-pixel holes in the sky census of the Vanilla top view). Welded on a 1/8-unit lattice. */
	QHash<quint64, quint16> weld;
	auto weldKey = []( const Vector3 & p ) {
		return ( quint64( quint32( qint32( std::lround( p[0] * 8.0f ) ) ) ) << 32 )
			| quint64( quint32( qint32( std::lround( p[1] * 8.0f ) ) ) );
	};
	for ( const Triangle & t : tris ) {
		const quint16 id[3] = { t.v1(), t.v2(), t.v3() };
		if ( id[0] >= nv || id[1] >= nv || id[2] >= nv )
			continue;
		float mnx = 1e30f, mny = 1e30f, mxx = -1e30f, mxy = -1e30f;
		for ( int k = 0; k < 3; k++ ) {
			mnx = std::min( mnx, w[id[k]][0] );
			mxx = std::max( mxx, w[id[k]][0] );
			mny = std::min( mny, w[id[k]][1] );
			mxy = std::max( mxy, w[id[k]][1] );
		}
		auto disjoint = [&]( const FvRect & r ) {
			return r.empty() || mxx <= r.x0 || mnx >= r.x1 || mxy <= r.y0 || mny >= r.y1;
		};
		const bool inOuter = mnx >= outer.x0 && mxx <= outer.x1 && mny >= outer.y0 && mxy <= outer.y1;
		if ( inOuter && disjoint( cut ) && disjoint( ours ) ) {
			out << t;
			continue;
		}
		changed = true;
		FvPoly p( 3 );
		for ( int k = 0; k < 3; k++ ) {
			p[size_t( k )] = { w[id[k]][0], w[id[k]][1], { 0, 0, 0 } };
			p[size_t( k )].b[k] = 1.0f;
		}
		std::vector<FvPoly> pieces;
		FvPoly in = fvInside( p, outer );
		if ( in.size() >= 3 ) {
			std::vector<FvPoly> a;
			fvOutside( in, cut, a );
			for ( const FvPoly & q : a )
				fvOutside( q, ours, pieces );
		}
		if ( pieces.empty() ) {
			ring.droppedTris++;
			continue;
		}
		for ( const FvPoly & q : pieces ) {
			std::vector<quint16> ids;
			for ( const FvPt & c : q ) {
				int same = -1;
				for ( int k = 0; k < 3; k++ )
					if ( c.b[k] > 0.99999f )
						same = k;
				if ( same >= 0 ) {
					ids.push_back( id[same] );
					continue;
				}
				FvVert o;
				o.p = V[id[0]].p * c.b[0] + V[id[1]].p * c.b[1] + V[id[2]].p * c.b[2];
				o.uv = V[id[0]].uv * c.b[0] + V[id[1]].uv * c.b[1] + V[id[2]].uv * c.b[2];
				o.n = V[id[0]].n * c.b[0] + V[id[1]].n * c.b[1] + V[id[2]].n * c.b[2];
				o.t = V[id[0]].t * c.b[0] + V[id[1]].t * c.b[1] + V[id[2]].t * c.b[2];
				o.c = V[id[0]].c * c.b[0] + V[id[1]].c * c.b[1] + V[id[2]].c * c.b[2];
				o.n.normalize();
				o.t.normalize();
				Vector3 wo = xf * o.p;
				/* FINALFIX: a clip point on a cell line (every clip line is one: the block, our chunks, the ring)
				 * lands on the line exactly. From barycentrics it sat a float off, so where two chunks' cut edges
				 * meet the LAND (x -16 on the block's north and south edges) each chunk had its own corner -- the
				 * 2 px pinholes of the Vanilla top view (sky census, 10-06 run8). */
				if ( xf.scale > 0.0f ) {
					bool moved = false;
					for ( int a = 0; a < 2; a++ ) {
						const float line = std::round( wo[a] / kFvCell ) * kFvCell;
						if ( wo[a] != line && std::fabs( wo[a] - line ) < 0.5f ) {
							wo[a] = line;
							moved = true;
						}
					}
					if ( moved )
						o.p = xf.inverted() * wo;
				}
				const quint64 wk = weldKey( wo );
				const auto hit = weld.constFind( wk );
				if ( hit != weld.constEnd() && std::fabs( w[hit.value()][2] - wo[2] ) < 1.0f ) {
					ids.push_back( hit.value() );
					continue;
				}
				if ( V.size() >= 65535 )
					break;
				V.push_back( o );
				w.push_back( wo );
				ids.push_back( quint16( V.size() - 1 ) );
				weld.insert( wk, ids.back() );
			}
			for ( size_t k = 1; k + 1 < ids.size(); k++ ) {
				if ( ids[0] == ids[k] || ids[k] == ids[k + 1] || ids[0] == ids[k + 1] )
					continue;
				out << Triangle( ids[0], ids[k], ids[k + 1] );
				ring.clipTris++;
			}
		}
	}
	ring.terrainTris += out.size();

	const bool upright = std::fabs( xf.rotation( 0, 0 ) - 1.0f ) < 1e-4f && std::fabs( xf.rotation( 1, 1 ) - 1.0f ) < 1e-4f
		&& std::fabs( xf.rotation( 2, 2 ) - 1.0f ) < 1e-4f && xf.scale > 0.0f;
	auto onEdge = [&]( const FvRect & r, float x, float y ) {
		if ( r.empty() )
			return false;
		const float e = 0.5f;
		const bool inX = x >= r.x0 - e && x <= r.x1 + e, inY = y >= r.y0 - e && y <= r.y1 + e;
		return ( inY && ( std::fabs( x - r.x0 ) < e || std::fabs( x - r.x1 ) < e ) )
			|| ( inX && ( std::fabs( y - r.y0 ) < e || std::fabs( y - r.y1 ) < e ) );
	};

	/* THE SEAM: every vertex on a clip line (the footprint's edge, the cut's, our coverage's) to the LAND's
	 * height there -- the line our ring's or the block's edge stands on */
	std::vector<char> snappedV( V.size(), 0 );   // moved to the LAND by the seam below (the skirt reads it)
	if ( changed && upright && landZ ) {
		std::vector<char> used( V.size(), 0 );
		for ( const Triangle & t : out )
			used[t.v1()] = used[t.v2()] = used[t.v3()] = 1;
		for ( size_t v = 0; v < V.size(); v++ ) {
			if ( !used[v] )
				continue;
			const float x = w[v][0], y = w[v][1];
			if ( !onEdge( outer, x, y ) && !onEdge( cut, x, y ) && !onEdge( ours, x, y ) )
				continue;
			float z = 0.0f;
			if ( !landZ( x, y, &z ) )
				continue;
			ring.snapMax = std::max( ring.snapMax, double( std::fabs( z - w[v][2] ) ) );
			V[v].p[2] = ( z - xf.translation[2] ) / xf.scale;
			snappedV[v] = std::fabs( z - w[v][2] ) > 0.5f;
			ring.snapped++;
		}
	}

	/* FINALFIX THE SKIRT: a clip line is a T-junction -- the LAND's 32 a cell, or the next ring's coarser
	 * level, bends between this chunk's edge vertices, and the sliver between the two edges showed the clear
	 * colour (sky census on the Vanilla type: cracks along the block edge, top 13 px, and along ring edges).
	 * Every open edge lying on a clip line gets a wall, both faces: on the LAND (where landZ answers) from
	 * the higher of the two edges down past the lower, sampled at the LAND's rate; elsewhere straight down,
	 * deep enough for the next level's step. The wall's foot leans 2 units to the kept side so it never
	 * reads as ground over the cut. The outermost ring's rim gets none (nothing lies beyond it). */
	const bool skirtOuter = skirt >= 2;
	if ( skirt > 0 && upright && !out.isEmpty() ) {
		QHash<quint32, int> uses;
		auto key = []( quint16 a, quint16 b ) { return a < b ? ( quint32( a ) << 16 ) | b : ( quint32( b ) << 16 ) | a; };
		for ( const Triangle & t : out ) {
			uses[key( t.v1(), t.v2() )]++;
			uses[key( t.v2(), t.v3() )]++;
			uses[key( t.v3(), t.v1() )]++;
		}
		const float depth = 512.0f + 128.0f * float( std::max( 1, ring.level ) );
		const float sLand = kFvCell / 32.0f;
		auto clipLine = [&]( float x, float y ) {
			return onEdge( cut, x, y ) || onEdge( ours, x, y ) || ( skirtOuter && onEdge( outer, x, y ) );
		};
		QVector<Triangle> walls;
		const QVector<Triangle> base = out;
		for ( const Triangle & t : base ) {
			const quint16 id[3] = { t.v1(), t.v2(), t.v3() };
			for ( int k = 0; k < 3; k++ ) {
				const quint16 a = id[k], b = id[( k + 1 ) % 3], c = id[( k + 2 ) % 3];
				if ( uses.value( key( a, b ) ) != 1 )
					continue;
				const Vector3 wa = xf * V[a].p, wb = xf * V[b].p, wc = xf * V[c].p;
				const float dx = wb[0] - wa[0], dy = wb[1] - wa[1];
				const float len = std::sqrt( dx * dx + dy * dy );
				if ( len < 1.0f || ( std::fabs( dx ) > 0.5f && std::fabs( dy ) > 0.5f ) )
					continue;   // a clip line is axis-aligned
				/* FINALFIX: and a chunk's own open edge (a seam with the next chunk) that leaves a snapped
				 * corner: the snap lifted or dropped that corner to the LAND, the neighbour's seam edge runs to
				 * its own next vertex, and the tear between the two edges showed sky through the slanted
				 * rays of the Vanilla top view (2 px at x -16 beside the block, census 10-06). */
				const bool tear = ( size_t( a ) < snappedV.size() && snappedV[a] ) || ( size_t( b ) < snappedV.size() && snappedV[b] );
				if ( !tear && ( !clipLine( wa[0], wa[1] ) || !clipLine( wb[0], wb[1] ) ) )
					continue;
				// the kept side: toward the triangle's third corner, across the edge
				float nx = -dy / len, ny = dx / len;
				/* FINALFIX: the kept side from the clip rects first, the third corner only when they cannot tell
				 * -- a clipped sliver's third corner can lie on (or a float hair across) the line, and its wall
				 * then leaned INTO the cut (census g13: a 16-cell wall of commonwealth.16.-16.-16 counted as
				 * vanilla over ours, all 2048 of its triangles). */
				const float mx = 0.5f * ( wa[0] + wb[0] ), my = 0.5f * ( wa[1] + wb[1] );
				auto removed = [&]( float px, float py ) {
					return cut.has( px, py ) || ours.has( px, py ) || ( skirtOuter && !outer.has( px, py ) );
				};
				const bool goneP = removed( mx + 4.0f * nx, my + 4.0f * ny ), goneN = removed( mx - 4.0f * nx, my - 4.0f * ny );
				if ( goneP != goneN ? goneP : ( wc[0] - wa[0] ) * nx + ( wc[1] - wa[1] ) * ny < 0.0f ) {
					nx = -nx;
					ny = -ny;
				}
				float z0 = 0.0f;
				const bool land = landZ && landZ( 0.5f * ( wa[0] + wb[0] ), 0.5f * ( wa[1] + wb[1] ), &z0 );
				const int steps = land ? std::max( 1, int( std::ceil( len / sLand - 0.01f ) ) ) : 1;
				if ( V.size() + size_t( 2 * ( steps + 1 ) ) >= 65535 )
					break;
				std::vector<quint16> top, foot;
				for ( int s = 0; s <= steps; s++ ) {
					const float f = float( s ) / float( steps );
					FvVert o;
					o.p = V[a].p * ( 1.0f - f ) + V[b].p * f;
					o.uv = V[a].uv * ( 1.0f - f ) + V[b].uv * f;
					o.n = V[a].n * ( 1.0f - f ) + V[b].n * f;
					o.t = V[a].t * ( 1.0f - f ) + V[b].t * f;
					o.c = V[a].c * ( 1.0f - f ) + V[b].c * f;
					o.n.normalize();
					o.t.normalize();
					const Vector3 wp = xf * o.p;
					float hi = wp[2], lo = wp[2], lz = 0.0f;
					if ( land && landZ( wp[0], wp[1], &lz ) ) {
						hi = std::max( hi, lz );
						lo = std::min( lo, lz );
					}
					FvVert u = o, d = o;
					u.p[2] = ( hi - xf.translation[2] ) / xf.scale;
					d.p = Vector3( ( wp[0] + 2.0f * nx - xf.translation[0] ) / xf.scale,
						( wp[1] + 2.0f * ny - xf.translation[1] ) / xf.scale, ( lo - depth - xf.translation[2] ) / xf.scale );
					V.push_back( u );
					top.push_back( quint16( V.size() - 1 ) );
					V.push_back( d );
					foot.push_back( quint16( V.size() - 1 ) );
				}
				for ( int s = 0; s < steps; s++ ) {
					const quint16 t0 = top[size_t( s )], t1 = top[size_t( s + 1 )];
					const quint16 f0 = foot[size_t( s )], f1 = foot[size_t( s + 1 )];
					walls << Triangle( t0, t1, f1 ) << Triangle( t0, f1, f0 )
						<< Triangle( t0, f1, t1 ) << Triangle( t0, f0, f1 );
				}
				ring.skirtTris += 4 * steps;
			}
		}
		if ( !walls.isEmpty() ) {
			out += walls;
			changed = true;
		}
	}
	if ( !changed )
		return;

	// rewritten in the cell view's own full-precision layout (the shader, textures and alpha stay)
	BSVertexDesc nd( 0x0041B00000650407ULL );
	if ( hasC ) {
		nd.SetFlag( VertexFlags::VF_COLORS );
		nd.ResetAttributeOffsets( 130 );
	}
	const int stride = int( nd.GetVertexSize() );
	src.set<quint32>( iS, "Num Vertices", 0 );
	src.set<quint32>( iS, "Num Triangles", 0 );
	src.updateArraySize( iVD );
	src.updateArraySize( iTri );
	src.set<BSVertexDesc>( iS, "Vertex Desc", nd.Value() );
	src.set<quint32>( iS, "Num Vertices", quint32( V.size() ) );
	src.set<quint32>( iS, "Num Triangles", quint32( out.size() ) );
	src.set<quint32>( iS, "Data Size", quint32( V.size() * stride + out.size() * 6 ) );
	const QModelIndex iVD2 = src.getIndex( iS, "Vertex Data" );
	src.updateArraySize( iVD2 );
	for ( size_t v = 0; v < V.size(); v++ ) {
		const QModelIndex row = src.index( int( v ), 0, iVD2 );
		const FvVert & o = V[v];
		src.set<Vector3>( row, "Vertex", o.p );
		src.set<HalfVector2>( row, "UV", HalfVector2( o.uv ) );
		src.set<ByteVector3>( row, "Normal", ByteVector3( o.n ) );
		src.set<ByteVector3>( row, "Tangent", ByteVector3( o.t ) );
		const Vector3 b = Vector3::crossproduct( o.n, o.t );
		src.set<float>( row, "Bitangent X", b[0] );
		src.set<float>( row, "Bitangent Y", b[1] );
		src.set<float>( row, "Bitangent Z", b[2] );
		if ( hasC )
			src.set<ByteColor4>( row, "Vertex Colors", ByteColor4( FloatVector4( o.c ) ) );
	}
	const QModelIndex iTri2 = src.getIndex( iS, "Triangles" );
	src.updateArraySize( iTri2 );
	src.setArray<Triangle>( iTri2, out );
	if ( src.isNiBlock( iS, "BSMeshLODTriShape" ) ) {
		src.set<quint32>( iS, "LOD0 Size", quint32( out.size() ) );
		src.set<quint32>( iS, "LOD1 Size", 0 );
		src.set<quint32>( iS, "LOD2 Size", 0 );
	}
}

} // namespace

bool wwFarVanAppend( NifModel * nif, const QModelIndex & parent, const QString & ws, const float shift[3],
	std::vector<WwFarVanRing> & rings, const std::function<bool( float, float, float * )> & landZ,
	const std::function<bool()> & overBudget, QString * error )
{
	if ( !nif || !parent.isValid() ) {
		if ( error )
			*error = QStringLiteral( "no document to append the vanilla LOD to" );
		return false;
	}
	const QString wsl = ws.toLower();
	const Vector3 shiftV( shift[0], shift[1], shift[2] );
	/* FINALFIX: two passes -- the terrain (.btr) of EVERY ring first, then the objects (.bto) ring by ring
	 * under the budget. One pass let ring 1's objects spend the budget and the outer rings' ground was never
	 * built (sky below the horizon). A chunk is counted in the terrain pass; "any" is its .btr. */
	for ( int pass = 0; pass < 2; pass++ )
	for ( WwFarVanRing & r : rings ) {
		if ( r.x1 < r.x0 || r.y1 < r.y0 )
			continue;
		if ( pass == 1 )
			r.stopped = false;
		const FvRect outer = FvRect::cells( r.x0, r.y0, r.x1, r.y1 );
		const FvRect cut = r.noCut ? FvRect() : FvRect::cells( r.cx0, r.cy0, r.cx1, r.cy1 );
		const FvRect ours = r.noOursCut ? FvRect() : FvRect::cells( r.ux0, r.uy0, r.ux1, r.uy1 );
		const FvRect oursTrue = FvRect::cells( r.ux0, r.uy0, r.ux1, r.uy1 );
		const int L = std::max( 1, r.level );
		for ( int y = fvFloorDiv( r.y0, L ) * L; y <= r.y1 && !r.stopped; y += L ) {
			for ( int x = fvFloorDiv( r.x0, L ) * L; x <= r.x1; x += L ) {
				// which cells of this chunk the ring needs, and how many of them we cover
				int need = 0, mine = 0;
				for ( int cy = y; cy < y + L; cy++ )
					for ( int cx = x; cx < x + L; cx++ ) {
						const float mx = ( float( cx ) + 0.5f ) * kFvCell, my = ( float( cy ) + 0.5f ) * kFvCell;
						if ( !outer.has( mx, my ) || ( !r.noCut && FvRect::cells( r.cx0, r.cy0, r.cx1, r.cy1 ).has( mx, my ) ) )
							continue;
						need++;
						if ( oursTrue.has( mx, my ) )
							mine++;
					}
				if ( need == 0 )
					continue;
				if ( mine == need && !r.noOursCut ) {
					if ( pass == 0 )
						r.chunksOurs++;
					continue;
				}
				if ( overBudget && overBudget() ) {
					r.stopped = true;
					r.notes << QStringLiteral( "%1 NOT BUILT past chunk %2,%3: the far field's budget" )
						.arg( pass == 0 ? QStringLiteral( "terrain" ) : QStringLiteral( "objects" ) ).arg( x ).arg( y );
					break;
				}
				const QString stem = QStringLiteral( "%1.%2.%3.%4" ).arg( wsl ).arg( L ).arg( x ).arg( y );
				bool any = false;
				for ( int kind = pass; kind == pass; kind++ ) {
					const QString path = kind == 0
						? QStringLiteral( "meshes/terrain/%1/%2.btr" ).arg( wsl, stem )
						: QStringLiteral( "meshes/terrain/%1/objects/%2.bto" ).arg( wsl, stem );
					NifModel src;
					QString why;
					if ( !fvLoad( path, src, &why ) ) {
						if ( why != QLatin1String( "missing" ) )
							r.notes << QStringLiteral( "%1: %2" ).arg( path, why );
						continue;
					}
					any = true;
					/* FINALFIX: a .btr is chunk-local (0..4096 at Scale L, the root at 0): the game places it at
					 * the chunk's corner. Measured: Commonwealth.4.-20.24.btr 'Land' x 0..4096, Scale 4, no
					 * Translation -- so every terrain triangle fell outside the ring and was dropped. */
					if ( kind == 0 && src.getBlockCount() > 0 && src.blockInherits( src.getBlockIndex( 0 ), "NiAVObject" ) ) {
						const QModelIndex i0 = src.getBlockIndex( 0 );
						const Vector3 t0 = src.get<Vector3>( i0, "Translation" );
						src.set<Vector3>( i0, "Translation",
							t0 + Vector3( float( x ) * kFvCell, float( y ) * kFvCell, 0.0f ) );
					}
					// clip / filter, then flatten every node's transform into its shapes (shift taken off)
					QList<int> shapes;
					for ( int b = 0; b < src.getBlockCount(); b++ ) {
						const QModelIndex i = src.getBlockIndex( b );
						if ( src.blockInherits( i, "BSTriShape" ) )
							shapes << b;
					}
					/* FINALFIX: a .btr's water is the shape 'WATER' on a BSEffectShaderProperty (measured,
					 * Commonwealth.4.-20.20.btr: 'Land' + 'WATER'); drawn as the effect it was a white sheet.
					 * It goes to the cell's water like a BSWaterShaderProperty shape, and its edge is never
					 * snapped to the LAND (a sheet at the body's height). */
					std::vector<bool> water( size_t( shapes.size() ), false );
					for ( int k = 0; k < shapes.size(); k++ ) {
						const QModelIndex iSh = src.getBlockIndex( src.getLink( src.getBlockIndex( shapes[k] ), "Shader Property" ) );
						water[size_t( k )] = iSh.isValid() && ( src.isNiBlock( iSh, "BSWaterShaderProperty" )
							|| ( kind == 0 && src.isNiBlock( iSh, "BSEffectShaderProperty" ) ) );
					}
					const std::function<bool( float, float, float * )> noLand;
					std::vector<Transform> world;
					for ( int k = 0; k < shapes.size(); k++ ) {
						const QModelIndex i = src.getBlockIndex( shapes[k] );
						world.push_back( fvWorldOf( src, i ) );
						fvShape( src, i, kind == 0 ? FvTerrain : FvObjects, outer, cut, ours,
							water[size_t( k )] ? noLand : landZ, r,
							kind != 0 || water[size_t( k )] ? 0 : ( &r == &rings.back() ? 1 : 2 ) );
					}
					for ( int b = 0; b < src.getBlockCount(); b++ ) {
						const QModelIndex i = src.getBlockIndex( b );
						if ( src.blockInherits( i, "NiAVObject" ) && !src.blockInherits( i, "BSTriShape" ) )
							Transform().writeBack( &src, i );
					}
					for ( int k = 0; k < shapes.size(); k++ ) {
						Transform t = world[size_t( k )];
						t.translation = t.translation - shiftV;
						t.writeBack( &src, src.getBlockIndex( shapes[k] ) );
					}
					/* FINALFIX: the .btr 'Land' is Shader Type 18 (LOD landscape), which no cell-lit program
					 * takes (fo4_default.prog: "Shader Type != 18"); it fell to default.prog / sk_msn.prog and
					 * drew WHITE (WW_PROGRAM_CENSUS on g12_over_van: 91 default + 45 sk_msn, ours fo4_cellcsm).
					 * It is drawn the way our own tiles are (btdterrain.cpp): the default type with the
					 * model-space normal bit, no specular, LOD landscape in Shader Flags 2. */
					for ( int k = 0; k < shapes.size(); k++ ) {
						if ( kind != 0 || water[size_t( k )] )
							continue;
						const QModelIndex iSh = src.getBlockIndex(
							src.getLink( src.getBlockIndex( shapes[k] ), "Shader Property" ) );
						if ( !iSh.isValid() || !src.isNiBlock( iSh, "BSLightingShaderProperty" )
							|| src.get<quint32>( iSh, "Shader Type" ) != 18u )
							continue;
						src.set<quint32>( iSh, "Shader Type", 0u );
						src.set<quint32>( iSh, "Shader Flags 1", ( src.get<quint32>( iSh, "Shader Flags 1" ) | 0x1000u ) & ~0x1u );
						src.set<quint32>( iSh, "Shader Flags 2", src.get<quint32>( iSh, "Shader Flags 2" ) | 0x2u );
						r.landRetyped++;
					}
					// the cell's water draws a lighting-shader shape (renderer.cpp, fo4_water.prog): the effect goes
					for ( int k = 0; k < shapes.size(); k++ ) {
						if ( !water[size_t( k )] )
							continue;
						const QModelIndex iShape = src.getBlockIndex( shapes[k] );
						const QModelIndex iOld = src.getBlockIndex( src.getLink( iShape, "Shader Property" ) );
						if ( !iOld.isValid() || !src.isNiBlock( iOld, "BSEffectShaderProperty" ) )
							continue;
						const QModelIndex iShader = src.insertNiBlock( QStringLiteral( "BSLightingShaderProperty" ) );
						const QModelIndex iTextures = src.insertNiBlock( QStringLiteral( "BSShaderTextureSet" ) );
						src.setLink( iShader, "Texture Set", src.getBlockNumber( iTextures ) );
						src.set<uint>( iTextures, "Num Textures", 10 );
						src.updateArraySize( iTextures, "Textures" );
						const QModelIndex iTexArray = src.getIndex( iTextures, "Textures" );
						src.set<QString>( src.getIndex( iTexArray, 0 ), QStringLiteral( "#FFFFFFFF" ) );
						src.set<QString>( src.getIndex( iTexArray, 1 ), QStringLiteral( "#FFFF8080" ) );
						src.setLink( src.getBlockIndex( shapes[k] ), "Shader Property", src.getBlockNumber( iShader ) );
					}
					const QMap<qint32, qint32> map = src.moveAllNiBlocks( nif, false );
					const int root = map.value( 0, -1 );
					if ( root < 0 )
						continue;
					nif->set<QString>( nif->getBlockIndex( root ), "Name",
						QStringLiteral( "FarLOD v %1 %2" ).arg( r.tag, stem ) );
					addLink( nif, parent, QStringLiteral( "Children" ), root );
					for ( int k = 0; k < shapes.size(); k++ ) {
						const int b = map.value( shapes[k], -1 );
						if ( b < 0 )
							continue;
						QString name;
						if ( water[size_t( k )] ) {
							name = QStringLiteral( "FarLOD water v %1 %2 #%3" ).arg( r.tag, stem ).arg( k );
							r.waterBlocks.push_back( b );
						} else if ( kind == 0 ) {
							name = QStringLiteral( "FarLOD terrain v %1 %2 #%3" ).arg( r.tag, stem ).arg( k );
						} else {
							name = QStringLiteral( "FarLOD v %1 obj %2 #%3" ).arg( r.tag, stem ).arg( k );
						}
						nif->set<QString>( nif->getBlockIndex( b ), "Name", name );
						r.shapes++;
					}
				}
				if ( pass == 1 )
					continue;
				if ( !any )
					r.chunksNone++;
				else if ( mine > 0 && !r.noOursCut )
					r.chunksBoth++;
				else
					r.chunksVanilla++;
			}
		}
	}
	if ( error )
		error->clear();
	return true;
}
