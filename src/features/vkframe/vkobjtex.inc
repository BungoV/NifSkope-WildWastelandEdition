// Lane VKOBJ3 (2026-11-13, plan row G4 slice 3): the texture cache's BC twins and their handles in the bindless table.
// Included by vkpresent.cpp after vkbindless.inc (it uses farTwin / farTwinFind / farCards / fcFormat of vkfarcards.inc and
// blAdd / blDrop / blTranslate / blGravesBlack of vkbindless.inc). The API: vkobjtex.h.
#include "features/vkframe/vkobjtex.h"

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
		WwVkPresent::farTwinDrop( key );  // its own drain and idle wait find nothing left
		O.st.freed++;
	}
	O.gone.clear();
	O.st.freeMs += std::chrono::duration<double, std::milli>( std::chrono::steady_clock::now() - t0 ).count();
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
	if ( !farTwin( key, 0, w, h, 1, glFmt, tex, why, levels ) )
		return refuse( why );
	O.st.makeMs += std::chrono::duration<double, std::milli>( std::chrono::steady_clock::now() - t0 ).count();
	auto old = O.twins.find( GLuint( tex ) );
	if ( old != O.twins.end() ) {  // a deleted twin's name the cache never reported: its Vulkan half goes now
		for ( const std::uint64_t hd : old->second.handles )
			blDrop( hd );
		otForgetName( old->second.key );
		O.gone.push_back( old->second.key );
		O.twins.erase( old );
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
	if ( !blSamplerRead( tex, s, gp, w ) || !blAdd( handle, key, s, w ) ) {
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

ObjTexStat objTexFrame()
{
	Ot & O = ot();
	otSweep();
	const BlStat b = takeBlStat();
	ObjTexStat s = O.st;
	s.twins = int( O.twins.size() );
	for ( const auto & t : O.twins ) {
		s.handles += int( t.second.handles.size() );
		s.mb += t.second.mb;
	}
	s.images = b.images;
	s.samplers = b.samplers;
	s.misses = b.misses;
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
	return n;
}
}  // namespace WwVkPresent
