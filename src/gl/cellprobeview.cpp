// lane PROBEVIEW1: the PRTP band's Pass overlays (src/gl/cellprobeview.h)

#include "cellprobeview.h"

#include "gl/celllights.h"
#include "gl/glscene.h"

#include <QFile>
#include <QHash>

#include <algorithm>
#include <cmath>

namespace {

struct PvState
{
	QHash<const void *, WwCellProbeView> views;
	QHash<const void *, int> picked;	// the probe picked per document, -1 none
	int pin = -2;				// WW_CELL_PV_PROBE, -2 unread, -1 none
	QString lastDump;
};

PvState & pv()
{
	static PvState s;
	if ( s.pin == -2 ) {
		const QByteArray p = qgetenv( "WW_CELL_PV_PROBE" ).trimmed();
		bool ok = false;
		const int i = p.toInt( &ok );
		s.pin = ok ? i : -1;
	}
	return s;
}

constexpr float kBoxHalf = 18.0f;	// the lit box, around the largest marker (16)
const float kAxes[6][3] = { { 1, 0, 0 }, { -1, 0, 0 }, { 0, 1, 0 }, { 0, -1, 0 }, { 0, 0, 1 }, { 0, 0, -1 } };

// the GI pass's curve (cell_lights.glsl cellPassOut): clamp, then 1 / 2.2
float shown( float v )
{
	return std::pow( std::min( std::max( v, 0.0f ), 1.0f ), 1.0f / 2.2f );
}

void drawChunked( Scene * scene, const std::vector<Vector3> & p, const std::vector<FloatVector4> & c, bool lines )
{
	constexpr size_t kChunk = 65532;	// a multiple of 6 and of 2
	for ( size_t i = 0; i < p.size(); i += kChunk ) {
		const size_t n = std::min( kChunk, p.size() - i );
		if ( lines )
			scene->drawLines( p.data() + i, n, c.data() + i );
		else
			scene->drawTriangles( p.data() + i, n, c.data() + i, true );
	}
}

} // namespace

void wwCellProbeViewPublish( const void * nif, const WwCellProbeView & v )
{
	PvState & s = pv();
	s.views.insert( nif, v );
	s.picked.insert( nif, s.pin >= 0 && size_t( s.pin ) * 21 < v.probes.size() ? s.pin : -1 );
}

const WwCellProbeView * wwCellProbeViewFor( const void * nif )
{
	PvState & s = pv();
	auto it = s.views.constFind( nif );
	return it == s.views.constEnd() ? nullptr : &*it;
}

bool wwCellProbeViewPick( Scene * scene, const float origin[3], const float dir[3] )
{
	if ( !scene || !scene->nifModel || wwCellPassFor( scene->nifModel ) <= 0 )
		return false;
	const WwCellProbeView * v = wwCellProbeViewFor( scene->nifModel );
	if ( !v || !v->probesShown )
		return false;
	int best = -1;
	float bestT = 1e30f;
	for ( size_t i = 0; i * 21 + 2 < v->probes.size(); i++ ) {
		const float * c = &v->probes[i * 21];
		// the ray against the lit box (slabs)
		float t0 = 0.0f, t1 = 1e30f;
		bool hit = true;
		for ( int a = 0; a < 3 && hit; a++ ) {
			const float lo = c[a] - kBoxHalf - origin[a], hi = c[a] + kBoxHalf - origin[a];
			if ( std::fabs( dir[a] ) < 1e-9f ) {
				hit = lo <= 0.0f && hi >= 0.0f;
				continue;
			}
			float ta = lo / dir[a], tb = hi / dir[a];
			if ( ta > tb )
				std::swap( ta, tb );
			t0 = std::max( t0, ta );
			t1 = std::min( t1, tb );
			hit = t0 <= t1;
		}
		if ( hit && t0 < bestT ) {
			bestT = t0;
			best = int( i );
		}
	}
	PvState & s = pv();
	const int was = s.picked.value( scene->nifModel, -1 );
	s.picked.insert( scene->nifModel, best );
	return best >= 0 || was >= 0;	// a miss clears a pick (and is then an ordinary click)
}

void wwCellProbeViewDraw( Scene * scene, const Transform & viewTrans )
{
	if ( !scene || !scene->nifModel || scene->selecting )
		return;
	const int pass = wwCellPassFor( scene->nifModel );
	if ( pass <= 0 )
		return;
	const WwCellProbeView * v = wwCellProbeViewFor( scene->nifModel );
	if ( !v )
		return;
	PvState & s = pv();
	const int picked = s.picked.value( scene->nifModel, -1 );
	std::vector<Vector3> tp;
	std::vector<FloatVector4> tc;
	size_t tiles = 0, boxes = 0;
	// the surfels, as tiles a hair off their surface
	if ( pass >= 3 ) {
		// a splat 0.6 of the surfel cell: the surfels read apart, and a curved surface keeps few cards in the air
		const float h = v->surfelCell * 0.3f;
		// WW_CELL_PV_ID=1: each tile carries its surfel's index + 1 as 24-bit color (the gate's exact map)
		static const bool idColors = qEnvironmentVariableIntValue( "WW_CELL_PV_ID" ) == 1;
		for ( size_t i = 0; i + 11 < v->surfels.size(); i += 12 ) {
			const float * f = &v->surfels[i];
			const Vector3 n( f[3], f[4], f[5] );
			const Vector3 up = std::fabs( n[2] ) < 0.9f ? Vector3( 0, 0, 1 ) : Vector3( 1, 0, 0 );
			Vector3 t = Vector3::crossproduct( n, up );
			t.normalize();
			const Vector3 b = Vector3::crossproduct( n, t );
			const Vector3 c = Vector3( f[0], f[1], f[2] ) + n * 0.5f;
			const Vector3 q[4] = { c - t * h - b * h, c + t * h - b * h, c + t * h + b * h, c - t * h + b * h };
			FloatVector4 col;
			const quint32 id = quint32( i / 12 + 1 );
			if ( idColors )
				col = FloatVector4( float( id & 255 ) / 255.0f, float( ( id >> 8 ) & 255 ) / 255.0f, float( ( id >> 16 ) & 255 ) / 255.0f, 1.0f );
			else if ( pass == 3 )
				col = FloatVector4( shown( f[6] ), shown( f[7] ), shown( f[8] ), 1.0f );
			else
				col = FloatVector4( shown( f[9] * 0.31830989f ), shown( f[10] * 0.31830989f ), shown( f[11] * 0.31830989f ), 1.0f );
			for ( int k : { 0, 1, 2, 0, 2, 3 } ) {
				tp.push_back( q[k] );
				tc.push_back( col );
			}
			tiles++;
		}
	}
	// the probes, each face its own value on that axis
	if ( v->probesShown ) {
		for ( size_t i = 0; i * 21 + 20 < v->probes.size(); i++ ) {
			const float * p = &v->probes[i * 21];
			const Vector3 c( p[0], p[1], p[2] );
			for ( int a = 0; a < 6; a++ ) {
				const Vector3 n( kAxes[a][0], kAxes[a][1], kAxes[a][2] );
				const Vector3 u = a < 2 ? Vector3( 0, 1, 0 ) : Vector3( 1, 0, 0 );
				const Vector3 w = Vector3::crossproduct( n, u );
				const Vector3 m = c + n * kBoxHalf;
				const Vector3 q[4] = { m - u * kBoxHalf - w * kBoxHalf, m + u * kBoxHalf - w * kBoxHalf,
					m + u * kBoxHalf + w * kBoxHalf, m - u * kBoxHalf + w * kBoxHalf };
				FloatVector4 col;
				if ( pass == 2 ) {
					const float k = i * 6 + 5 < v->probeSky.size() ? shown( v->probeSky[i * 6 + size_t( a )] ) : 0.0f;
					col = FloatVector4( k, k, k, 1.0f );
				} else {
					const float * e = p + 3 + a * 3;
					col = FloatVector4( shown( e[0] * 0.31830989f ), shown( e[1] * 0.31830989f ), shown( e[2] * 0.31830989f ), 1.0f );
				}
				for ( int k : { 0, 1, 2, 0, 2, 3 } ) {
					tp.push_back( q[k] );
					tc.push_back( col );
				}
			}
			boxes++;
		}
	}
	// the picked probe's links
	std::vector<Vector3> lp;
	std::vector<FloatVector4> lc;
	QString linkList;
	int nLinks = 0;
	if ( picked >= 0 && size_t( picked ) * 21 + 2 < v->probes.size() && size_t( picked ) + 1 < v->linkStart.size() ) {
		const float * p = &v->probes[size_t( picked ) * 21];
		const Vector3 c( p[0], p[1], p[2] );
		const FloatVector4 green( 0.1f, 1.0f, 0.2f, 1.0f ), yellow( 1.0f, 0.9f, 0.1f, 1.0f );
		for ( int k = v->linkStart[size_t( picked )]; k < v->linkStart[size_t( picked ) + 1]; k++ ) {
			const int si = v->links[size_t( k )];
			if ( si < 0 || size_t( si ) * 12 + 2 >= v->surfels.size() )
				continue;
			const float * f = &v->surfels[size_t( si ) * 12];
			lp.push_back( c );
			lp.push_back( Vector3( f[0], f[1], f[2] ) );
			lc.push_back( green );
			lc.push_back( green );
			linkList += QStringLiteral( " %1" ).arg( si );
			nLinks++;
		}
		// the picked box's edges
		const float e = kBoxHalf + 1.0f;
		for ( int a = 0; a < 3; a++ )
			for ( int s1 = -1; s1 <= 1; s1 += 2 )
				for ( int s2 = -1; s2 <= 1; s2 += 2 ) {
					Vector3 d0, d1;
					d0[a] = -e;
					d1[a] = e;
					d0[( a + 1 ) % 3] = d1[( a + 1 ) % 3] = s1 * e;
					d0[( a + 2 ) % 3] = d1[( a + 2 ) % 3] = s2 * e;
					lp.push_back( c + d0 );
					lp.push_back( c + d1 );
					lc.push_back( yellow );
					lc.push_back( yellow );
				}
	}
	scene->loadModelViewMatrix( viewTrans );
	glEnable( GL_DEPTH_TEST );
	glDepthFunc( GL_LEQUAL );
	glDepthMask( GL_TRUE );
	glDisable( GL_CULL_FACE );
	glEnable( GL_POLYGON_OFFSET_FILL );
	glPolygonOffset( -1.0f, -1.0f );
	if ( !tp.empty() )
		drawChunked( scene, tp, tc, false );
	glDisable( GL_POLYGON_OFFSET_FILL );
	if ( !lp.empty() ) {
		glDepthFunc( GL_ALWAYS );	// the links show through the walls between the probe and its surfels
		drawChunked( scene, lp, lc, true );
	}
	glDepthFunc( GL_LESS );
	glDisable( GL_BLEND );
	static const QString dump = QString::fromLocal8Bit( qgetenv( "WW_CELL_PV_DUMP" ) );
	if ( !dump.isEmpty() ) {
		QString o = QStringLiteral( "pass=%1 tiles=%2 boxes=%3 probe=%4 links=%5\n" ).arg( pass ).arg( tiles ).arg( boxes )
			.arg( picked ).arg( nLinks );
		if ( picked >= 0 ) {
			const float * p = &v->probes[size_t( picked ) * 21];
			o += QStringLiteral( "probepos=%1,%2,%3\nlinked=%4\n" ).arg( double( p[0] ), 0, 'f', 2 ).arg( double( p[1] ), 0, 'f', 2 )
				.arg( double( p[2] ), 0, 'f', 2 ).arg( linkList.trimmed() );
		}
		if ( o != s.lastDump ) {
			QFile f( dump );
			if ( f.open( QIODevice::WriteOnly | QIODevice::Truncate ) ) {
				f.write( o.toUtf8() );
				s.lastDump = o;
			}
		}
	}
}
