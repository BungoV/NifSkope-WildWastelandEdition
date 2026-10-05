#include "cellgrass.h"


#include "esmdata.h"
#include "gl/glscene.h"
#include "gl/lookdevstage.h"
#include "gl/renderer.h"
#include "harnesswindow.h"

#include <QDebug>
#include <QElapsedTimer>
#include <QFile>
#include <QHash>
#include <QPair>
#include <QSet>
#include <QSettings>
#include <QTextStream>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <random>

#include "esmfile.hpp"

// lane GRASSMB1: the game's grass placement (cellgrass.h). Every float below is float32 in the engine's order;
// no contraction into fused multiply-adds, or a blade moves by one half-float step and the rebuild gate fails.
#if defined( __clang__ )
#pragma clang fp contract( off )
#elif defined( __GNUC__ )
#pragma GCC optimize( "fp-contract=off" )
#endif

namespace {

constexpr unsigned int GRUP = 0x50555247U;	// as esmdata.cpp spells the group tag

constexpr const char * kGrassKey = "WW/CellGrass";

struct GrassState
{
	bool init = false, on = false, pinned = false, redSeed = false;
	QString dump;
	QHash<const void *, QSet<int>> shapes;
	QHash<const void *, QHash<int, float>> wave;	// block -> its GRAS wave period (the property's +0xf4)
	// the wind's clock: the game's accumulated frame seconds (St+0x3c), here the time since the first grass frame;
	// cur at this frame, prev at the last (the vertex shader's two phases)
	QElapsedTimer clock;
	bool timePinned = false, windDrawn = false;
	double tCur = 0.0, tPrev = 0.0;
	QString saidWind;
	QHash<const void *, QString> notes;
	QSet<QString> dumpedDocs;
};

GrassState & gs()
{
	static GrassState s;
	if ( !s.init ) {
		s.init = true;
		const QString pin = qEnvironmentVariable( "WW_CELL_GRASS" ).trimmed();
		if ( !pin.isEmpty() ) {
			s.on = pin != QLatin1StringView( "0" ) && pin.compare( QLatin1StringView( "off" ), Qt::CaseInsensitive ) != 0;
			s.pinned = true;
		} else if ( wwHarnessRun() ) {
			s.on = false;	// a harness measures what it pins
			s.pinned = true;
		} else {
			s.on = QSettings().value( QLatin1StringView( kGrassKey ), false ).toBool();
		}
		s.redSeed = qEnvironmentVariable( "WW_CELL_GRASS_RED" ).contains( QLatin1StringView( "seed" ) );
		s.dump = qEnvironmentVariable( "WW_CELL_GRASS_DUMP" ).trimmed();
		bool ok = false;
		const double t = qEnvironmentVariable( "WW_CELL_GRASS_WIND_T" ).toDouble( &ok );
		if ( ok ) {	// a fixed clock: the frame at t, its previous frame 1/60 s before
			s.timePinned = true;
			s.tCur = t;
			s.tPrev = t - 1.0 / 60.0;
		} else if ( wwHarnessRun() ) {	// a harness never reads the wall clock: t = 0, run to run identical
			s.timePinned = true;
			s.tCur = 0.0;
			s.tPrev = -1.0 / 60.0;
		}
	}
	return s;
}

float bitsToFloat( std::uint32_t u )
{
	float v;
	std::memcpy( &v, &u, 4 );
	return v;
}

//! XMConvertFloatToHalf, bit for bit.
quint16 toHalf( float v )
{
	std::uint32_t u;
	std::memcpy( &u, &v, 4 );
	const std::uint32_t sign = ( u & 0x80000000U ) >> 16U;
	u &= 0x7FFFFFFFU;
	std::uint32_t r;
	if ( u > 0x47FFEFFFU ) {
		r = 0x7FFFU;
	} else {
		if ( u < 0x38800000U ) {
			const std::uint32_t sh = 113U - ( u >> 23U );
			u = sh < 24U ? ( 0x800000U | ( u & 0x7FFFFFU ) ) >> sh : 0U;
		} else {
			u += 0xC8000000U;
		}
		r = ( ( u + 0x0FFFU + ( ( u >> 13U ) & 1U ) ) >> 13U ) & 0x7FFFU;
	}
	return quint16( r | sign );
}

float fromHalf( quint16 h )
{
	const std::uint32_t sign = std::uint32_t( h & 0x8000U ) << 16U;
	std::uint32_t e = ( h >> 10U ) & 0x1FU, m = h & 0x3FFU, u;
	if ( e == 0x1FU ) {
		u = sign | 0x7F800000U | ( m << 13U );
	} else if ( e == 0 ) {
		if ( m == 0 ) {
			u = sign;
		} else {	// a denormal half is a normal float
			e = 113U;
			while ( !( m & 0x400U ) ) {
				m <<= 1U;
				e--;
			}
			u = sign | ( e << 23U ) | ( ( m & 0x3FFU ) << 13U );
		}
	} else {
		u = sign | ( ( e + 112U ) << 23U ) | ( m << 13U );
	}
	return bitsToFloat( u );
}

const float kTwoM32 = bitsToFloat( 0x2F800000U );	// 2.3283064e-10f
const float kInv255 = bitsToFloat( 0x3B808081U );	// 1 / 255, as the engine stores it

//! Float0To1DistA: 512 floats from std::mt19937(1).
const float * distTable()
{
	static float t[512];
	static bool made = false;
	if ( !made ) {
		std::mt19937 mt( 1U );
		for ( int k = 0; k < 512; k++ )
			t[k] = ( float( std::uint32_t( mt() ) ) * kTwoM32 ) * 0.99999f;
		made = true;
	}
	return t;
}

//! The engine's 512-entry cos table (filled at run time; cos(k 2 pi / 512) is the stated stand-in).
const float * cosTable()
{
	static float t[512];
	static bool made = false;
	if ( !made ) {
		for ( int k = 0; k < 512; k++ )
			t[k] = float( std::cos( double( k ) * 6.283185307179586 / 512.0 ) );
		made = true;
	}
	return t;
}

int ctruncDiv( int a, int b )
{
	return a / b;	// C++ integer division truncates toward zero, as the engine's does
}

struct Ctx
{
	const EsmWorld & world;
	ESMFile * esm = nullptr;
	QHash<QPair<int, int>, std::shared_ptr<EsmLand>> lands;
	QHash<quint32, QVector<quint32>> ltexGrass;
	QHash<quint32, EsmGrass> grass;
	std::mt19937 keep;

	explicit Ctx( const EsmWorld & w ) : world( w ), esm( w.plugin() ) {}

	const EsmLand * land( int cx, int cy )
	{
		const QPair<int, int> k( cx, cy );
		auto it = lands.constFind( k );
		if ( it == lands.constEnd() ) {
			auto l = std::make_shared<EsmLand>();
			if ( !world.land( cx, cy, *l ) )
				l.reset();
			it = lands.insert( k, l );
		}
		return it.value().get();
	}

	//! An LTEX's grasses as GetGrassParameters takes them: GNAM order, unresolved skipped, at most 3.
	const QVector<quint32> * grasses( quint32 ltex )
	{
		if ( !ltex || ltex == ESM_LTEX_ENGINE_DEFAULT || !esm )
			return nullptr;
		auto it = ltexGrass.constFind( ltex );
		if ( it == ltexGrass.constEnd() ) {
			QVector<quint32> g;
			bool isLtex = false;
			const ESMFile::ESMRecord * lr = esm->findRecord( ltex );
			if ( lr && lr->type != GRUP && *lr == "LTEX" ) {
				isLtex = true;
				ESMFile::ESMField f( *esm, *lr );
				while ( f.next() ) {
					if ( !( f == "GNAM" ) || f.size() < 4 )
						continue;
					const quint32 id = esm->mapFormID( *lr, f.readUInt32() );
					EsmGrass gr;
					if ( g.size() < 3 && id && world.grass( id, gr ) ) {
						g.append( id );
						grass.insert( id, gr );
					}
				}
			}
			if ( !isLtex )
				return nullptr;
			it = ltexGrass.insert( ltex, g );
		}
		return &it.value();
	}
};

struct LandHit
{
	float h = 0.0f, n[3] = { 0.0f, 0.0f, 1.0f }, col[3] = { 1.0f, 1.0f, 1.0f };
};

//! GetLandData at a world point: the triangle's plane height and FACE normal, the nearest corner's colour.
bool landData( Ctx & c, float lx, float ly, LandHit & out )
{
	const int cx = int( std::floor( double( lx ) / 4096.0 ) );
	const int cy = int( std::floor( double( ly ) / 4096.0 ) );
	const EsmLand * L = c.land( cx, cy );
	if ( !L )
		return false;
	const float u = lx - float( cx * 4096 );
	const float v = ly - float( cy * 4096 );
	const int ix = qBound( 0, int( std::floor( double( u / 128.0f ) ) ), 31 );
	const int iy = qBound( 0, int( std::floor( double( v / 128.0f ) ) ), 31 );
	const float fx = u - float( ix * 128 );
	const float fy = v - float( iy * 128 );
	int t[3][2];
	if ( ( ( ix + iy ) & 1 ) == 0 ) {
		t[0][0] = 0; t[0][1] = 0; t[1][0] = 1; t[1][1] = 1;
		if ( fy >= fx ) {
			t[2][0] = 0; t[2][1] = 1;
		} else {
			t[2][0] = 1; t[2][1] = 0;
		}
	} else {
		t[0][0] = 1; t[0][1] = 0; t[1][0] = 0; t[1][1] = 1;
		if ( fx + fy > 128.0f ) {
			t[2][0] = 1; t[2][1] = 1;
		} else {
			t[2][0] = 0; t[2][1] = 0;
		}
	}
	float P[3][3];
	for ( int k = 0; k < 3; k++ ) {
		P[k][0] = float( t[k][0] * 128 );
		P[k][1] = float( t[k][1] * 128 );
		P[k][2] = L->heights[iy + t[k][1]][ix + t[k][0]];
	}
	const float e1[3] = { P[1][0] - P[0][0], P[1][1] - P[0][1], P[1][2] - P[0][2] };
	const float e2[3] = { P[2][0] - P[0][0], P[2][1] - P[0][1], P[2][2] - P[0][2] };
	float N[3] = { e1[1] * e2[2] - e1[2] * e2[1], e1[2] * e2[0] - e1[0] * e2[2], e1[0] * e2[1] - e1[1] * e2[0] };
	if ( N[2] < 0.0f ) {
		N[0] = -N[0];
		N[1] = -N[1];
		N[2] = -N[2];
	}
	const float ln = std::sqrt( ( N[0] * N[0] + N[1] * N[1] ) + N[2] * N[2] );
	for ( int k = 0; k < 3; k++ )
		out.n[k] = N[k] / ln;
	out.h = P[0][2] - ( N[0] * ( fx - P[0][0] ) + N[1] * ( fy - P[0][1] ) ) / N[2];
	int best = 0;
	float bd = 0.0f;
	for ( int k = 0; k < 3; k++ ) {
		const float dx = fx - P[k][0];
		const float dy = fy - P[k][1];
		const float dd = dx * dx + dy * dy;
		if ( k == 0 || dd < bd ) {
			best = k;
			bd = dd;
		}
	}
	if ( L->hasColors ) {
		const quint8 * cc = L->colors[iy + t[best][1]][ix + t[best][0]];
		for ( int k = 0; k < 3; k++ )
			out.col[k] = float( cc[k] ) / 255.0f;
	} else {
		out.col[0] = out.col[1] = out.col[2] = 1.0f;
	}
	return true;
}

struct Entry
{
	quint32 form = 0;
	float grid[9];
};

//! GetGrassParameters for the patch centred on quadrant vertex (cxv, cyv).
void grassParams( Ctx & c, const EsmLand & L, int q, int cxv, int cyv, std::vector<Entry> & out )
{
	out.clear();
	int nb[9][12];
	int slotOf[12];
	quint32 slotLtex[12];
	for ( int k = 0; k < 12; k++ ) {
		slotOf[k] = -1;
		slotLtex[k] = 0;
	}
	for ( int li = 0; li < L.layers[q].size(); li++ ) {
		const int idx = L.layers[q][li].index;
		if ( idx >= 0 && idx < 12 ) {
			slotOf[idx] = li;
			slotLtex[idx] = L.layers[q][li].ltex;
		}
	}
	for ( int dy = -1; dy <= 1; dy++ ) {
		for ( int dx = -1; dx <= 1; dx++ ) {
			const int k = ( dy + 1 ) * 3 + ( dx + 1 );
			const int vx = cxv + dx, vy = cyv + dy;
			for ( int s = 0; s < 12; s++ )
				nb[k][s] = 0;
			for ( const EsmLandLayer & lay : L.layers[q] )
				if ( lay.index >= 0 && lay.index < 12 )
					nb[k][lay.index] = int( lay.opacity[vy][vx] * 255.0f );
		}
	}
	const quint32 base = L.baseTex[q];
	if ( const QVector<quint32> * gl = c.grasses( base ) ) {
		for ( quint32 g : *gl ) {
			if ( out.size() >= 16 )
				break;
			const float dens = float( c.grass[g].density ) * 0.01f;
			Entry e;
			e.form = g;
			for ( int k = 0; k < 9; k++ ) {
				float s = 0.0f;
				for ( int b = 0; b < 12; b++ )
					s = s + float( nb[k][b] ) * kInv255;
				const float pct = 1.0f - s;
				e.grid[k] = pct > 0.005f ? dens : 0.0f;
			}
			out.push_back( e );
		}
	}
	for ( int slot = 0; slot < 12; slot++ ) {
		if ( slotOf[slot] < 0 )
			continue;
		const QVector<quint32> * gl = c.grasses( slotLtex[slot] );
		if ( !gl )
			continue;
		bool any = false;
		for ( int k = 0; k < 9; k++ )
			any = any || nb[k][slot] > 25;
		if ( !any )
			continue;
		for ( quint32 g : *gl ) {
			if ( out.size() >= 16 )
				break;
			const float dens = float( c.grass[g].density ) * 0.01f;
			Entry e;
			e.form = g;
			for ( int k = 0; k < 9; k++ )
				e.grid[k] = nb[k][slot] > 1 ? dens : 0.0f;
			out.push_back( e );
		}
	}
	for ( Entry & e : out ) {
		float s = 0.0f;
		for ( int k = 0; k < 9; k++ )
			s = s + e.grid[k];
		if ( s * 0.11111111f < 0.005f )
			for ( int k = 0; k < 9; k++ )
				e.grid[k] = 0.0f;
	}
}

int keepDraw( std::mt19937 & mt, int lo, int hi )
{
	const std::uint32_t u = std::uint32_t( mt() );
	return int( ( ( float( u ) * kTwoM32 ) * 0.99999f ) * float( hi - lo ) ) + lo;
}

//! CreateGrass for one grass on one patch.
void createGrass( Ctx & c, int cellX, int cellY, int q, int col, int row, const Entry & e, float ox, float oy,
	float waterW, bool redSeed, std::vector<WwGrassBlade> & out )
{
	const EsmGrass & G = c.grass[e.form];
	const float * dist = distTable();
	const float * cosT = cosTable();
	std::int32_t seed = std::int32_t( std::uint32_t( cellX ) * std::uint32_t( cellY ) ) + ( redSeed ? 1 : 0 );
	auto F = [&]() {
		const float r = dist[seed & 511];
		seed = std::int32_t( std::uint32_t( seed ) + 1U );
		return r;
	};
	const float qd = 256.0f / G.positionRange;
	const std::int64_t t = ( std::isfinite( qd ) && std::fabs( qd ) < 9.2e18f )
		? std::int64_t( qd ) : std::numeric_limits<std::int64_t>::min();
	const std::uint32_t n = std::min( std::uint32_t( std::uint64_t( t ) & 0xFFFFFFFFU ),
		std::uint32_t( int( 256.0f / 20.0f ) ) );
	if ( n == 0 )
		return;
	const float step = 1.0f / float( n );
	const float amp = ( step * 256.0f ) * 0.5f;
	const float k512 = 512.0f / 6.2831855f;
	const float cosMin = cosT[int( ( float( G.minSlope ) * 0.017453292f ) * k512 ) & 511];
	const float cosMax = cosT[int( ( float( G.maxSlope ) * 0.017453292f ) * k512 ) & 511];
	float X0, Y0;
	wwCellGrassBlockOrigin( cellX, cellY, X0, Y0 );
	const float U = float( G.unitsFromWater );
	const bool fit = ( G.flags & 4 ) != 0;
	const float cr = G.colourRange;
	for ( std::uint32_t j = 0; j < n; j++ ) {
		const float xj = float( j ) + 0.5f;
		for ( std::uint32_t i = 0; i < n; i++ ) {
			const float yi = float( i ) + 0.5f;
			float x = ox + ( xj * amp ) * 2.0f;
			float y = oy + ( yi * amp ) * 2.0f;
			// CheckCreateFromDensity
			const float gx = ( xj * 2.0f ) * step;
			const float gy = ( yi * 2.0f ) * step;
			const int ix = int( gx );
			const float fx = gx - float( ix );
			const int iy = int( gy );
			const float fy = gy - float( iy );
			const int idx = iy * 3 + ix;
			const float top = fx * e.grid[idx + 1] + ( 1.0f - fx ) * e.grid[idx];
			const float bot = ( 1.0f - fx ) * e.grid[idx + 3] + fx * e.grid[idx + 4];
			const float v = top * ( 1.0f - fy ) + bot * fy;
			const std::int32_t D = std::int32_t( v * 32768.0f );
			if ( D == 0 )
				continue;
			if ( !( keepDraw( c.keep, 0, 0x7FFF ) < D ) )
				continue;
			x = x + ( F() * 2.0f - 1.0f ) * amp;
			y = y + ( F() * 2.0f - 1.0f ) * amp;
			const quint16 hx = toHalf( x - X0 );
			const quint16 hy = toHalf( y - Y0 );
			const float lx = X0 + fromHalf( hx );
			const float ly = Y0 + fromHalf( hy );
			LandHit hit;
			if ( !landData( c, lx, ly, hit ) )
				continue;
			const float h = hit.h;
			const float lo = waterW - U, hi = U + waterW;
			bool ok;
			switch ( G.waterType ) {
			case 0: ok = h >= lo; break;
			case 1: ok = waterW <= h && h <= hi; break;
			case 2: ok = h <= lo; break;
			case 3: ok = lo <= h && h <= waterW; break;
			case 4: ok = h >= hi ? true : !( h > lo ); break;
			case 5: ok = !( h > hi || h < lo ); break;
			default: ok = true; break;
			}
			if ( !ok )
				continue;
			const float nx = hit.n[0], ny = hit.n[1], nz = hit.n[2];
			if ( nz < cosMax || nz > cosMin )
				continue;
			const float lum = ( hit.col[0] * 0.31f + hit.col[1] * 0.37f ) + hit.col[2] * 0.32f;
			const float r = F();
			const float fac = ( cr * 0.5f ) * ( r - 0.5f ) + ( 1.0f - cr );
			float shade = lum * fac;
			if ( shade >= 1.0f )
				shade = 0.99f;
			else if ( shade < 0.0f )
				shade = 0.0f;
			const float s = F() * 2.0f - 1.0f;
			const float cc = std::sqrt( 1.0f - s * s );
			float V1[3], V2[3], V3[3];
			if ( !fit ) {
				V1[0] = s; V1[1] = cc; V1[2] = 0.0f;
				V2[0] = -cc; V2[1] = s; V2[2] = 0.0f;
				V3[0] = 0.0f; V3[1] = 0.0f; V3[2] = 1.0f;
			} else {
				const float ax = std::fabs( nx ), ay = std::fabs( ny ), az = std::fabs( nz );
				float T[3];
				if ( ax <= ay && ax <= az ) {
					T[0] = 0.0f; T[1] = -nz; T[2] = ny;
				} else if ( ay <= ax && ay <= az ) {
					T[0] = -nz; T[1] = 0.0f; T[2] = nx;
				} else {
					T[0] = -ny; T[1] = nx; T[2] = 0.0f;
				}
				const float tl = std::sqrt( ( T[1] * T[1] + T[0] * T[0] ) + T[2] * T[2] );
				if ( tl > 1.0e-6f ) {
					const float inv = 1.0f / tl;
					for ( int k = 0; k < 3; k++ )
						T[k] = T[k] * inv;
				} else {
					T[0] = T[1] = T[2] = 0.0f;
				}
				const float B[3] = { T[1] * nz - ny * T[2], nx * T[2] - T[0] * nz, ny * T[0] - nx * T[1] };
				const float N[3] = { nx, ny, nz };
				const float nc = -cc;
				for ( int k = 0; k < 3; k++ ) {
					V1[k] = ( cc * T[k] + s * B[k] ) + 0.0f * N[k];
					V2[k] = ( s * T[k] + nc * B[k] ) + 0.0f * N[k];
					V3[k] = ( 0.0f * T[k] + 0.0f * B[k] ) + 1.0f * N[k];
				}
			}
			WwGrassBlade b;
			b.cx = cellX;
			b.cy = cellY;
			b.quadrant = q;
			b.col = col;
			b.row = row;
			b.form = e.form;
			b.j = int( j );
			b.i = int( i );
			b.h[0] = hx;
			b.h[1] = hy;
			b.h[2] = toHalf( h );
			b.h[3] = toHalf( shade );
			b.h[4] = toHalf( V1[0] );
			b.h[5] = toHalf( V2[0] );
			b.h[6] = toHalf( V3[0] );
			b.h[7] = toHalf( V2[2] );
			b.h[8] = toHalf( V1[1] );
			b.h[9] = toHalf( V2[1] );
			b.h[10] = toHalf( V3[1] );
			b.h[11] = toHalf( V3[2] );
			b.h[12] = toHalf( V1[2] );
			b.h[13] = toHalf( ( F() * 2.0f - 1.0f ) * G.heightRange );
			b.h[14] = 0;
			b.h[15] = 0;
			out.push_back( b );
		}
	}
}

}	// namespace

bool wwCellGrassOn()
{
	return gs().on;
}

void wwCellGrassSetOn( bool on )
{
	GrassState & s = gs();
	s.on = on;
	if ( !s.pinned )
		QSettings().setValue( QLatin1StringView( kGrassKey ), on );
}

void wwCellGrassBlockOrigin( int cx, int cy, float & x0, float & y0 )
{
	x0 = float( ctruncDiv( cx, 12 ) * 12 ) * 4096.0f;
	y0 = float( ctruncDiv( cy, 12 ) * 12 ) * 4096.0f;
}

bool wwCellGrassPlace( const EsmWorld & world, int cx, int cy, std::vector<WwGrassBlade> & out )
{
	GrassState & st = gs();
	Ctx c( world );
	const EsmLand * L = c.land( cx, cy );
	if ( !L )
		return false;
	c.keep.seed( ( std::uint32_t( cx ) * 0x9E3779B1U ) ^ std::uint32_t( cy ) );
	float wh = 0.0f;
	const bool hasWater = world.cellWater( cx, cy, wh, nullptr );
	const float waterW = hasWater ? wh : -3.4e38f;
	const size_t first = out.size();
	std::vector<Entry> ents;
	for ( int q = 0; q < 4; q++ ) {
		for ( int col = 1; col < 16; col += 2 ) {
			for ( int row = 1; row < 16; row += 2 ) {
				grassParams( c, *L, q, col, row, ents );
				const float ox = float( cx * 4096 + ( q & 1 ) * 2048 + col * 128 - 128 );
				const float oy = float( cy * 4096 + ( q >> 1 ) * 2048 + row * 128 - 128 );
				for ( const Entry & e : ents )
					createGrass( c, cx, cy, q, col, row, e, ox, oy, waterW, st.redSeed, out );
			}
		}
	}
	if ( !st.dump.isEmpty() ) {
		QFile f( st.dump );
		const bool fresh = !st.dumpedDocs.contains( st.dump );
		st.dumpedDocs.insert( st.dump );
		if ( f.open( fresh ? QIODevice::WriteOnly | QIODevice::Text : QIODevice::Append | QIODevice::Text ) ) {
			QTextStream ts( &f );
			for ( size_t k = first; k < out.size(); k++ ) {
				const WwGrassBlade & b = out[k];
				ts << "B " << b.cx << ' ' << b.cy << ' ' << b.quadrant << ' ' << b.col << ' ' << b.row << ' '
				   << QStringLiteral( "%1" ).arg( b.form, 8, 16, QLatin1Char( '0' ) ) << ' ' << b.j << ' ' << b.i;
				for ( int h = 0; h < 16; h++ )
					ts << ' ' << QStringLiteral( "%1" ).arg( b.h[h], 4, 16, QLatin1Char( '0' ) );
				ts << '\n';
			}
		}
	}
	return true;
}

void wwCellGrassTransform( const WwGrassBlade & b, bool uniformScale, float t[3], float R[9], float s[3],
	float * shade )
{
	float X0, Y0;
	wwCellGrassBlockOrigin( b.cx, b.cy, X0, Y0 );
	t[0] = X0 + fromHalf( b.h[0] );
	t[1] = Y0 + fromHalf( b.h[1] );
	t[2] = fromHalf( b.h[2] );
	// rows of R: (V1.x V2.x V3.x) (V1.y V2.y V3.y) (V1.z V2.z V3.z)
	R[0] = fromHalf( b.h[4] );
	R[1] = fromHalf( b.h[5] );
	R[2] = fromHalf( b.h[6] );
	R[3] = fromHalf( b.h[8] );
	R[4] = fromHalf( b.h[9] );
	R[5] = fromHalf( b.h[10] );
	R[6] = fromHalf( b.h[12] );
	R[7] = fromHalf( b.h[7] );
	R[8] = fromHalf( b.h[11] );
	const float k = 1.0f + fromHalf( b.h[13] );
	s[0] = uniformScale ? k : 1.0f;
	s[1] = uniformScale ? k : 1.0f;
	s[2] = k;
	if ( shade )
		*shade = fromHalf( b.h[3] );
}

void wwCellGrassBegin( const void * nif )
{
	gs().shapes.remove( nif );
	gs().wave.remove( nif );
	gs().notes.remove( nif );
	gs().dumpedDocs.clear();
}

void wwCellGrassShape( const void * nif, int block, float wavePeriod )
{
	gs().shapes[nif].insert( block );
	gs().wave[nif].insert( block, wavePeriod );
}

bool wwCellGrassWind( float wind[4], float wind2[4] )
{
	quint8 w[4] = { 0, 0, 0, 0 };
	if ( !wwLookdevWind( w ) )
		return false;
	// Sky::UpdateWind, float32 in its order (one weather: no transition blend)
	const float k255 = 1.0f / 255.0f;	// 0x3b808081
	const float M = float( w[0] ) * k255;
	const float T = float( w[3] ) * k255;
	const float freq = ( 1.0f - T ) * 0.5f + T * 4.0f;	// fWindLowestFrequency, fWindHighestFrequency
	const float minSpeed = ( ( 1.0f - T ) + T * 0.0f ) * M;	// fWindSpeedLowestLowMultiplier
	const float maxSpeed = ( ( 1.0f - T ) + T * 1.5f ) * M;	// fWindSpeedHighestHighMultiplier
	const float dir = float( w[1] ) * k255 * 360.0f;
	// the game adds the nearest of 4 BSRandom draws within +- range / 2 (not reproducible): offset 0 here
	const float twoPi = 6.28318548f;
	float angle = ( 0.0f + dir ) * 0.0174532924f;
	if ( !( angle < twoPi ) )
		angle -= twoPi;
	else if ( angle < 0.0f )
		angle += twoPi;
	wind[0] = angle;
	wind[1] = 0.0f;
	wind[2] = 0.0f;	// the phases are per shape (wwCellGrassUniforms)
	wind[3] = 0.0f;
	// BSDFPrePassShader::SetupGeometry: (fWindMinSpeed * 300, fWindMaxSpeed * 300, fWindFrequency)
	wind2[0] = minSpeed * 300.0f;
	wind2[1] = maxSpeed * 300.0f;
	wind2[2] = freq;
	wind2[3] = 1.0f;
	return true;
}

float wwCellGrassPhaseOffset( const WwGrassBlade & b )
{
	// the vertex shader's add v5.y, v5.x then * 0.0078125 (the blade's block-relative position halves)
	const float sum = fromHalf( b.h[1] ) + fromHalf( b.h[0] );
	return sum * 0.0078125f;
}

float wwCellGrassPhase( double t, float wavePeriod )
{
	// ((timer * 0.0016666667) * 6.2831802) * the GRAS wave period, float32
	return ( ( float( t ) * 0.0016666667f ) * 6.2831802f ) * wavePeriod;
}

void wwCellGrassFrameEnd()
{
	GrassState & s = gs();
	if ( s.timePinned )
		return;
	if ( !s.clock.isValid() )
		s.clock.start();
	s.tPrev = s.tCur;
	s.tCur = double( s.clock.nsecsElapsed() ) * 1e-9;
}

bool wwCellGrassWantsRepaint()
{
	GrassState & s = gs();
	const bool want = s.windDrawn && !s.timePinned && !wwHarnessRun();
	s.windDrawn = false;
	return want;
}

void wwCellGrassNote( const void * nif, const QString & line )
{
	gs().notes[nif] = line;
	qInfo().noquote() << line;
}

QString wwCellGrassEcho( const void * nif )
{
	const GrassState & s = gs();
	const auto it = s.notes.constFind( nif );
	if ( it != s.notes.constEnd() )
		return it.value();
	return s.on ? QStringLiteral( "cell grass: on, nothing placed" )
	            : QStringLiteral( "cell grass: off (Grass row off)" );
}

void wwCellGrassUniforms( Scene * scene, int block )
{
	if ( !scene || !scene->renderer )
		return;
	NifSkopeOpenGLContext::Program * prog = scene->renderer->getCurrentProgram();
	if ( !prog || prog->uniLocation( "wwGrassFade" ) < 0 )
		return;
	const GrassState & s = gs();
	const auto it = s.shapes.constFind( scene->nifModel );
	if ( it == s.shapes.constEnd() || !it->contains( block ) ) {
		prog->uni3f( "wwGrassFade", 0.0f, 0.0f, 0.0f );
		if ( prog->uniLocation( "wwGrassWind2" ) >= 0 )
			prog->uni4f( "wwGrassWind2", FloatVector4( 0.0f ) );
		return;
	}
	// fGrassStartFadeDistance 3500, fGrassFadeRange 1000 (the INI defaults), in view units
	const float k = scene->view.scale;
	prog->uni3f( "wwGrassFade", 3500.0f * k, 1000.0f * k, 1.0f );
	// the wind (the game's grass vertex shader, Shaders011 entry 02183): cb2[11] = (angle, 0, prev phase, phase),
	// cb2[12] = (min speed * 300, max speed * 300, frequency); .w = 1 = a grass shape (the game's grass vertex shader
	// runs: renormalised frame, colour ^ 2.2); no weather loaded = no wind (speeds 0: the blade stays put)
	float wind[4] = { 0, 0, 0, 0 }, wind2[4] = { 0, 0, 0, 1 };
	GrassState & ws = gs();
	const bool haveWind = wwCellGrassWind( wind, wind2 );
	if ( haveWind ) {
		const auto wit = ws.wave.constFind( scene->nifModel );
		const float wave = wit != ws.wave.constEnd() ? wit->value( block, 0.0f ) : 0.0f;
		wind[2] = wwCellGrassPhase( ws.tPrev, wave );
		wind[3] = wwCellGrassPhase( ws.tCur, wave );
		ws.windDrawn = ws.windDrawn || wind2[1] > 0.0f;
	}
	const QString said = haveWind
		? QStringLiteral( "cell grass wind: angle %1 min %2 max %3 freq %4%5" ).arg( double( wind[0] ) ).arg( double( wind2[0] ) )
			.arg( double( wind2[1] ) ).arg( double( wind2[2] ) ).arg( ws.timePinned ? QStringLiteral( " (clock pinned)" ) : QString() )
		: QStringLiteral( "cell grass wind: none (no weather loaded)" );
	if ( said != ws.saidWind ) {
		ws.saidWind = said;
		qInfo().noquote() << said;
	}
	if ( prog->uniLocation( "wwGrassWind" ) >= 0 ) {
		prog->uni4f( "wwGrassWind", FloatVector4( wind[0], wind[1], wind[2], wind[3] ) );
		prog->uni4f( "wwGrassWind2", FloatVector4( wind2[0], wind2[1], wind2[2], wind2[3] ) );
	}
}
