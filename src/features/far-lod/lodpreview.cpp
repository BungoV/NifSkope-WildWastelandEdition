#include "features/far-lod/lodpreview.h"

#include "wwswitch.h"

#include <QStringList>

namespace
{
struct PreviewState
{
	int force = -1, ring = -1, grid = LodPreview::Follow;
	bool tint = false;
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
		: w == "ring" ? wwEnvIsSet( "WW_CELL_LOD_RING" ) : w == "grid" ? wwEnvIsSet( "WW_CELL_NEAR_GRID" ) : false;
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
	// full white, rep0 green, rep1 yellow, rep2 orange, rep3 red, impostor blue; the fade band magenta
	static const float c[7][3] = { { 0.92f, 0.92f, 0.92f }, { 0.20f, 0.85f, 0.25f }, { 0.95f, 0.90f, 0.15f },
		{ 1.00f, 0.55f, 0.10f }, { 0.95f, 0.15f, 0.15f }, { 0.25f, 0.45f, 1.00f }, { 1.00f, 0.20f, 1.00f } };
	const int s = step < 0 ? 0 : step > 6 ? 6 : step;
	for ( int k = 0; k < 3; k++ )
		rgb[k] = c[s][k];
}
}	// namespace LodPreview
