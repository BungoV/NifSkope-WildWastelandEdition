#ifndef WW_FAKEAIR_H
#define WW_FAKEAIR_H

#include <QString>

/* lane MUSGI1 (11-03): the game's fake air -- placed effect cards that paint fog, mist, haze or a light shaft (vanilla
 * Effects\Ambient\Mist*, GlowFill*, *LightBeam*, BeamSimple*, BeamSmoky*; Far Harbor GlowingSeaMist*). bungo: with our
 * Volumetric Air on, ALL fog is ours, so these hide while it is on. Steam, smoke, fire, sparks, debris, weapon beams,
 * camera-attached and particle (mps) effects stay. Rule on the model path, pure. */
inline bool wwFakeAirModel( const QString & model )
{
	QString m = model.toLower();
	m.replace( QLatin1Char( '\\' ), QLatin1Char( '/' ) );
	if ( m.startsWith( QLatin1String( "meshes/" ) ) )
		m.remove( 0, 7 );
	if ( !m.startsWith( QLatin1String( "effects/" ) ) )
		return false;
	const QString stem = m.mid( m.lastIndexOf( QLatin1Char( '/' ) ) + 1 );
	static const char * const air[] = { "mist", "fog", "haze", "beam", "glowfill", "glowflat" };
	static const char * const keep[] = { "steam", "smoke", "fire", "flame", "spark", "ember", "debris", "tracer", "laser",
		"projectile", "impact", "attach", "underwater", "institute", "explosion" };
	bool hit = false;
	for ( const char * a : air )
		hit = hit || stem.contains( QLatin1String( a ) );
	if ( !hit || stem.startsWith( QLatin1String( "mps" ) ) )
		return false;
	for ( const char * k : keep )
		if ( stem.contains( QLatin1String( k ) ) )
			return false;
	return true;
}

//! WW_CELL_FAKEAIR_RED=keep draws the cards (gl/cellvolair.cpp)
bool wwFakeAirRed();

#endif
