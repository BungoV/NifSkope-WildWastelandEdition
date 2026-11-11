// lane AIRPERF1 round 5: the far field as the ray-traced GI traces it (rtfartrace.h)
#include "features/gi/rtfartrace.h"

#include "cellmesh.h"
#include "cellstreamrule.h"
#include "features/far-lod/cellfarhide.h"
#include "features/gi/probealbedo.h"
#include "features/gi/rttracesetrule.h"
#include "gl/glproperty.h"
#include "gl/glshape.h"
#include "model/nifmodel.h"

#include <QElapsedTimer>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <map>
#include <tuple>

namespace
{

// the tones a triangle takes when no map was read: the near land's stated dirt (sRGB 110, 100, 85, linear) for the
// terrain, the soup's mid grey for an object
const unsigned char kDirt[3] = { 40, 32, 23 };
const unsigned char kGrey[3] = { 128, 128, 128 };
// round 6: far water, open water's diffuse albedo 0.06 (linear x 255); its maps are normals and flow, not a colour
const unsigned char kWater[3] = { 15, 15, 15 };

struct Colour
{
	const DDSTexture16 * tex = nullptr;
	const unsigned char * tone = kGrey;
	qint64 flat = 0;

	void at( float u, float v, std::vector<unsigned char> & out )
	{
		if ( !tex ) {
			flat++;
			out.insert( out.end(), tone, tone + 3 );
			return;
		}
		static const float one[3] = { 1.0f, 1.0f, 1.0f };
		float lin[3] = { 0.5f, 0.5f, 0.5f };
		// mip 3 of a map of at most 512 texels: the mean of an 8 x 8 block, a far triangle's footprint or less
		ProbeAlbedo::sampleLod( tex, nullptr, u, v, 3.0f, 0.0f, one, lin );
		for ( int k = 0; k < 3; k++ )
			out.push_back( static_cast<unsigned char>( std::lround( std::clamp( lin[k], 0.0f, 1.0f ) * 255.0f ) ) );
	}
};

void span( RtFarShape & f )
{
	bool first = true;
	for ( const RtFarShape::Run & r : f.runs ) {
		const int x = int( r.cell >> 32 ), y = int( qint32( quint32( r.cell & 0xFFFFFFFFll ) ) );
		if ( first ) {
			f.span[0] = f.span[2] = x;
			f.span[1] = f.span[3] = y;
			first = false;
			continue;
		}
		f.span[0] = std::min( f.span[0], x );
		f.span[1] = std::min( f.span[1], y );
		f.span[2] = std::max( f.span[2], x );
		f.span[3] = std::max( f.span[3], y );
	}
}

} // namespace

quint64 rtFarShapeSig( const NifModel * nif, Shape * sh )
{
	if ( !nif || !sh )
		return 0;
	const QSharedPointer<CellMesh> cm = cellMeshFor( nif, sh->index() );
	if ( cm && cm->inst )
		return cm->inst->cmds.isEmpty() ? 0 : ( cm->inst->id * 2 + 1 ) | ( quint64( 1 ) << 63 );
	const quint64 nV = quint64( cm ? cm->verts.size() : sh->verts.size() ), nT = quint64( cm ? cm->triangles.size() : sh->triangles.size() );
	if ( !nV || !nT )
		return 0;
	return ( ( nV << 32 ) ^ nT ^ ( quint64( qHash( sh->getName() ) ) << 20 ) ) & ~( quint64( 1 ) << 63 );
}

std::shared_ptr<const RtFarShape> rtFarShapeBuild( const NifModel * nif, Shape * sh, int src, ProbeAlbedo & alb, bool flatRed )
{
	QElapsedTimer clock;
	clock.start();
	auto out = std::make_shared<RtFarShape>();
	RtFarShape & f = *out;
	f.src = src;
	f.sig = rtFarShapeSig( nif, sh );
	// a far object is a shell without an inside: no ray may read its back as "inside a wall"; a water sheet the same
	f.twoSided = src != 0;
	if ( !f.sig )
		return out;
	const QSharedPointer<CellMesh> cm = cellMeshFor( nif, sh->index() );
	const Transform w = sh->restWorldTrans();
	BSShaderLightingProperty * bs = sh->wwShaderLighting();
	const QString texName = bs ? bs->fileName( 0 ) : QString();
	Colour col;
	col.tone = src == 0 ? kDirt : ( src == 2 ? kWater : kGrey );
	if ( !flatRed && src != 2 && !texName.isEmpty() )
		col.tex = alb.loadFine( texName );
	const bool tested = sh->wwAlphaTested() || ( cm && cm->inst && cm->inst->alphaTested );
	if ( src == 1 && tested && !texName.isEmpty() ) {
		int mw = 0, mh = 0, minA = 255;
		std::vector<unsigned char> a;
		const int thr = std::clamp( int( std::lround( sh->wwAlphaThreshold() * 255.0f ) ), 1, 255 );
		if ( alb.alphaBytes( texName, &mw, &mh, &a, &minA ) && minA < thr && mw > 0 && mh > 0 ) {
			f.mask = std::make_shared<const std::vector<unsigned char>>( std::move( a ) );
			f.maskW = mw;
			f.maskH = mh;
			f.thr = thr;
			f.maskName = texName.toLower().toStdString();
		}
	}
	const bool withUv = bool( f.mask );

	if ( cm && cm->inst ) {
		// ---- an instanced bucket: each sub-mesh once, in its own frame; a run a traced row
		const CellMeshInst & I = *cm->inst;
		const int nV = int( I.vbo.size() / 36 );
		const char * vb = I.vbo.constData();
		std::map<std::tuple<quint32, quint32, qint32>, std::pair<int, int>> subs;   // -> first triangle, count
		for ( const CellMeshInst::Cmd & c : I.cmds ) {
			if ( c.count < 3 || !c.instanceCount )
				continue;
			const auto key = std::make_tuple( c.firstIndex, c.count, c.baseVertex );
			auto it = subs.find( key );
			if ( it == subs.end() ) {
				const int first = int( f.tris.size() / 9 );
				for ( quint32 i = 0; i + 2 < c.count; i += 3 ) {
					int idx[3];
					bool ok = true;
					for ( int k = 0; k < 3; k++ ) {
						const qint64 e = qint64( c.firstIndex ) + i + k;
						if ( e >= I.ebo.size() ) {
							ok = false;
							break;
						}
						idx[k] = int( I.ebo[int( e )] ) + c.baseVertex;
						ok = ok && idx[k] >= 0 && idx[k] < nV;
					}
					if ( !ok || idx[0] == idx[1] || idx[1] == idx[2] || idx[0] == idx[2] )
						continue;
					float p[9], t[6];
					for ( int k = 0; k < 3; k++ ) {
						std::memcpy( p + k * 3, vb + qint64( idx[k] ) * 36, 12 );
						std::memcpy( t + k * 2, vb + qint64( idx[k] ) * 36 + 28, 8 );
					}
					f.tris.insert( f.tris.end(), p, p + 9 );
					col.at( ( t[0] + t[2] + t[4] ) / 3.0f, ( t[1] + t[3] + t[5] ) / 3.0f, f.alb );
					if ( withUv )
						f.uv.insert( f.uv.end(), t, t + 6 );
				}
				it = subs.emplace( key, std::make_pair( first, int( f.tris.size() / 9 ) - first ) ).first;
			}
			if ( it->second.second <= 0 )
				continue;
			for ( quint32 n = 0; n < c.instanceCount; n++ ) {
				const qint64 r = qint64( c.baseInstance ) + n;
				if ( ( r + 1 ) * 16 > I.inst.size() )
					break;
				const unsigned rowStep = r < I.rowStep.size() ? unsigned( I.rowStep[int( r )] ) : 0u;
				if ( !rttracesetrule::rowTraced( rowStep ) )
					continue;
				const float * R = I.inst.constData() + r * 16;
				// world = the shape's own transform x the row
				const auto place = [&w, R]( float x, float y, float z ) {
					return w * Vector3( R[0] * x + R[1] * y + R[2] * z + R[3], R[4] * x + R[5] * y + R[6] * z + R[7],
						R[8] * x + R[9] * y + R[10] * z + R[11] );
				};
				const Vector3 o = place( 0, 0, 0 ), ax = place( 1, 0, 0 ) - o, ay = place( 0, 1, 0 ) - o, az = place( 0, 0, 1 ) - o;
				RtFarShape::Xf x;
				for ( int k = 0; k < 3; k++ ) {
					x.m[k * 4 + 0] = ax[k];
					x.m[k * 4 + 1] = ay[k];
					x.m[k * 4 + 2] = az[k];
					x.m[k * 4 + 3] = o[k];
				}
				const float det = ax[0] * ( ay[1] * az[2] - ay[2] * az[1] ) - ay[0] * ( ax[1] * az[2] - ax[2] * az[1] )
					+ az[0] * ( ax[1] * ay[2] - ax[2] * ay[1] );
				x.flip = det < 0.0f;
				x.mirrorU = R[15] > 1.5f;
				x.uMid = R[12];
				RtFarShape::Run run;
				run.cell = r < I.rowCell.size() ? I.rowCell[int( r )]
					: CellFarHide::cellKey( cellstreamrule::cellOf( double( o[0] ) ), cellstreamrule::cellOf( double( o[1] ) ) );
				run.ref = r < I.rowRef.size() ? I.rowRef[int( r )] : 0;
				run.first = it->second.first;
				run.count = it->second.second;
				run.xf = int( f.xf.size() );
				// round 6: the ladder's ring row is a carded tree's authored far mesh
				run.kind = src == 1 && rttracesetrule::rowCard( rowStep ) ? quint8( 3 ) : quint8( src );
				f.xf.push_back( x );
				f.runs.push_back( run );
			}
		}
	} else {
		// ---- arrays (beside the document, or the shape's own): world triangles in runs a cell long
		const QVector<Vector3> & V = cm ? cm->verts : sh->verts;
		const QVector<Triangle> & T = cm ? cm->triangles : sh->triangles;
		static const QVector<Vector2> none;
		const QVector<Vector2> & UV = cm ? cm->coords : ( sh->coords.isEmpty() ? none : sh->coords.at( 0 ) );
		const int nV = int( V.size() );
		const bool haveUv = UV.size() >= nV;
		std::vector<Vector3> P;
		P.reserve( size_t( nV ) );
		for ( const Vector3 & v : V )
			P.push_back( w * v );
		std::vector<std::pair<qint64, int>> order;   // cell, triangle
		order.reserve( size_t( T.size() ) );
		for ( int i = 0; i < int( T.size() ); i++ ) {
			const int i0 = T[i][0], i1 = T[i][1], i2 = T[i][2];
			if ( i0 == i1 || i1 == i2 || i0 == i2 || i0 >= nV || i1 >= nV || i2 >= nV )
				continue;
			const Vector3 & a = P[size_t( i0 )], & b = P[size_t( i1 )], & c = P[size_t( i2 )];
			order.emplace_back( CellFarHide::cellKey( cellstreamrule::cellOf( double( a[0] + b[0] + c[0] ) / 3.0 ),
				cellstreamrule::cellOf( double( a[1] + b[1] + c[1] ) / 3.0 ) ), i );
		}
		std::stable_sort( order.begin(), order.end(), []( const std::pair<qint64, int> & a, const std::pair<qint64, int> & b ) {
			return a.first < b.first;
		} );
		f.tris.reserve( order.size() * 9 );
		f.alb.reserve( order.size() * 3 );
		for ( const std::pair<qint64, int> & o : order ) {
			const Triangle & tr = T[o.second];
			float t[6] = { 0, 0, 0, 0, 0, 0 };
			for ( int k = 0; k < 3; k++ ) {
				const Vector3 & p = P[size_t( tr[k] )];
				f.tris.push_back( p[0] );
				f.tris.push_back( p[1] );
				f.tris.push_back( p[2] );
				if ( haveUv ) {
					t[k * 2 + 0] = UV[tr[k]][0];
					t[k * 2 + 1] = UV[tr[k]][1];
				}
			}
			if ( haveUv ) {
				col.at( ( t[0] + t[2] + t[4] ) / 3.0f, ( t[1] + t[3] + t[5] ) / 3.0f, f.alb );
			} else {
				col.flat++;
				f.alb.insert( f.alb.end(), col.tone, col.tone + 3 );
			}
			if ( withUv )
				f.uv.insert( f.uv.end(), t, t + 6 );
			const int at = int( f.tris.size() / 9 ) - 1;
			if ( f.runs.empty() || f.runs.back().cell != o.first ) {
				RtFarShape::Run run;
				run.cell = o.first;
				run.first = at;
				run.kind = quint8( src );
				f.runs.push_back( run );
			}
			f.runs.back().count++;
		}
	}
	f.flat = col.flat;
	f.tris.shrink_to_fit();
	f.alb.shrink_to_fit();
	f.uv.shrink_to_fit();
	span( f );
	f.ms = double( clock.nsecsElapsed() ) / 1e6;
	return out;
}
