#include "fxparity.h"

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
		s.on = s.pinned ? pin != "0" : QSettings().value( QStringLiteral( "WW/FxParity" ), false ).toBool();
		s.reds = 0;
		for ( const QByteArray & t : wwEnv( "WW_FX_PARITY_RED" ).split( ',' ) ) {
			const QByteArray r = t.trimmed();
			s.reds |= r == "color" ? WwFxColor : r == "drag" ? WwFxDrag : r == "subtex" ? WwFxSubTex
				: r == "law" ? WwFxLaw : r == "soft" ? WwFxSoft : 0;
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
	QSettings().setValue( QStringLiteral( "WW/FxParity" ), on );
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
