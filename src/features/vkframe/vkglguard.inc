// Lane VKFLIGHT4 (2026-11-16): the Vulkan view's GL guard, bungo's 11-16 rule "OpenGL ... not active while vulkan
// draws" as one number. Every route's `<route>OnGl( why )` (the draw or pass that went to GL instead of its Vulkan
// twin) adds here: N = those calls (draws or passes the routes count), M = the routes that went to GL at least once.
// frame() says `vk view gl: N draws, M passes (top: <route> n, ...)` once a say window while the view is on Vulkan;
// ready = N 0, M 0. What it cannot see: GL work with no route function (the editor overlays, GL passes never given a
// Vulkan twin): those stay in the `vk view` route line's "still on GL" list.
namespace
{
struct GlGuard
{
	std::mutex mx;	// routes on the worker or a loader thread call too
	std::map<std::string, long long> zones;
};

GlGuard & glGuardS()
{
	static GlGuard * g = new GlGuard;	// leaked: exit never orders it against a late caller
	return *g;
}
}  // namespace

namespace WwVkPresent
{
void glGuard( const char * zone, long long draws )
{
	GlGuard & g = glGuardS();
	std::lock_guard<std::mutex> lk( g.mx );
	g.zones[zone ? zone : "?"] += draws;
}

std::string glGuardTake( long long * drawsOut, int * passesOut )
{
	GlGuard & g = glGuardS();
	std::map<std::string, long long> z;
	{
		std::lock_guard<std::mutex> lk( g.mx );
		z.swap( g.zones );
	}
	std::vector<std::pair<long long, std::string>> top;
	long long n = 0;
	for ( const auto & e : z ) {
		n += e.second;
		top.emplace_back( e.second, e.first );
	}
	std::sort( top.begin(), top.end(), []( const auto & a, const auto & b ) { return a.first != b.first ? a.first > b.first : a.second < b.second; } );
	std::string s = "vk view gl: " + std::to_string( n ) + " draws, " + std::to_string( top.size() ) + " passes";
	if ( !top.empty() ) {
		s += " (top:";
		for ( size_t i = 0; i < top.size() && i < 8; i++ )
			s += std::string( i ? ", " : " " ) + top[i].second + " " + std::to_string( top[i].first );
		s += ")";
	}
	if ( drawsOut )
		*drawsOut = n;
	if ( passesOut )
		*passesOut = int( top.size() );
	return s;
}
}  // namespace WwVkPresent
