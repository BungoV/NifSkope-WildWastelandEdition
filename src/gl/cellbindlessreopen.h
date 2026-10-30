#ifndef CELLBINDLESSREOPEN_H
#define CELLBINDLESSREOPEN_H

/* lane BINDREOPEN1: the ORDER of the bindless teardown, apart from GL so tests/bindreopen1 runs the same steps on a
 * mock GL that aborts on a use after delete. G supplies the steps (src/gl/cellbindless.cpp: the real GL; the test: the
 * mock). Red WW_BINDLESS_RED=reopenraw (`raw`): the order before the lane (WwBl::forgetAll: glFinish, handles made
 * non-resident, the frozen table cleared; the staged row, the ring, the geometry pool and a dead context's Ctx kept). */

namespace WwBlReopen
{
//! a reopen (TexCache::flush: Scene::clear deletes every shape, then every cached texture goes)
template <class G> void teardown( G & g, bool raw )
{
	if ( raw ) {
		g.finish();
		g.handlesNonResident();
		g.frozenClear();
		return;
	}
	g.fenceWait();	// every row, multi-draw command and pooled draw issued so far is done on the GPU
	g.ringReset();	// the segment fences are all passed: dropped; cursors to 0; no run open
	g.rowZero();	// the staged row names no handle of the textures about to go
	g.handlesNonResident();	// before TexCache deletes their textures
	g.frozenClear();
	g.poolDrop();	// the entries of the deleted shapes and the arenas' buffers: the next document starts at the floor
}

//! the context that owns the state is about to be destroyed
template <class G> void contextGone( G & g, bool raw )
{
	if ( raw )
		return;	// the Ctx stays, its mapped pointers name buffers of a dead context
	g.fenceWait();
	g.handlesNonResident();
	g.unmapRings();	// the persistent maps go before their buffers
	g.deleteBuffers();
	g.forgetCtx();	// a new context at the same address starts from nothing
}
}

#endif
