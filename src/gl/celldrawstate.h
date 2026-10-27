#ifndef CELLDRAWSTATE_H
#define CELLDRAWSTATE_H

/* lane PERF3: one read of blend / active unit / viewport per draw, shared by the census and the AO/SSR/SSGI hooks
 * (each glGet is a driver sync). Valid only inside wwProgramCensus; outside it every call reads GL.
 * WW_DRAW_GLCACHE=0 or the red WW_DRAW_GLCACHE_RED=1 reads GL at every call again (the old path and its cost). */
bool wwDrawBlend();
int wwDrawActiveTexture();
void wwDrawViewport( int vp[4] );
//! a hook changed blend inside the census (the cache reads it again)
void wwDrawGlCacheDirty();
//! lane CELLUNI4: a hook wrote blend on/off inside the census (the cache keeps it; the red reads it again)
void wwDrawGlCacheBlend( bool on );

//! lane PERF3: the AO opaque pass skips the SSR/SSGI per-draw hooks (the main pass sets them again per draw);
//! WW_CELL_AO_LEAN=0 or the red WW_CELL_AO_LEAN_RED=1 runs them
bool wwCellAoLean();

struct WwDrawGlScope
{
	WwDrawGlScope();
	~WwDrawGlScope();
};

#endif
