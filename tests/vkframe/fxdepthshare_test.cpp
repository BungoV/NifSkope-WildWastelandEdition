// lane VKOBJ5 (11-15): the soft effects' depth asks for its shared target again after a refusal
// (src/features/vkframe/fxdepthshare.h). A model of Scene::grabEffectDepth's texture block (gl/glscene.cpp) runs passes
// against a fake present device. -DFXDEPTHSHARE_RED uses the old rule (ask only when shared or no texture yet, rebuild
// only on a size change) and must fail. Last line: `FXDEPTHSHARE OK` (rc 0) or `FXDEPTHSHARE FAILED n` (rc 1).
#include "features/vkframe/fxdepthshare.h"

#include <cstdio>

static int fails = 0;
static void check( bool ok, const char * what )
{
	std::printf( "%s %s\n", ok ? "ok  " : "FAIL", what );
	fails += ok ? 0 : 1;
}

// the fake device: refuses its first `refuse` asks (-1: all), then grants
struct Device
{
	int refuse = 0, asks = 0;
	bool give() { ++asks; if ( refuse < 0 || refuse > 0 ) { refuse -= refuse > 0; return false; } return true; }
};

// Scene's fxDepth state and the block, line for line with glscene.cpp
struct Scene
{
	WwFxDepthShare::Retry retry;
	bool hasTex = false, shared = false;
	int texW = 0, texH = 0, glLive = 0, warned = 0, recovered = 0;

	bool due( int w, int h, int pass, bool vkOn ) const
	{
#ifdef FXDEPTHSHARE_RED
		(void)pass; (void)vkOn;
		return !hasTex || texW != w || texH != h;
#else
		return retry.due( w, h, pass, shared, hasTex, texW, texH, vkOn );
#endif
	}
	bool ask( int w, int h, int pass ) const
	{
#ifdef FXDEPTHSHARE_RED
		(void)w; (void)h; (void)pass;
		return shared || !hasTex;
#else
		return retry.ask( w, h, pass, shared, hasTex );
#endif
	}
	void grab( Device & dev, int w, int h, int pass, bool vkOn )
	{
		if ( !due( w, h, pass, vkOn ) )
			return;
		const bool asked = vkOn && ask( w, h, pass );
		if ( asked && dev.give() ) {
			if ( hasTex && !shared )
				--glLive;
			hasTex = shared = true;
			if ( retry.granted() )
				++recovered;
		} else {
			if ( asked && retry.refused( w, h, pass ) )
				++warned;
			if ( shared )
				hasTex = shared = false;
		}
		if ( !hasTex ) {
			hasTex = true;
			++glLive;
		}
		texW = w;
		texH = h;
	}
};

static void run( Scene & s, Device & d, int w, int h, int from, int to, bool vkOn = true )
{
	for ( int p = from; p < to; ++p )
		s.grab( d, w, h, p, vkOn );
}

int main()
{
	std::setvbuf( stdout, nullptr, _IONBF, 0 );
	using WwFxDepthShare::Retry;

	{	// forced first refusal at one size: shared after kGap passes, GL's texture freed, one warning, one recovery line
		Scene s; Device d; d.refuse = 1;
		run( s, d, 800, 600, 0, Retry::kGap + 1 );
		check( s.shared, "a forced first refusal recovers at the same size within kGap passes" );
		check( s.glLive == 0, "the recovered target frees GL's own texture" );
		check( d.asks == 2, "two asks: the refusal and the grant" );
		check( s.warned == 1 && s.recovered == 1, "the reason logged once, the recovery once" );
		run( s, d, 800, 600, Retry::kGap + 1, 5000 );
		check( d.asks == 2, "no ask once shared at the same size" );
	}
	{	// a device that always refuses: kTries asks per size, one warning ever, GL's texture kept
		Scene s; Device d; d.refuse = -1;
		run( s, d, 800, 600, 0, 20000 );
		check( d.asks == Retry::kTries, "a refusing device is asked kTries times per size" );
		check( !s.shared && s.glLive == 1, "GL's own texture stays" );
		run( s, d, 1024, 768, 20000, 40000 );
		check( d.asks == 2 * Retry::kTries, "a size change asks again, bounded the same" );
		check( s.warned == 1 && s.glLive == 1, "still one warning, one GL texture" );
	}
	{	// refusals used up, then the device comes up: the next size change recovers
		Scene s; Device d; d.refuse = Retry::kTries;
		run( s, d, 800, 600, 0, 20000 );
		check( !s.shared, "kTries refusals leave GL's texture" );
		run( s, d, 640, 480, 20000, 20001 );
		check( s.shared && s.glLive == 0, "the next size change recovers" );
	}
	{	// WW_VKOBJ_SHADE off: never asked; switched on live at the same size: asked at the next pass
		Scene s; Device d;
		run( s, d, 800, 600, 0, 500, false );
		check( d.asks == 0 && !s.shared && s.glLive == 1, "switch off: no ask, GL's texture" );
		run( s, d, 800, 600, 500, 501, true );
		check( s.shared && s.glLive == 0 && s.warned == 0, "switched on live: shared at the next pass" );
	}
	{	// refused at a size, shared after a later grant, then a size change keeps asking (shared) with no warning
		Scene s; Device d; d.refuse = 1;
		run( s, d, 800, 600, 0, Retry::kGap + 1 );
		run( s, d, 900, 700, Retry::kGap + 1, Retry::kGap + 2 );
		check( s.shared && d.asks == 3 && s.warned == 1, "a shared target follows a size change" );
	}

	if ( fails ) {
		std::printf( "FXDEPTHSHARE FAILED %d\n", fails );
		return 1;
	}
	std::printf( "FXDEPTHSHARE OK\n" );
	return 0;
}
