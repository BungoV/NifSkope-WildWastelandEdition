#ifndef WW_FXPARITY_H
#define WW_FXPARITY_H

/* lane FXPARITY1: "Particle Game Rules" (Scene window row, key WW/FxParity, pin WW_FX_PARITY=0|1). On: particles are
 * simulated and drawn by the game's rules (src/gl/fxparityrules.h, res/shaders/particles.frag). Off (the default):
 * the viewer's particle look as before. Red WW_FX_PARITY_RED=color,drag,subtex,law,soft,range,bomb,gravity puts one rule back as before. */

enum WwFxRule
{
	WwFxColor = 1,		// BSPSysSimpleColor colour and alpha ramps
	WwFxDrag = 2,		// linear drag, capped at the whole axis share
	WwFxSubTex = 4,		// flipbook steps spread over the life
	WwFxLaw = 8,		// the effect shader's law: alpha squared, no falloff, palette row, emissive as written
	WwFxSoft = 16,		// the soft fade near surfaces (cell view)
	WwFxDragRange = 32,	// lane FXPARITY2: the drag's Range / Range Falloff around the drag object
	WwFxBomb = 64,		// lane FXPARITY2: NiPSysBombModifier
	WwFxGravity = 128,	// lane FXPARITY2: gravity x1.6, spherical toward the centre, decay, turbulence
	WwFxAll = 255
};

bool wwFxParityOn();
void wwFxParitySetOn( bool on );
//! The rules in force now: 0 while off, else every rule less the reds; re-read after a live set
int wwFxParityRules();

#endif
