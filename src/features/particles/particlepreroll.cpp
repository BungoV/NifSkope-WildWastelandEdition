/* Particles module: the held still shot's emitter pre-roll (was in src/nifskope_ui.cpp; MODULES1).
 * Called by the shot hooks in nifskope_ui.cpp. */
#include "features/particles/glparticles.h"
#include "gl/glscene.h"
#include "glview.h"
#include <QMap>
#include <QString>
#include <QStringList>

bool wwCellParticlesOn();	// src/cellview.cpp

/* a held still shot starts its emitters cold; run them WW_CELL_PARTICLES_PREROLL (5) s first, as the
 * game's are already burning. Particles row only; red WW_CELL_PARTICLES_RED=nopreroll. */
QString wwParticlePreroll( GLView * ogl, float shotT )
{
	Scene * sc = ogl ? ogl->getScene() : nullptr;
	if ( !sc || !wwCellParticlesOn() || qEnvironmentVariableIntValue( "WW_RENDER_FRAMES" ) > 0 )
		return QString();
	const bool red = qgetenv( "WW_CELL_PARTICLES_RED" ).trimmed() == "nopreroll";
	bool ok = false;
	float secs = qEnvironmentVariable( "WW_CELL_PARTICLES_PREROLL" ).toFloat( &ok );
	if ( !ok || !( secs >= 0.0f ) )
		secs = 5.0f;
	if ( red )
		secs = 0.0f;
	int systems = 0, steps = 0, live = 0;
	QMap<QString, int> byName;	// the census names each system, so a gate can read one model's systems
	for ( Node * n : sc->nodes.list() ) {
		auto * ps = dynamic_cast<Particles *>( n );
		if ( !ps )
			continue;
		const int k = secs > 0.0f ? ps->wwPreroll( shotT - secs, shotT, 1.0f / 30.0f ) : 0;
		systems += k > 0;
		steps += k;
		live += ps->liveCount();
		byName[ps->getName()] += ps->liveCount();
	}
	QStringList census;
	for ( auto it = byName.cbegin(); it != byName.cend(); ++it )
		census << QStringLiteral( "%1=%2" ).arg( it.key() ).arg( it.value() );
	return QStringLiteral( "particle preroll: %1 s%2, %3 systems, %4 steps, live %5 at t %6 | %7" ).arg( secs )
		.arg( red ? QStringLiteral( " (red WW_CELL_PARTICLES_RED=nopreroll)" ) : QString() ).arg( systems ).arg( steps )
		.arg( live ).arg( shotT ).arg( census.join( QLatin1Char( ' ' ) ) );
}
