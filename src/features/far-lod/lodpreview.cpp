#include "features/far-lod/lodpreview.h"

#include "wwswitch.h"

#include <QStringList>

#include <algorithm>

namespace
{
struct PreviewState
{
	int force = -1, ring = -1, grid = LodPreview::Follow, level = -1;
	bool tint = false;
	int cards[3] = { 0, 0, 0 };	// lane LODLIVE1: flat, octahedral, mid-fade (last frame)
	int rows[LodPreview::kBands] = {};
	quint64 rowsGen = 0;
	quint64 gen = 1;
	bool eyeSet = false;
	float eye[3] = { 0.0f, 0.0f, 0.0f };
};

PreviewState & ps()
{
	static PreviewState s;
	return s;
}

const QStringList & levelNames()
{
	static const QStringList n = { QStringLiteral( "full" ), QStringLiteral( "rep0" ), QStringLiteral( "rep1" ),
		QStringLiteral( "rep2" ), QStringLiteral( "rep3" ), QStringLiteral( "impostor" ) };
	return n;
}

const QStringList & ringNames()
{
	static const QStringList n = { QStringLiteral( "band" ), QStringLiteral( "lod4" ), QStringLiteral( "lod8" ),
		QStringLiteral( "lod16" ), QStringLiteral( "lod32" ) };
	return n;
}

const QStringList & gridNames()
{
	static const QStringList n = { QStringLiteral( "follow" ), QStringLiteral( "freeze" ), QStringLiteral( "off" ) };
	return n;
}

//! a pin's value: a name from `names` or its index; -2 = unset or unknown
int pinOf( const QByteArray & v, const QStringList & names )
{
	if ( v.isEmpty() )
		return -2;
	const QString s = QString::fromLatin1( v ).trimmed().toLower();
	const int i = names.indexOf( s );
	if ( i >= 0 )
		return i;
	bool ok = false;
	const int n = s.toInt( &ok );
	return ok && n >= -1 && n < names.size() ? n : -2;
}
}	// namespace

namespace LodPreview
{
int force()
{
	const int p = pinOf( wwEnv( "WW_CELL_LOD_FORCE" ), levelNames() );
	return p != -2 ? p : ps().force;
}

void setForce( int step )
{
	if ( step < -1 || step >= kLevels || step == ps().force )
		return;
	ps().force = step;
	ps().gen++;
}

int objectSlot()
{
	const int f = force();
	return f < 0 ? -1 : f <= 1 ? 0 : f >= 5 ? 3 : f - 1;
}

bool tint()
{
	return wwEnvIsSet( "WW_CELL_LOD_TINT" ) ? wwEnvInt( "WW_CELL_LOD_TINT" ) != 0 : ps().tint;
}

void setTint( bool on )
{
	if ( on == ps().tint )
		return;
	ps().tint = on;
	ps().gen++;
}

int ring()
{
	const int p = pinOf( wwEnv( "WW_CELL_LOD_RING" ), ringNames() );
	return p != -2 ? p : ps().ring;
}

void setRing( int r )
{
	if ( r < -1 || r >= kRings || r == ps().ring )
		return;
	ps().ring = r;
	ps().gen++;
}

int grid()
{
	const int p = pinOf( wwEnv( "WW_CELL_NEAR_GRID" ), gridNames() );
	return p >= 0 ? p : ps().grid;
}

void setGrid( int mode )
{
	if ( mode < Follow || mode > Off || mode == ps().grid )
		return;
	ps().grid = mode;
	if ( mode == Follow )
		ps().eyeSet = false;
	ps().gen++;
}

bool pinned( const char * which )
{
	const QByteArray w( which );
	return w == "force" ? wwEnvIsSet( "WW_CELL_LOD_FORCE" ) : w == "tint" ? wwEnvIsSet( "WW_CELL_LOD_TINT" )
		: w == "ring" ? wwEnvIsSet( "WW_CELL_LOD_RING" ) : w == "grid" ? wwEnvIsSet( "WW_CELL_NEAR_GRID" )
		: w == "level" ? wwEnvIsSet( "WW_CELL_LOD_LEVEL" ) : false;
}

quint64 gen()
{
	return ps().gen;
}

bool frozenEye( float out[3] )
{
	if ( !ps().eyeSet || grid() == Follow )
		return false;
	for ( int k = 0; k < 3; k++ )
		out[k] = ps().eye[k];
	return true;
}

void setFrozenEye( const float eye[3] )
{
	for ( int k = 0; k < 3; k++ )
		ps().eye[k] = eye[k];
	ps().eyeSet = true;
}

QString levelName( int step )
{
	return step >= 0 && step < levelNames().size() ? levelNames().at( step ) : QStringLiteral( "auto" );
}

QString ringName( int r )
{
	return r >= 0 && r < ringNames().size() ? ringNames().at( r ) : QStringLiteral( "all" );
}

QString gridName( int mode )
{
	return mode >= 0 && mode < gridNames().size() ? gridNames().at( mode ) : QStringLiteral( "follow" );
}

int ringOf( const QString & name )
{
	if ( !name.startsWith( QLatin1String( "FarLOD " ) ) )
		return -1;
	const QStringView v( name );
	qsizetype i = 7;
	while ( i < v.size() ) {
		qsizetype e = v.indexOf( QLatin1Char( ' ' ), i );
		if ( e < 0 )
			e = v.size();
		const QStringView t = v.mid( i, e - i );
		if ( t.size() >= 2 && t.at( 0 ) == QLatin1Char( 'r' ) ) {
			bool ok = false;
			const int n = t.mid( 1 ).toInt( &ok );
			if ( ok )
				return n;
		}
		i = e + 1;
	}
	return -1;
}

bool ringHides( const QString & name )
{
	const int want = ring();
	if ( want < 0 )
		return false;
	const int r = ringOf( name );
	return r >= 0 && r != want;
}

void tintColour( int step, float rgb[3] )
{
	// full white, rep0 green, rep1 yellow, rep2 orange, rep3 red, impostor blue; the fade band magenta; octahedral cyan
	static const float c[8][3] = { { 0.92f, 0.92f, 0.92f }, { 0.20f, 0.85f, 0.25f }, { 0.95f, 0.90f, 0.15f },
		{ 1.00f, 0.55f, 0.10f }, { 0.95f, 0.15f, 0.15f }, { 0.25f, 0.45f, 1.00f }, { 1.00f, 0.20f, 1.00f },
		{ 0.10f, 0.90f, 0.90f } };
	const int s = step < 0 ? 0 : step > 7 ? 7 : step;
	for ( int k = 0; k < 3; k++ )
		rgb[k] = c[s][k];
}

int level()
{
	const QByteArray v = wwEnv( "WW_CELL_LOD_LEVEL" ).trimmed().toLower();
	if ( v == "auto" )
		return -1;
	static const QStringList n = { QStringLiteral( "lod4" ), QStringLiteral( "lod8" ), QStringLiteral( "lod16" ),
		QStringLiteral( "lod32" ) };
	const int p = pinOf( v, n );
	return p != -2 ? p : ps().level;
}

void setLevel( int l )
{
	if ( l < -1 || l > 3 || l == ps().level )
		return;
	ps().level = l;
	ps().gen++;
}

QString mapLevelName( int l )
{
	return l >= 0 && l <= 3 ? QStringLiteral( "lod%1" ).arg( 4 << l ) : QStringLiteral( "auto" );
}

QString bandName( int b )
{
	static const char * n[kBands] = { "full mesh", "LOD4", "LOD8", "LOD16", "LOD32", "flat card / billboard",
		"mid fade", "octahedral impostor" };
	return QString::fromLatin1( n[b < 0 ? 0 : b >= kBands ? kBands - 1 : b] );
}

int bandOfRing( int ring, bool objects, int lvl, int slot )
{
	// cellfarlod.cpp: the object rows take objectSlot() on every ring, else the ring's slot; the map level moves the
	// rings past the band; the ring slots 0,0,1,2,3 = the game's levels 4,4,8,16,32
	if ( objects && slot >= 0 )
		return Lod4 + std::min( slot, 3 );
	if ( ring > 0 && lvl >= 0 )
		return Lod4 + std::min( lvl, 3 );
	static const int kSlot[kRings] = { 0, 0, 1, 2, 3 };
	return Lod4 + kSlot[ring < 0 ? 0 : ring >= kRings ? kRings - 1 : ring];
}

int bandOfFarName( const QString & name )
{
	const int r = ringOf( name );
	if ( r < 0 )
		return -1;
	const bool ground = name.startsWith( QLatin1String( "FarLOD terrain" ) ) || name.startsWith( QLatin1String( "FarLOD water" ) );
	// the game's own chunks (cellfarvanilla.cpp "FarLOD [terrain |water ]v r<i> <ws>.<L>.<x>.<y>"): their level L as built
	if ( name.contains( QLatin1String( " v r" ) ) ) {
		for ( const QString & t : name.split( QLatin1Char( ' ' ) ) ) {
			const QStringList f = t.split( QLatin1Char( '.' ) );
			if ( f.size() < 4 )
				continue;
			const int L = f.at( f.size() - 3 ).toInt();
			return L >= 32 ? Lod32 : L >= 16 ? Lod16 : L >= 8 ? Lod8 : Lod4;
		}
	}
	return bandOfRing( r, !ground, level(), objectSlot() );
}

int octStripe( int frameW )
{
	return frameW <= 64 ? 1 : frameW <= 128 ? 2 : frameW <= 256 ? 3 : 4;
}

void noteCards( int flat, int oct, int fading )
{
	ps().cards[0] = flat;
	ps().cards[1] = oct;
	ps().cards[2] = fading;
}

void noteRows( int band, int n )
{
	if ( ps().rowsGen != ps().gen ) {
		ps().rowsGen = ps().gen;
		for ( int & k : ps().rows )
			k = 0;
	}
	if ( band >= 0 && band < kBands )
		ps().rows[band] += n;
}

QString viewCounts()
{
	const int * w = ps().rowsGen == ps().gen ? ps().rows : nullptr;
	auto row = [w]( int b ) { return w ? w[b] : 0; };
	return QStringLiteral( "far cards flat %1, octahedral %2, mid fade %3; instanced far rows LOD4 %4, LOD8 %5, LOD16 %6, "
		"LOD32 %7, mid fade %8" )
		.arg( ps().cards[0] ).arg( ps().cards[1] ).arg( ps().cards[2] )
		.arg( row( Lod4 ) ).arg( row( Lod8 ) ).arg( row( Lod16 ) ).arg( row( Lod32 ) ).arg( row( Fade ) );
}

QString viewLegend( const float ringStart[kRings], int rings, double tanHalfFovY )
{
	// the screen height share of a 1024-unit object (a house) where each ring starts, at this view's field of view
	QStringList ringAt;
	for ( int i = 0; i < rings && i < kRings; i++ ) {
		const double d = std::max( 1.0, double( ringStart[i] ) );
		const double share = tanHalfFovY > 0.0 ? 1024.0 / ( 2.0 * d * tanHalfFovY ) : 0.0;
		ringAt << QStringLiteral( "%1 %2 from %3 u = %4% screen" ).arg( ringName( i ), bandName( bandOfFarName(
			QStringLiteral( "FarLOD terrain r%1" ).arg( i ) ) ) ).arg( double( ringStart[i] ), 0, 'f', 0 )
			.arg( 100.0 * share, 0, 'f', 2 );
	}
	return QStringLiteral( "white = full mesh (the near cells), green = LOD4, yellow = LOD8, orange = LOD16, red = LOD32, "
		"blue = flat card / billboard tree, cyan + stripes = octahedral impostor (1 stripe 64 px frames .. 4 = 512 px), "
		"magenta = mid fade; ground rings (a 1024-unit object's screen height where each starts): %1" )
		.arg( ringAt.join( QLatin1String( "; " ) ) );
}
}	// namespace LodPreview
