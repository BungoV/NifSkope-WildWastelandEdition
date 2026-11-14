// lane VKOBJ5 (11-15): when the soft effects' depth (Scene::grabEffectDepth, gl/glscene.cpp) asks the present device for
// its shared target again after a refusal. Pure rules, no GL; tests/vkframe/fxdepthshare.sh runs them with a forced
// first refusal.
#ifndef WW_FXDEPTHSHARE_H
#define WW_FXDEPTHSHARE_H

namespace WwFxDepthShare {

/* A refusal keeps GL's own texture. The next ask comes on a size change, else after kGap second passes, at most kTries
 * asks per size: the device may not be up yet at the first pass, and a device that keeps refusing costs kTries asks. */
struct Retry
{
	static constexpr int kGap = 120, kTries = 4;
	int failW = 0, failH = 0;  //!< the size the last refusal was for (0: none since the last grant)
	int failPass = 0;          //!< the second pass of the last refusal
	int tries = 0;             //!< refusals at failW x failH
	int refusals = 0;          //!< refusals since the last grant (the recovery line reports it)
	bool said = false;         //!< the refusal reason was logged

	//! Ask for the shared target now? shared: the texture is shared already; hasTex: a texture exists.
	bool ask( int w, int h, int pass, bool shared, bool hasTex ) const
	{
		if ( shared || !hasTex )
			return true;
		if ( w != failW || h != failH )
			return true;
		return tries < kTries && pass - failPass >= kGap;
	}
	//! Rebuild the texture this pass? (GL's own texture at the same size is rebuilt only when an ask is due.)
	bool due( int w, int h, int pass, bool shared, bool hasTex, int texW, int texH, bool vkOn ) const
	{
		if ( !hasTex || texW != w || texH != h )
			return true;
		return vkOn && !shared && ask( w, h, pass, shared, hasTex );
	}
	//! A refusal; true the first time (log the reason once).
	bool refused( int w, int h, int pass )
	{
		if ( w != failW || h != failH ) {
			failW = w;
			failH = h;
			tries = 0;
		}
		++tries;
		++refusals;
		failPass = pass;
		const bool first = !said;
		said = true;
		return first;
	}
	//! A grant; returns the refusals before it (above 0: log the recovery).
	int granted()
	{
		const int n = refusals;
		failW = failH = tries = refusals = 0;
		return n;
	}
};

} // namespace WwFxDepthShare

#endif
