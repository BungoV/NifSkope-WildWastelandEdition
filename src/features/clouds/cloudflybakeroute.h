#pragma once
// Lane VKCLOUD5 (2026-11-16): the fly-through box bake's route, one place for the app (cloudfly.cpp runPass) and the gate
// (tests/vkframe/flybakeview.cpp). No Qt: compiles under WW_VKPRESENT_NO_QT.
// wantVk: try vkFn( from, to ) (one Vulkan run of slices [from, to)); on success GL draws nothing (onVk true).
// A refused or failed Vulkan run falls back to glFn( k ) for every slice (onVk false).
// redGlWrites (gate red): GL draws the slices after a Vulkan run too, which the gate's GL_SAMPLES_PASSED must catch.
// Returns false only when a GL slice failed.

template <class VkFn, class GlFn>
bool wwFlyBakeRoute( bool wantVk, bool redGlWrites, VkFn && vkFn, GlFn && glFn, int from, int to, bool & onVk )
{
	onVk = wantVk && vkFn( from, to );
	if ( onVk && !redGlWrites )
		return true;
	bool ok = true;
	for ( int k = from; k < to && ok; k++ )
		ok = glFn( k );
	return ok;
}
