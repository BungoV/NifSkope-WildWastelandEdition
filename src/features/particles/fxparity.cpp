#include "features/particles/fxparity.h"

#include "wwswitch.h"

#include <QSettings>

#include <cstdio>

/* lane FXPARITY1: the "Particle Game Rules" switch (fxparity.h). Re-read on any live set of WW_FX_PARITY*. */
namespace
{
struct FxpState
{
	bool init = false;
	quint64 liveGen = 0;
	bool on = false, pinned = false;
	int reds = 0;
	int said = -1;
};

FxpState & fxp()
{
	static FxpState s;
	if ( !s.init || wwSwitchMovedFor( s.liveGen, { "WW_FX_PARITY" } ) ) {
		s.init = true;
		const QByteArray pin = wwEnv( "WW_FX_PARITY" ).trimmed();
		s.pinned = !pin.isEmpty();
		// lane DEFAULTS1: ships on (new key so an old OFF does not stick); WW_DEFAULTS1_RED=1 = the old row
		s.on = s.pinned ? pin != "0" : wwEnv( "WW_DEFAULTS1_RED" ).trimmed() == "1"
			? QSettings().value( QStringLiteral( "WW/FxParity" ), false ).toBool()
			: QSettings().value( QStringLiteral( "WW/FxParity2" ), true ).toBool();
		s.reds = 0;
		for ( const QByteArray & t : wwEnv( "WW_FX_PARITY_RED" ).split( ',' ) ) {
			const QByteArray r = t.trimmed();
			s.reds |= r == "color" ? WwFxColor : r == "drag" ? WwFxDrag : r == "subtex" ? WwFxSubTex
				: r == "law" ? WwFxLaw : r == "soft" ? WwFxSoft : r == "range" ? WwFxDragRange : r == "bomb" ? WwFxBomb
				: r == "gravity" ? WwFxGravity : 0;
		}
	}
	return s;
}
}	// namespace

bool wwFxParityOn()
{
	return fxp().on;
}

void wwFxParitySetOn( bool on )
{
	FxpState & s = fxp();
	if ( s.pinned )
		return;
	s.on = on;
	QSettings().setValue( QStringLiteral( "WW/FxParity2" ), on );
}

int wwFxParityRules()
{
	FxpState & s = fxp();
	const int r = s.on ? ( WwFxAll & ~s.reds ) : 0;
	if ( r != s.said ) {
		s.said = r;
		fprintf( stderr, "particle game rules: %s (rules %d, reds %d)\n", s.on ? "on" : "off", r, s.reds );
	}
	return r;
}
