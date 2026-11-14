// Lane VKOBJ3 (2026-11-13, plan row G4 slice 3): the texture cache's BC twins and their handles in the bindless table.
// Included by vkpresent.cpp after vkbindless.inc (it uses farTwin / farTwinFind / farCards / fcFormat of vkfarcards.inc and
// blAdd / blDrop / blTranslate / blGravesBlack of vkbindless.inc). The API: vkobjtex.h.
#include "features/vkframe/vkobjtex.h"

#include <algorithm>
#include <unordered_map>
#include <vector>

namespace
{
// the twins' keys in farTwin's map: a tag no address carries (user space is below 2^47), then a serial
constexpr std::uintptr_t kOtKeyTag = std::uintptr_t( 0x0B7E000000000000ull );

struct OtTwin
{
	std::uintptr_t key = 0, prevKey = 0;  // prevKey: the twin made before (the red's wrong image)
	std::vector<std::uint64_t> handles;
	double mb = 0.0;
};

struct Ot
{
	std::unordered_map<GLuint, OtTwin> twins;
	std::vector<std::uintptr_t> gone;
	std::uintptr_t serial = 0, lastKey = 0;
	std::vector<unsigned char> vkRows;
	WwVkPresent::ObjTexStat st;
	// VKOBJ4: the batch's hand-over (GL signals `glSem` with the new twins as GENERAL, one submit waits it) and its load
	VkSemaphore sem = VK_NULL_HANDLE;
	GLuint glSem = 0;
	void * semCtx = nullptr;
	VkFence fence = VK_NULL_HANDLE;
	bool semTried = false;
	std::string semWhy;
	std::vector<GLuint> pending;
	int loadTwins = 0, loadFlushes = 0;
	double loadMs = 0.0;
};

Ot & ot()
{
	static Ot o;
	return o;
}

// the cache deleted the GL name: farTwin's free must not delete a name glGenTextures may have handed out again
void otForgetName( std::uintptr_t key )
{
	auto ft = farCards().twins.find( key );
	if ( ft != farCards().twins.end() )
		ft->second.tex = 0;
}

// the twins gone since the last sweep: one drain and idle wait, the freed table elements black, then each twin's free
void otSweep()
{
	Ot & O = ot();
	if ( O.gone.empty() )
		return;
	if ( !vk().ok ) {  // the device went: shutdown's farCardsFree freed them
		O.gone.clear();
		return;
	}
	const auto t0 = std::chrono::steady_clock::now();
	drain();
	vk().vkDeviceWaitIdle( vk().dev );
	blGravesBlack();
	for ( const std::uintptr_t key : O.gone ) {
		blSwzForget( key );  // VKOBJ4: its swizzled views first
		WwVkPresent::farTwinDrop( key );  // its own drain and idle wait find nothing left
		O.st.freed++;
	}
	O.gone.clear();
	O.st.freeMs += std::chrono::duration<double, std::milli>( std::chrono::steady_clock::now() - t0 ).count();
}

// VKOBJ4: the batch's semaphore pair and fence, made once per device (false: twins take farTwin's own layout change)
bool otSemReady()
{
	Ot & O = ot();
	if ( O.sem && O.semCtx == gl().ctx )
		return true;
	if ( O.semTried )
		return false;
	O.semTried = true;
	VkFenceCreateInfo fi { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
	if ( !gl().signalSemaphore || !exportSem( O.sem, O.glSem, O.semWhy )
		|| vk().vkCreateFence( vk().dev, &fi, nullptr, &O.fence ) != VK_SUCCESS ) {
		if ( O.semWhy.empty() )
			O.semWhy = "no glSignalSemaphoreEXT or no fence for the batch";
		return false;
	}
	O.semCtx = gl().ctx;
	return true;
}

/* VKOBJ4: the batch's flush: GL signals once with every twin made since (GL's uploads before it in its stream), one
 * submit waits it (the twins are GENERAL on the present device from there), one fence wait */
void otFlush()
{
	Ot & O = ot();
	if ( O.pending.empty() )
		return;
	const auto t0 = std::chrono::steady_clock::now();
	Vk & v = vk();
	Gl & g = gl();
	bool ok = v.ok && g.ok && O.sem && O.semCtx == g.ctx;
	if ( ok ) {
		const std::vector<GLenum> lay( O.pending.size(), kLayoutGeneral );
		g.signalSemaphore( O.glSem, 0, nullptr, GLuint( O.pending.size() ), O.pending.data(), lay.data() );
		g.flush();  // Vulkan's wait sees GL's signal only once it is flushed
		drain();    // the queue is the worker's
		const VkPipelineStageFlags stage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
		VkSubmitInfo si { VK_STRUCTURE_TYPE_SUBMIT_INFO };
		si.waitSemaphoreCount = 1;
		si.pWaitSemaphores = &O.sem;
		si.pWaitDstStageMask = &stage;
		ok = v.vkQueueSubmit( v.queue, 1, &si, O.fence ) == VK_SUCCESS
			&& v.vkWaitForFences( v.dev, 1, &O.fence, VK_TRUE, 1000000000ull ) == VK_SUCCESS && v.vkResetFences( v.dev, 1, &O.fence ) == VK_SUCCESS;
	}
	O.st.flushes++;
	O.st.flushed += int( O.pending.size() );
	if ( !ok ) {
		O.st.flushFails++;
		O.st.why = "the batch's hand-over (GL signal, one submit waiting it) failed";
	}
	O.pending.clear();
	const double ms = std::chrono::duration<double, std::milli>( std::chrono::steady_clock::now() - t0 ).count();
	O.st.flushMs += ms;
	O.loadMs += ms;
	O.loadFlushes++;
}

// VKOBJ4: a twin's GL name leaves the batch (the cache deletes it before the flush)
void otUnpend( GLuint tex )
{
	auto & p = ot().pending;
	p.erase( std::remove( p.begin(), p.end(), tex ), p.end() );
}
}  // namespace

namespace WwVkPresent
{
bool objTexMake( unsigned glFmt, int w, int h, int levels, unsigned & tex, std::string & why )
{
	Ot & O = ot();
	tex = 0;
	auto refuse = [&]( const std::string & r ) {
		why = r;
		O.st.refused++;
		O.st.why = r;
		return false;
	};
	if ( !fcFormat( GLenum( glFmt ) ).block )
		return refuse( "GL format " + std::to_string( glFmt ) + " is not block-compressed" );
	otSweep();
	const auto t0 = std::chrono::steady_clock::now();
	const std::uintptr_t key = kOtKeyTag | ++O.serial;
	fcLayoutByGl = vk().ok && otSemReady();  // VKOBJ4: the layout comes with the batch's flush
	const bool batched = fcLayoutByGl;
	const bool made = farTwin( key, 0, w, h, 1, glFmt, tex, why, levels );
	fcLayoutByGl = false;
	if ( !made )
		return refuse( why );
	const double ms = std::chrono::duration<double, std::milli>( std::chrono::steady_clock::now() - t0 ).count();
	O.st.makeMs += ms;
	O.loadMs += ms;
	O.loadTwins++;
	auto old = O.twins.find( GLuint( tex ) );
	if ( old != O.twins.end() ) {  // a deleted twin's name the cache never reported: its Vulkan half goes now
		for ( const std::uint64_t hd : old->second.handles )
			blDrop( hd );
		otForgetName( old->second.key );
		O.gone.push_back( old->second.key );
		O.twins.erase( old );
	}
	otUnpend( GLuint( tex ) );
	if ( batched ) {
		O.pending.push_back( GLuint( tex ) );
		O.st.batched++;
	}
	OtTwin & t = O.twins[GLuint( tex )];
	t.key = key;
	t.prevKey = O.lastKey;
	const Shared * s = farTwinFind( key );
	t.mb = s ? s->mb : 0.0;
	O.lastKey = key;
	O.st.made++;
	return true;
}

bool objTexIs( unsigned tex )
{
	const Ot & O = ot();
	return tex && !O.twins.empty() && O.twins.count( GLuint( tex ) );
}

void objTexHandle( unsigned tex, std::uint64_t handle, GlProc gp, int red )
{
	Ot & O = ot();
	if ( !tex || !handle || O.twins.empty() )
		return;
	auto it = O.twins.find( GLuint( tex ) );
	if ( it == O.twins.end() )
		return;
	BlSampler s;
	std::string w;
	const std::uintptr_t key = red == 1 && it->second.prevKey ? it->second.prevKey : it->second.key;
	const bool read = blSamplerRead( tex, s, gp, w );
	if ( read && red == 2 )  // VKOBJ4 red `swz`: the swizzle's own view dropped
		for ( int c = 0; c < 4; c++ )
			s.swizzle[c] = GLint( 0x1903 + c );
	if ( !read || !blAdd( handle, key, s, w ) ) {
		O.st.addFails++;
		O.st.addWhy = w;
		return;
	}
	it->second.handles.push_back( handle );
	O.st.adds++;
}

void objTexGone( unsigned tex )
{
	Ot & O = ot();
	if ( !tex || O.twins.empty() )
		return;
	auto it = O.twins.find( GLuint( tex ) );
	if ( it == O.twins.end() )
		return;
	for ( const std::uint64_t h : it->second.handles )
		blDrop( h );
	otForgetName( it->second.key );
	otUnpend( GLuint( tex ) );
	O.gone.push_back( it->second.key );
	O.twins.erase( it );
	O.st.gone++;
}

void objTexRows( const void * glRows, std::size_t n )
{
	Ot & O = ot();
	if ( !glRows || !n )
		return;
	O.vkRows.resize( n * kBlRowBytes );
	O.st.whole += blTranslate( glRows, O.vkRows.data(), n );
	O.st.rows += n;
}

void objTexFlush()
{
	otFlush();
}

ObjTexStat objTexFrame()
{
	Ot & O = ot();
	otFlush();
	otSweep();
	// VKOBJ4: a cell's load = the frames that make twins until kOtLoadQuiet frames make none (then the next one starts)
	constexpr int kOtLoadQuiet = 30;
	static int quiet = 0;
	quiet = O.st.made ? 0 : quiet + 1;
	const BlStat b = takeBlStat();
	ObjTexStat s = O.st;
	s.loadTwins = O.loadTwins;
	s.loadFlushes = O.loadFlushes;
	s.loadMs = O.loadMs;
	if ( quiet >= kOtLoadQuiet )
		O.loadTwins = O.loadFlushes = 0, O.loadMs = 0.0;
	s.twins = int( O.twins.size() );
	for ( const auto & t : O.twins ) {
		s.handles += int( t.second.handles.size() );
		s.mb += t.second.mb;
	}
	s.images = b.images;
	s.samplers = b.samplers;
	s.misses = b.misses;
	s.swizzled = b.swizzled;
	if ( s.addWhy.empty() && b.fails )
		s.addWhy = b.failWhy;
	O.st = ObjTexStat();
	return s;
}

int objTexFree()
{
	Ot & O = ot();
	const int n = int( O.twins.size() );
	for ( const auto & t : O.twins )
		otForgetName( t.second.key );
	O.twins.clear();
	O.gone.clear();
	O.lastKey = 0;
	O.pending.clear();  // VKOBJ4: the batch's semaphores go with the device
	if ( O.sem || O.fence ) {
		drain();
		vk().vkDeviceWaitIdle( vk().dev );
		if ( O.sem )
			vk().vkDestroySemaphore( vk().dev, O.sem, nullptr );
		if ( O.fence )
			vk().vkDestroyFence( vk().dev, O.fence, nullptr );
	}
	if ( O.glSem && gl().ok && O.semCtx == gl().ctx )
		gl().deleteSemaphores( 1, &O.glSem );
	O.sem = VK_NULL_HANDLE;
	O.fence = VK_NULL_HANDLE;
	O.glSem = 0;
	O.semCtx = nullptr;
	O.semTried = false;
	O.loadTwins = O.loadFlushes = 0;
	O.loadMs = 0.0;
	return n;
}
}  // namespace WwVkPresent
