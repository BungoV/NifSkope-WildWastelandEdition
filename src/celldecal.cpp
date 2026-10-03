/* lane PLACED1: placed decals, projected on the CPU. See celldecal.h for the rule. */

#include "celldecal.h"

#include "esmdata.h"
#include "esmplaced.h"
#include "lodgen.h"
#include "data/niftypes.h"
#include "io/material.h"

#include <QFile>
#include <QHash>
#include <QStringList>
#include <QTextStream>

#include <algorithm>
#include <cmath>
#include <cstring>

namespace
{

constexpr float DECAL_RAY_LENGTH = 1000.0f;   // the game's reach for a decal without a box
/* Ours, not the game's: 22 of the Vault's 503 ray decals sit ON their surface (0 to 1 unit past the drawn
 * triangle), where a ray from the reference itself misses by rounding. The ray starts this far behind. */
constexpr float DECAL_RAY_SLACK = 1.0f;
constexpr float DECAL_ANGLE_MIN = 0.3f;      // the angle rule's threshold
constexpr float DECAL_ANGLE_FADE = 0.25f;     // and its fade width

struct V3
{
	float x = 0.0f, y = 0.0f, z = 0.0f;
	V3() {}
	V3( float a, float b, float c ) : x( a ), y( b ), z( c ) {}
	explicit V3( const float * p ) : x( p[0] ), y( p[1] ), z( p[2] ) {}
	V3 operator+( const V3 & o ) const { return V3( x + o.x, y + o.y, z + o.z ); }
	V3 operator-( const V3 & o ) const { return V3( x - o.x, y - o.y, z - o.z ); }
	V3 operator*( float s ) const { return V3( x * s, y * s, z * s ); }
};
inline float dot( const V3 & a, const V3 & b ) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline V3 cross( const V3 & a, const V3 & b )
{
	return V3( a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x );
}
inline V3 unit( const V3 & a )
{
	const float l = std::sqrt( dot( a, a ) );
	return l > 1.0e-20f ? a * ( 1.0f / l ) : V3( 0.0f, 0.0f, 1.0f );
}

//! Every receiver triangle in a uniform grid (CSR), triangles named by a global index.
struct TriGrid
{
	const std::vector<CellDecalReceiver> * recv = nullptr;
	std::vector<quint64> firstTri;   // per receiver, its first global index
	quint64 total = 0;
	float lo[3] = { 0, 0, 0 };
	float cell = 128.0f;
	int n[3] = { 1, 1, 1 };
	std::vector<quint32> start;
	std::vector<quint32> items;
	std::vector<quint32> stamp;
	quint32 stampNow = 0;

	inline const float * vertPos( const CellDecalReceiver & r, quint32 i ) const
	{
		return reinterpret_cast<const float *>( reinterpret_cast<const char *>( r.pos ) + size_t( i ) * r.stride );
	}
	inline const float * vertNrm( const CellDecalReceiver & r, quint32 i ) const
	{
		return reinterpret_cast<const float *>( reinterpret_cast<const char *>( r.nrm ) + size_t( i ) * r.stride );
	}
	bool tri( quint32 g, V3 p[3], V3 nr[3] ) const
	{
		size_t ri = size_t( std::upper_bound( firstTri.begin(), firstTri.end(), quint64( g ) ) - firstTri.begin() ) - 1;
		const CellDecalReceiver & r = ( *recv )[ri];
		const quint32 * t = r.tris + size_t( g - firstTri[ri] ) * 3;
		for ( int k = 0; k < 3; k++ ) {
			if ( t[k] >= r.numVerts )
				return false;
			p[k] = V3( vertPos( r, t[k] ) );
			if ( nr )
				nr[k] = V3( vertNrm( r, t[k] ) );
		}
		return true;
	}
	inline int cellOf( float v, int axis ) const
	{
		const int c = int( std::floor( ( v - lo[axis] ) / cell ) );
		return c < 0 ? 0 : ( c >= n[axis] ? n[axis] - 1 : c );
	}
	void build( const std::vector<CellDecalReceiver> & receivers )
	{
		recv = &receivers;
		firstTri.clear();
		total = 0;
		float hi[3] = { -3.4e38f, -3.4e38f, -3.4e38f };
		lo[0] = lo[1] = lo[2] = 3.4e38f;
		for ( const CellDecalReceiver & r : receivers ) {
			firstTri.push_back( total );
			total += r.numTris;
			for ( size_t v = 0; v < r.numVerts; v++ ) {
				const float * p = vertPos( r, quint32( v ) );
				for ( int k = 0; k < 3; k++ ) {
					lo[k] = std::min( lo[k], p[k] );
					hi[k] = std::max( hi[k], p[k] );
				}
			}
		}
		if ( !total || total > 0xFFFFFFF0ULL ) {
			total = 0;
			return;
		}
		float ext = 1.0f;
		for ( int k = 0; k < 3; k++ )
			ext = std::max( ext, hi[k] - lo[k] );
		cell = std::max( 96.0f, ext / 160.0f );
		for ( int k = 0; k < 3; k++ )
			n[k] = std::max( 1, int( std::floor( ( hi[k] - lo[k] ) / cell ) ) + 1 );
		const size_t cells = size_t( n[0] ) * size_t( n[1] ) * size_t( n[2] );
		start.assign( cells + 1, 0 );
		// two passes: count, then fill
		for ( int pass = 0; pass < 2; pass++ ) {
			quint32 g = 0;
			for ( const CellDecalReceiver & r : receivers ) {
				for ( size_t t = 0; t < r.numTris; t++, g++ ) {
					const quint32 * ix = r.tris + t * 3;
					if ( ix[0] >= r.numVerts || ix[1] >= r.numVerts || ix[2] >= r.numVerts )
						continue;
					int c0[3], c1[3];
					for ( int k = 0; k < 3; k++ ) {
						const float a = vertPos( r, ix[0] )[k], b = vertPos( r, ix[1] )[k], c = vertPos( r, ix[2] )[k];
						c0[k] = cellOf( std::min( a, std::min( b, c ) ), k );
						c1[k] = cellOf( std::max( a, std::max( b, c ) ), k );
					}
					for ( int z = c0[2]; z <= c1[2]; z++ )
						for ( int y = c0[1]; y <= c1[1]; y++ )
							for ( int x = c0[0]; x <= c1[0]; x++ ) {
								const size_t ci = ( size_t( z ) * size_t( n[1] ) + size_t( y ) ) * size_t( n[0] ) + size_t( x );
								if ( pass == 0 )
									start[ci + 1]++;
								else
									items[start[ci]++] = g;
							}
				}
			}
			if ( pass == 0 ) {
				for ( size_t i = 0; i < cells; i++ )
					start[i + 1] += start[i];
				items.resize( start[cells] );
			} else {
				// the fill advanced every start to its end: shift back
				for ( size_t i = cells; i > 0; i-- )
					start[i] = start[i - 1];
				start[0] = 0;
			}
		}
		stamp.assign( size_t( total ), 0 );
	}
	//! Every triangle whose cells overlap the box [blo, bhi], once each.
	void gather( const float blo[3], const float bhi[3], std::vector<quint32> & out )
	{
		out.clear();
		if ( !total )
			return;
		for ( int k = 0; k < 3; k++ )
			if ( bhi[k] < lo[k] || blo[k] > lo[k] + cell * float( n[k] ) )
				return;
		stampNow++;
		int c0[3], c1[3];
		for ( int k = 0; k < 3; k++ ) {
			c0[k] = cellOf( blo[k], k );
			c1[k] = cellOf( bhi[k], k );
		}
		for ( int z = c0[2]; z <= c1[2]; z++ )
			for ( int y = c0[1]; y <= c1[1]; y++ )
				for ( int x = c0[0]; x <= c1[0]; x++ ) {
					const size_t ci = ( size_t( z ) * size_t( n[1] ) + size_t( y ) ) * size_t( n[0] ) + size_t( x );
					for ( quint32 i = start[ci]; i < start[ci + 1]; i++ ) {
						const quint32 g = items[i];
						if ( stamp[g] != stampNow ) {
							stamp[g] = stampNow;
							out.push_back( g );
						}
					}
				}
	}
	//! The nearest crossing of the ray o + t*d, 0 <= t <= maxT, either face. -1 when none.
	float ray( const V3 & o, const V3 & d, float maxT, std::vector<quint32> & scratch )
	{
		static const float ends[3] = { 64.0f, 256.0f, 1.0e30f };
		for ( int s = 0; s < 3; s++ ) {
			const float tEnd = std::min( ends[s], maxT );
			const V3 e = o + d * tEnd;
			const float blo[3] = { std::min( o.x, e.x ) - 0.5f, std::min( o.y, e.y ) - 0.5f, std::min( o.z, e.z ) - 0.5f };
			const float bhi[3] = { std::max( o.x, e.x ) + 0.5f, std::max( o.y, e.y ) + 0.5f, std::max( o.z, e.z ) + 0.5f };
			gather( blo, bhi, scratch );
			float best = -1.0f;
			for ( quint32 g : scratch ) {
				V3 p[3];
				if ( !tri( g, p, nullptr ) )
					continue;
				// Moller-Trumbore, two-sided
				const V3 e1 = p[1] - p[0], e2 = p[2] - p[0];
				const V3 pv = cross( d, e2 );
				const float det = dot( e1, pv );
				if ( std::fabs( det ) < 1.0e-9f )
					continue;
				const float inv = 1.0f / det;
				const V3 tv = o - p[0];
				const float u = dot( tv, pv ) * inv;
				if ( u < -1.0e-5f || u > 1.00001f )
					continue;
				const V3 qv = cross( tv, e1 );
				const float v = dot( d, qv ) * inv;
				if ( v < -1.0e-5f || u + v > 1.00001f )
					continue;
				const float t = dot( e2, qv ) * inv;
				if ( t < 0.0f || t > tEnd )
					continue;
				if ( best < 0.0f || t < best )
					best = t;
			}
			if ( best >= 0.0f )
				return best;
			if ( tEnd >= maxT )
				break;
		}
		return -1.0f;
	}
};

struct ClipVert
{
	float l[3];   // box frame, relative to the centre: width, height, depth
	V3 pos, nrm;
	float fade;
};

inline ClipVert mixVert( const ClipVert & a, const ClipVert & b, float t )
{
	ClipVert o;
	for ( int k = 0; k < 3; k++ )
		o.l[k] = a.l[k] + ( b.l[k] - a.l[k] ) * t;
	o.pos = a.pos + ( b.pos - a.pos ) * t;
	o.nrm = a.nrm + ( b.nrm - a.nrm ) * t;
	o.fade = a.fade + ( b.fade - a.fade ) * t;
	return o;
}

//! Sutherland-Hodgman against the six faces of the box |l[k]| <= half[k].
int clipToBox( ClipVert * poly, int count, const float half[3] )
{
	ClipVert tmp[16];
	for ( int axis = 0; axis < 3; axis++ ) {
		for ( int side = 0; side < 2; side++ ) {
			const float sgn = side ? -1.0f : 1.0f;
			int m = 0;
			for ( int i = 0; i < count; i++ ) {
				const ClipVert & a = poly[i];
				const ClipVert & b = poly[( i + 1 ) % count];
				const float da = half[axis] - sgn * a.l[axis];
				const float db = half[axis] - sgn * b.l[axis];
				if ( da >= 0.0f )
					tmp[m++] = a;
				if ( ( da >= 0.0f ) != ( db >= 0.0f ) )
					tmp[m++] = mixVert( a, b, da / ( da - db ) );
			}
			count = m;
			if ( count < 3 )
				return 0;
			for ( int i = 0; i < count; i++ )
				poly[i] = tmp[i];
		}
	}
	return count;
}

QString slashed( const QString & p, const char * folder )
{
	QString s = p;
	s.replace( QLatin1Char( '\\' ), QLatin1Char( '/' ) );
	while ( s.startsWith( QLatin1Char( '/' ) ) )
		s.remove( 0, 1 );
	if ( s.isEmpty() )
		return s;
	const QString pre = QString::fromLatin1( folder ) + QLatin1Char( '/' );
	if ( !s.startsWith( pre, Qt::CaseInsensitive ) )
		s.prepend( pre );
	return s;
}

struct DecalTextures
{
	QString diffuse, normal, spec;
};

//! The textures a decal base draws with: its material file's, else its own slots.
DecalTextures texturesOf( const EsmDecalBase & b, const QString & dataRoot )
{
	DecalTextures t;
	if ( !b.material.isEmpty() ) {
		QByteArray bytes;
		if ( lodgenProbeAsset( dataRoot, slashed( b.material, "materials" ), nullptr, nullptr, nullptr, &bytes ) ) {
			const ShaderMaterial sm( bytes );
			if ( sm.isValid() ) {
				const QStringList & l = sm.textures();
				if ( l.size() > 0 )
					t.diffuse = slashed( l[0], "textures" );
				if ( l.size() > 1 )
					t.normal = slashed( l[1], "textures" );
				if ( l.size() > 2 && sm.specularEnabled() )
					t.spec = slashed( l[2], "textures" );
			}
		}
	}
	if ( t.diffuse.isEmpty() ) {
		t.diffuse = slashed( b.diffuse, "textures" );
		t.normal = slashed( b.normal, "textures" );
		t.spec = slashed( b.spec, "textures" );
	}
	return t;
}

} // namespace

QString cellDecalFateName( CellDecalFate f )
{
	switch ( f ) {
	case CellDecalFate::Drawn:       return QStringLiteral( "drawn" );
	case CellDecalFate::NoDecalData: return QStringLiteral( "no decal data" );
	case CellDecalFate::DiceRoll:    return QStringLiteral( "random size or sheet quarter" );
	case CellDecalFate::NotABox:     return QStringLiteral( "primitive is not a box" );
	case CellDecalFate::RayMissed:   return QStringLiteral( "no surface within reach" );
	case CellDecalFate::NoSurface:   return QStringLiteral( "nothing opaque in the box" );
	case CellDecalFate::NoTexture:   return QStringLiteral( "no texture" );
	case CellDecalFate::RedOff:      return QStringLiteral( "red control" );
	}
	return QStringLiteral( "?" );
}

QString cellDecalCensusLine( const CellDecalResult & r )
{
	static const CellDecalFate refusals[] = { CellDecalFate::DiceRoll, CellDecalFate::NotABox,
		CellDecalFate::RayMissed, CellDecalFate::NoSurface, CellDecalFate::NoTexture, CellDecalFate::RedOff };
	const int drawn = r.count[int( CellDecalFate::Drawn )];
	int refused = 0;
	QStringList why;
	for ( CellDecalFate f : refusals ) {
		const int n = r.count[int( f )];
		refused += n;
		if ( n )
			why.append( QStringLiteral( "%1 %2" ).arg( cellDecalFateName( f ) ).arg( n ) );
	}
	QString s = QStringLiteral( "  placed decals: %1 read, drawn %2 (%3 triangles; %4 by their box, %5 by a ray; "
		"%6 opaque triangles offered), refused %7" )
		.arg( drawn + refused ).arg( drawn ).arg( r.triangles ).arg( r.withPrimitive ).arg( r.byRay )
		.arg( r.receiverTris ).arg( refused );
	if ( refused )
		s += QStringLiteral( ": " ) + why.join( QLatin1String( ", " ) );
	if ( !r.red.isEmpty() )
		s += QStringLiteral( " [RED CONTROL %1]" ).arg( r.red );
	return s + QLatin1Char( '\n' );
}

void cellProjectDecals( const EsmWorld & world, const QString & dataRoot,
	const std::vector<CellDecalRef> & refs, const std::vector<CellDecalReceiver> & receivers,
	const float origin[3], CellDecalResult & out, const QString & dumpPath )
{
	out = CellDecalResult();
	out.fates.assign( refs.size(), CellDecalFate::Drawn );
	/* RED CONTROL (WW_CELL_DECAL_RED): "none" draws no decal, "wide" doubles every
	 * box's width and height, "axis" projects along the wrong local axis. Each one
	 * breaks a different claim of tests/spells/cell_decal.sh. */
	const QByteArray red = qgetenv( "WW_CELL_DECAL_RED" ).trimmed().toLower();
	out.red = QString::fromLatin1( red );
	const bool redNone = red == "none" || red == "1";
	const bool redWide = red == "wide";
	const bool redAxis = red == "axis";

	TriGrid grid;
	bool gridBuilt = false;
	std::vector<quint32> scratch;
	QHash<quint32, EsmDecalBase> bases;
	QHash<quint32, int> meshOfBase;
	QString dump;
	QTextStream ds( &dump );

	for ( size_t i = 0; i < refs.size(); i++ ) {
		const CellDecalRef & r = refs[i];
		auto bit = bases.find( r.base );
		if ( bit == bases.end() ) {
			EsmDecalBase b;
			esmDecalBase( world, r.base, b );
			bit = bases.insert( r.base, b );
		}
		const EsmDecalBase & b = bit.value();
		auto setFate = [&]( CellDecalFate f ) {
			out.fates[i] = f;
			out.count[int( f )]++;
			if ( !dumpPath.isEmpty() )
				ds << QString::number( r.form, 16 ).rightJustified( 8, QLatin1Char( '0' ) )
				   << " " << int( f ) << " " << cellDecalFateName( f ) << "\n";
		};
		if ( !b.exists || !b.hasDecalData ) {
			setFate( CellDecalFate::NoDecalData );
			continue;
		}
		if ( redNone ) {
			setFate( CellDecalFate::RedOff );
			continue;
		}
		EsmRefrDecal rd;
		esmRefrDecal( world, r.form, rd );
		if ( !b.wholeTexture() || ( !rd.hasPrimitive && !b.sizeIsFixed() ) ) {
			setFate( CellDecalFate::DiceRoll );
			continue;
		}
		if ( rd.hasPrimitive && rd.primType != 1 ) {
			setFate( CellDecalFate::NotABox );
			continue;
		}
		int mi = meshOfBase.value( r.base, -1 );
		if ( mi < 0 ) {
			const DecalTextures t = texturesOf( b, dataRoot );
			if ( t.diffuse.isEmpty() ) {
				meshOfBase.insert( r.base, -2 );
				mi = -2;
			} else {
				CellDecalMesh m;
				m.name = QStringLiteral( "decal " ) + ( b.edid.isEmpty() ? QString::number( r.base, 16 ) : b.edid );
				m.diffuse = t.diffuse;
				m.normal = t.normal;
				m.spec = t.spec;
				mi = int( out.meshes.size() );
				out.meshes.push_back( m );
				meshOfBase.insert( r.base, mi );
			}
		}
		if ( mi < 0 ) {
			setFate( CellDecalFate::NoTexture );
			continue;
		}
		if ( !gridBuilt ) {
			grid.build( receivers );
			gridBuilt = true;
			out.receiverTris = qint64( grid.total );
		}

		// the reference's frame, the cell view's placement convention: world = R * local
		Matrix rm;
		rm.fromEuler( -r.rot[0], -r.rot[1], -r.rot[2] );
		const Vector3 cx = rm * Vector3( 1.0f, 0.0f, 0.0f );
		const Vector3 cy = rm * Vector3( 0.0f, 1.0f, 0.0f );
		const Vector3 cz = rm * Vector3( 0.0f, 0.0f, 1.0f );
		V3 axW( cx[0], cx[1], cx[2] );              // width: local +X
		V3 axH( -cz[0], -cz[1], -cz[2] );           // height: local -Z
		V3 axD( cy[0], cy[1], cy[2] );              // projection: local +Y
		if ( redAxis ) {
			const V3 t = axD;
			axD = axH;
			axH = t;
		}
		V3 centre( r.pos[0] - origin[0], r.pos[1] - origin[1], r.pos[2] - origin[2] );
		float width, height, depth, rayT = -1.0f;
		if ( rd.hasPrimitive ) {
			width = 2.0f * rd.half[0];
			height = 2.0f * rd.half[2];
			depth = 2.0f * rd.half[1];
		} else {
			rayT = grid.ray( centre - axD * DECAL_RAY_SLACK, axD, DECAL_RAY_LENGTH + DECAL_RAY_SLACK, scratch );
			if ( rayT < 0.0f ) {
				setFate( CellDecalFate::RayMissed );
				continue;
			}
			rayT -= DECAL_RAY_SLACK;
			centre = centre + axD * rayT;
			width = b.minWidth * rd.widthScale;
			height = b.minHeight * rd.heightScale;
			depth = b.depth;
		}
		if ( redWide ) {
			width *= 2.0f;
			height *= 2.0f;
		}
		if ( !( width > 0.0f ) || !( height > 0.0f ) || !( depth > 0.0f ) ) {
			setFate( CellDecalFate::NoSurface );
			continue;
		}
		const float half[3] = { width * 0.5f, height * 0.5f, depth * 0.5f };

		// the triangles near the box
		float blo[3], bhi[3];
		{
			const float c[3] = { centre.x, centre.y, centre.z };
			const float aw[3] = { axW.x, axW.y, axW.z }, ah[3] = { axH.x, axH.y, axH.z }, ad[3] = { axD.x, axD.y, axD.z };
			for ( int k = 0; k < 3; k++ ) {
				const float e = std::fabs( aw[k] ) * half[0] + std::fabs( ah[k] ) * half[1] + std::fabs( ad[k] ) * half[2];
				blo[k] = c[k] - e;
				bhi[k] = c[k] + e;
			}
		}
		grid.gather( blo, bhi, scratch );
		CellDecalMesh & mesh = out.meshes[size_t( mi )];
		const V3 against = axD * -1.0f;
		qint64 made = 0;
		for ( quint32 g : scratch ) {
			V3 p[3], nr[3];
			if ( !grid.tri( g, p, nr ) )
				continue;
			V3 face = cross( p[1] - p[0], p[2] - p[0] );
			if ( dot( face, face ) < 1.0e-12f )
				continue;
			face = unit( face );
			// a mirrored placement winds the other way: the shading normals say which side is the front
			if ( dot( face, nr[0] + nr[1] + nr[2] ) < 0.0f )
				face = face * -1.0f;
			const float gd = dot( face, against );
			if ( gd <= 0.0f )
				continue;
			ClipVert poly[16];
			float fadeMax = 0.0f;
			for ( int k = 0; k < 3; k++ ) {
				const V3 rel = p[k] - centre;
				poly[k].l[0] = dot( rel, axW );
				poly[k].l[1] = dot( rel, axH );
				poly[k].l[2] = dot( rel, axD );
				poly[k].pos = p[k];
				poly[k].nrm = nr[k];
				float f = 1.0f;
				if ( gd < DECAL_ANGLE_MIN ) {
					f = ( dot( unit( nr[k] ), against ) - DECAL_ANGLE_MIN ) / DECAL_ANGLE_FADE;
					f = f < 0.0f ? 0.0f : ( f > 1.0f ? 1.0f : f );
				}
				poly[k].fade = f;
				fadeMax = std::max( fadeMax, f );
			}
			if ( fadeMax <= 0.0f )
				continue;
			const int count = clipToBox( poly, 3, half );
			if ( count < 3 )
				continue;
			const quint32 first = quint32( mesh.verts.size() );
			for ( int k = 0; k < count; k++ ) {
				CellDecalVert v;
				const V3 n = unit( poly[k].nrm );
				// the decal's own frame laid onto the surface: +u along the width axis, +v along the height axis
				V3 tu = axW - n * dot( axW, n );
				V3 tv = axH - n * dot( axH, n );
				tu = dot( tu, tu ) > 1.0e-10f ? unit( tu ) : unit( cross( axH, n ) );
				tv = dot( tv, tv ) > 1.0e-10f ? unit( tv ) : unit( cross( n, tu ) );
				v.pos[0] = poly[k].pos.x; v.pos[1] = poly[k].pos.y; v.pos[2] = poly[k].pos.z;
				v.nrm[0] = n.x; v.nrm[1] = n.y; v.nrm[2] = n.z;
				v.bit[0] = tu.x; v.bit[1] = tu.y; v.bit[2] = tu.z;
				v.tan[0] = tv.x; v.tan[1] = tv.y; v.tan[2] = tv.z;
				v.uv[0] = poly[k].l[0] / width + 0.5f;
				v.uv[1] = poly[k].l[1] / height + 0.5f;
				v.alpha = poly[k].fade;
				mesh.verts.push_back( v );
			}
			for ( int k = 1; k + 1 < count; k++ ) {
				mesh.tris.push_back( first );
				mesh.tris.push_back( first + quint32( k ) );
				mesh.tris.push_back( first + quint32( k + 1 ) );
				made++;
			}
		}
		if ( !made ) {
			out.fates[i] = CellDecalFate::NoSurface;
			out.count[int( CellDecalFate::NoSurface )]++;
		} else {
			out.count[int( CellDecalFate::Drawn )]++;
			( rd.hasPrimitive ? out.withPrimitive : out.byRay )++;
			out.triangles += made;
			mesh.decals++;
		}
		if ( !dumpPath.isEmpty() ) {
			// form fate | world centre | width height depth | ray distance | triangles | axes W H D
			ds << QString::number( r.form, 16 ).rightJustified( 8, QLatin1Char( '0' ) )
			   << " " << int( out.fates[i] ) << " " << cellDecalFateName( out.fates[i] )
			   << " | " << centre.x + origin[0] << " " << centre.y + origin[1] << " " << centre.z + origin[2]
			   << " | " << width << " " << height << " " << depth
			   << " | " << rayT << " | " << made
			   << " | " << axW.x << " " << axW.y << " " << axW.z
			   << " " << axH.x << " " << axH.y << " " << axH.z
			   << " " << axD.x << " " << axD.y << " " << axD.z << "\n";
		}
	}
	if ( !dumpPath.isEmpty() ) {
		ds.flush();
		QFile f( dumpPath );
		if ( f.open( QIODevice::WriteOnly | QIODevice::Truncate ) )
			f.write( dump.toUtf8() );
	}
}
