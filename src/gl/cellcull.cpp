/*! Lane SUNCELL1: the cell view's per-placement culling (see cellcull.h). */
#include "cellcull.h"

#include "gl/glscene.h"
#include "gl/glshape.h"
#include "gl/renderer.h"

#include <QByteArray>
#include <QHash>
#include <QSettings>

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace
{

struct Counts
{
	std::int64_t shapes = 0, runs = 0, drawn = 0, culled = 0, calls = 0, tris = 0, trisAll = 0;
	std::int64_t cDrawn[3] = { 0, 0, 0 }, cCulled[3] = { 0, 0, 0 };
	bool operator==( const Counts & o ) const
	{
		for ( int i = 0; i < 3; i++ )
			if ( cDrawn[i] != o.cDrawn[i] || cCulled[i] != o.cCulled[i] )
				return false;
		return shapes == o.shapes && runs == o.runs && drawn == o.drawn && culled == o.culled && calls == o.calls
			&& tris == o.tris && trisAll == o.trisAll;
	}
};

struct CullState
{
	bool loaded = false, on = false, pinned = false;
	int red = 0;
	QHash<const void *, QHash<int, std::vector<WwCullRun>>> docs;
	Counts cur, last;
	bool any = false, printed = false;
};

CullState & st()
{
	static CullState s;
	if ( !s.loaded ) {
		s.loaded = true;
		const QByteArray pin = qgetenv( "WW_CELL_CULL" ).trimmed();
		if ( !pin.isEmpty() ) {
			s.pinned = true;
			s.on = pin != "0";
		} else {
			s.on = QSettings().value( QStringLiteral( "WW/CellCull" ), false ).toBool();
		}
		s.red = qgetenv( "WW_CELL_CULL_RED" ).trimmed() == "casters" ? 1 : 0;
	}
	return s;
}

//! the camera's four side planes in view space (Gribb-Hartmann from the column-major projection), normalised
void cameraPlanes( Scene * scene, float pl[4][4] )
{
	const auto & pm = scene->renderer->globalUniforms->projectionMatrix;	// pm[column][row]
	for ( int i = 0; i < 4; i++ ) {
		const int axis = i >> 1;		// 0 = x, 1 = y
		const float sg = ( i & 1 ) ? -1.0f : 1.0f;
		float l = 0.0f;
		for ( int c = 0; c < 4; c++ ) {
			pl[i][c] = pm[c][3] + sg * pm[c][axis];
			if ( c < 3 )
				l += pl[i][c] * pl[i][c];
		}
		l = std::sqrt( l );
		if ( l > 0.0f )
			for ( int c = 0; c < 4; c++ )
				pl[i][c] /= l;
	}
}

bool insidePlanes( const float pl[4][4], const Vector3 & v, float r )
{
	for ( int i = 0; i < 4; i++ )
		if ( pl[i][0] * v[0] + pl[i][1] * v[1] + pl[i][2] * v[2] + pl[i][3] < -r )
			return false;
	return true;
}

//! the visible runs into (first, count) pairs, adjacent ones merged; returns the culled count
template <class Visible>
std::int64_t collect( const std::vector<WwCullRun> & runs, std::int64_t numTris, Visible vis,
	std::vector<std::uint32_t> & out, std::int64_t & drawnTris )
{
	out.clear();
	std::int64_t culled = 0;
	drawnTris = 0;
	for ( const WwCullRun & r : runs ) {
		if ( std::int64_t( r.first ) >= numTris )
			break;
		const std::uint32_t cnt = std::uint32_t( std::min<std::int64_t>( r.count, numTris - std::int64_t( r.first ) ) );
		if ( !vis( r ) ) {
			culled++;
			continue;
		}
		drawnTris += cnt;
		if ( !out.empty() && out[out.size() - 2] + out[out.size() - 1] == r.first )
			out[out.size() - 1] += cnt;
		else {
			out.push_back( r.first );
			out.push_back( cnt );
		}
	}
	return culled;
}

} // namespace

void wwCellCullBegin( const void * nif )
{
	st().docs.remove( nif );
}

void wwCellCullShape( const void * nif, int block, std::vector<WwCullRun> && runs )
{
	if ( runs.size() < 2 )
		return;	// one run: nothing to cull inside the shape
	st().docs[nif].insert( block, std::move( runs ) );
}

const std::vector<WwCullRun> * wwCellCullRuns( const void * nif, int block )
{
	CullState & s = st();
	auto d = s.docs.constFind( nif );
	if ( d == s.docs.constEnd() )
		return nullptr;
	auto it = d->constFind( block );
	return it == d->constEnd() ? nullptr : &( *it );
}

bool wwCellCullOn()
{
	return st().on;
}

void wwCellCullSetOn( bool on )
{
	CullState & s = st();
	if ( s.pinned )
		return;
	s.on = on;
	QSettings().setValue( QStringLiteral( "WW/CellCull" ), on );
}

int wwCellCullRed()
{
	return st().red;
}

bool wwCellCullCamera( Scene * scene, const Shape * sh, int block, std::int64_t numTris,
	std::vector<std::uint32_t> & out )
{
	CullState & s = st();
	if ( !scene || !sh || !scene->renderer || numTris <= 0 || s.docs.isEmpty() )
		return false;
	const std::vector<WwCullRun> * runs = wwCellCullRuns( scene->nifModel, block );
	if ( !runs )
		return false;
	if ( !s.on ) {	// counted all the same, so the off frame's draw calls stand beside the on frame's
		s.any = true;
		s.cur.shapes++;
		s.cur.runs += std::int64_t( runs->size() );
		s.cur.drawn += std::int64_t( runs->size() );
		s.cur.calls++;
		s.cur.tris += numTris;
		s.cur.trisAll += numTris;
		return false;
	}
	float pl[4][4];
	cameraPlanes( scene, pl );
	const Transform & vt = sh->viewTrans();
	std::int64_t drawnTris = 0;
	const std::int64_t culled = collect( *runs, numTris, [&]( const WwCullRun & r ) {
		return insidePlanes( pl, vt * r.center, r.radius * std::fabs( vt.scale ) );
	}, out, drawnTris );
	s.any = true;
	s.cur.shapes++;
	s.cur.runs += std::int64_t( runs->size() );
	s.cur.culled += culled;
	s.cur.drawn += std::int64_t( runs->size() ) - culled;
	s.cur.calls += std::int64_t( out.size() / 2 );
	s.cur.tris += drawnTris;
	s.cur.trisAll += numTris;
	return true;
}

bool wwCellCullCaster( Scene * scene, const Shape * sh, int block, int cascade, const float * m,
	std::vector<std::uint32_t> & out )
{
	CullState & s = st();
	if ( !s.on || !scene || !sh || !scene->renderer || !m )
		return false;
	const std::vector<WwCullRun> * runs = wwCellCullRuns( scene->nifModel, block );
	if ( !runs )
		return false;
	const Transform & vt = sh->viewTrans();
	std::int64_t drawnTris = 0, culled = 0;
	if ( s.red == 1 ) {	// RED: the camera's frustum decides which casters the cascade gets
		float pl[4][4];
		cameraPlanes( scene, pl );
		culled = collect( *runs, std::int64_t( sh->triangles.size() ), [&]( const WwCullRun & r ) {
			return insidePlanes( pl, vt * r.center, r.radius * std::fabs( vt.scale ) );
		}, out, drawnTris );
	} else {
		// the cascade's own window: light-space x and y in [-1, 1] (no depth test: a caster toward the sun still casts)
		float lx = 0.0f, ly = 0.0f;
		for ( int c = 0; c < 3; c++ ) {
			lx += m[c * 4 + 0] * m[c * 4 + 0];
			ly += m[c * 4 + 1] * m[c * 4 + 1];
		}
		lx = std::sqrt( lx );
		ly = std::sqrt( ly );
		culled = collect( *runs, std::int64_t( sh->triangles.size() ), [&]( const WwCullRun & r ) {
			const Vector3 v = vt * r.center;
			const float rv = r.radius * std::fabs( vt.scale );
			const float x = m[0] * v[0] + m[4] * v[1] + m[8] * v[2] + m[12];
			const float y = m[1] * v[0] + m[5] * v[1] + m[9] * v[2] + m[13];
			return std::fabs( x ) <= 1.0f + rv * lx && std::fabs( y ) <= 1.0f + rv * ly;
		}, out, drawnTris );
	}
	if ( cascade >= 0 && cascade < 3 ) {
		s.any = true;
		s.cur.cDrawn[cascade] += std::int64_t( runs->size() ) - culled;
		s.cur.cCulled[cascade] += culled;
	}
	return true;
}

QString wwCellCullSummary()
{
	const Counts & c = st().last;
	return QStringLiteral( "cell cull: " ) + ( st().on ? QStringLiteral( "on" ) : QStringLiteral( "off" ) )
		+ QStringLiteral( " camera shapes=%1 runs=%2 drawn=%3 culled=%4 calls=%5 tris=%6/%7"
		" | casters c0 %8/%9 c1 %10/%11 c2 %12/%13 (drawn/culled)%14" )
		.arg( c.shapes ).arg( c.runs ).arg( c.drawn ).arg( c.culled ).arg( c.calls ).arg( c.tris ).arg( c.trisAll )
		.arg( c.cDrawn[0] ).arg( c.cCulled[0] ).arg( c.cDrawn[1] ).arg( c.cCulled[1] ).arg( c.cDrawn[2] )
		.arg( c.cCulled[2] ).arg( st().red == 1 ? QStringLiteral( " RED=casters" ) : QString() );
}

void wwCellCullFrame()
{
	CullState & s = st();
	if ( !s.any ) {
		if ( !s.printed && !s.docs.isEmpty() ) {
			std::fprintf( stderr, "cell cull: %s\n", s.on ? "on, no runs drawn this frame" : "off, no runs drawn this frame" );
			s.printed = true;
		}
		return;
	}
	if ( !( s.cur == s.last ) || !s.printed ) {
		s.last = s.cur;
		std::fprintf( stderr, "%s\n", wwCellCullSummary().toLocal8Bit().constData() );
		s.printed = true;
	}
	s.cur = Counts();
	s.any = false;
}
