/* lane VKOBJ4 slice 6: a class pool (cellbindless.cpp's layout: one tightly packed GL_FLOAT buffer per attribute, u16
 * indices) through objBlPoolCopy into its twins, read back on the present device (vkCmdCopyBuffer after GL's signal) and
 * compared byte for byte; then objBlPoolLayout as the bl twin's vertex input (made), the absent locations on the
 * constant binding. Prints `BLVIEW pool ... EXACT|FAIL`, `BLVIEW pool layout ...` and `BLVIEW RED pool ...` (must FAIL).
 * Included inside namespace blv after gvBuild's helpers (it borrows gv's command buffer, fence and semaphores). */

struct PoolRb
{
	CsmBuf rb;
	std::vector<VkBuffer> src;
	std::vector<VkDeviceSize> bytes, at;
};
PoolRb poolRb;

bool poolWork( std::string & why )
{
	Vk & v = vk();
	v.vkResetFences( v.dev, 1, &gv.fence );
	v.vkResetCommandBuffer( gv.cmd, 0 );
	VkCommandBufferBeginInfo bi { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
	bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	static const auto copyBuffer = reinterpret_cast<PFN_vkCmdCopyBuffer>( v.vkGetDeviceProcAddr( v.dev, "vkCmdCopyBuffer" ) );
	if ( !copyBuffer )
		return why = "no vkCmdCopyBuffer", false;
	v.vkBeginCommandBuffer( gv.cmd, &bi );
	for ( std::size_t i = 0; i < poolRb.src.size(); i++ ) {
		const VkBufferCopy c { 0, poolRb.at[i], poolRb.bytes[i] };
		copyBuffer( gv.cmd, poolRb.src[i], poolRb.rb.buf, 1, &c );
	}
	if ( v.vkEndCommandBuffer( gv.cmd ) != VK_SUCCESS )
		return why = "vkEndCommandBuffer (the pool readback) failed", false;
	const VkPipelineStageFlags waitAt = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
	VkSubmitInfo si { VK_STRUCTURE_TYPE_SUBMIT_INFO };
	si.waitSemaphoreCount = 1;
	si.pWaitSemaphores = &gv.glDone;
	si.pWaitDstStageMask = &waitAt;
	si.commandBufferCount = 1;
	si.pCommandBuffers = &gv.cmd;
	si.signalSemaphoreCount = 1;
	si.pSignalSemaphores = &gv.vkDone;
	if ( v.vkQueueSubmit( v.queue, 1, &si, gv.fence ) != VK_SUCCESS )
		return why = "vkQueueSubmit (the pool readback) failed", false;
	if ( v.vkWaitForFences( v.dev, 1, &gv.fence, VK_TRUE, 5000000000ull ) != VK_SUCCESS )
		return why = "the pool readback did not finish", false;
	return true;
}

// one crossing of the pool: objBlPoolCopy (red as given), GL's signal on the twins, the readback, GL waits ours
bool poolCross( std::uintptr_t key, const WwVkPresent::ObjBlPool & p, int red, std::vector<unsigned char> & got, std::string & why )
{
	using namespace WwVkPresent;
	Gl & g = gl();
	std::vector<unsigned> names;
	if ( !objBlPoolCopy( key, p, names, why, red ) )
		return false;
	VkBuffer vb[17] = {}, ib = VK_NULL_HANDLE;
	if ( !objBlPoolBinds( key, vb, ib ) )
		return why = "objBlPoolBinds: no twins", false;
	poolRb.src.clear();
	poolRb.bytes.clear();
	poolRb.at.clear();
	VkDeviceSize total = 0;
	std::uint64_t m = p.attrMask;
	for ( int i = 0; m && i < 16; i++, m >>= 4 ) {
		const VkDeviceSize n = VkDeviceSize( m & 15 );
		if ( !n )
			continue;
		poolRb.src.push_back( vb[i] );
		poolRb.bytes.push_back( VkDeviceSize( p.verts ) * n * 4 );
		poolRb.at.push_back( total );
		total += VkDeviceSize( p.verts ) * n * 4;
	}
	poolRb.src.push_back( ib );
	poolRb.bytes.push_back( VkDeviceSize( p.indices ) * 2 );
	poolRb.at.push_back( total );
	total += VkDeviceSize( p.indices ) * 2;
	if ( !csmBuf( poolRb.rb, total, false, VK_BUFFER_USAGE_TRANSFER_DST_BIT, why ) )
		return false;
	std::memset( poolRb.rb.ptr, 0, std::size_t( total ) );
	if ( const GLenum e = glGetError(); e != GL_NO_ERROR )
		return why = "GL error " + std::to_string( e ) + " after objBlPoolCopy", false;
	std::vector<GLenum> lay;
	g.signalSemaphore( gv.glDoneGl, GLuint( names.size() ), names.data(), 0, nullptr, lay.data() );
	g.flush();
	Worker & W = wk();
	std::uint64_t want = 0;
	{
		std::lock_guard<std::mutex> lk( W.m );
		want = W.toneDone + 1;
	}
	Work j;
	j.k = -1;
	j.fn = poolWork;
	hand( j );
	bool ok = false;
	{
		std::unique_lock<std::mutex> lk( W.m );
		W.idle.wait( lk, [&W, want]() { return W.toneDone >= want; } );
		ok = W.toneOk;
		why = W.toneWhy;
	}
	if ( !ok )
		return false;
	g.waitSemaphore( gv.vkDoneGl, GLuint( names.size() ), names.data(), 0, nullptr, lay.data() );
	glFinish();
	if ( const GLenum e = glGetError(); e != GL_NO_ERROR )
		return why = "GL error " + std::to_string( e ) + " at the semaphores", false;
	got.assign( static_cast<unsigned char *>( poolRb.rb.ptr ), static_cast<unsigned char *>( poolRb.rb.ptr ) + total );
	return true;
}

bool poolGate( const std::string & spvDir )
{
	using namespace WwVkPresent;
	constexpr GLenum kCopyWrite = 0x8F37, kStatic = 0x88E4;
	// fo4_default.vert's locations a cell class carries: position 3, colour 4, normal 3, tangent 3, bitangent 3, uv 2 (7)
	const std::uint64_t mask = 3ull | 4ull << 4 | 3ull << 8 | 3ull << 12 | 3ull << 16 | 2ull << 28;
	ObjBlPool p;
	p.attrMask = mask;
	p.verts = 3001;
	p.indices = 9000;
	std::mt19937 rng( 71 );
	std::uniform_real_distribution<float> uf( -1000.0f, 1000.0f );
	std::vector<unsigned char> want;
	std::uint64_t m = mask;
	for ( int i = 0; m && i < 16; i++, m >>= 4 ) {
		const std::size_t n = std::size_t( m & 15 );
		if ( !n )
			continue;
		std::vector<float> a( p.verts * n );
		for ( float & x : a )
			x = uf( rng );
		f.genBuffers( 1, &p.vbo[i] );
		f.bindBuffer( kCopyWrite, p.vbo[i] );
		f.bufferData( kCopyWrite, std::ptrdiff_t( a.size() * 4 ), a.data(), kStatic );
		const unsigned char * b = reinterpret_cast<const unsigned char *>( a.data() );
		want.insert( want.end(), b, b + a.size() * 4 );
	}
	std::vector<std::uint16_t> idx( p.indices );
	for ( std::uint16_t & x : idx )
		x = std::uint16_t( rng() % p.verts );
	f.genBuffers( 1, &p.ebo );
	f.bindBuffer( kCopyWrite, p.ebo );
	f.bufferData( kCopyWrite, std::ptrdiff_t( idx.size() * 2 ), idx.data(), kStatic );
	f.bindBuffer( kCopyWrite, 0 );
	if ( const GLenum e = glGetError(); e != GL_NO_ERROR ) {
		std::printf( "BLVIEW pool FAIL (GL error 0x%x making the pool)\n", unsigned( e ) );
		return false;
	}
	const unsigned char * ib = reinterpret_cast<const unsigned char *>( idx.data() );
	want.insert( want.end(), ib, ib + idx.size() * 2 );
	static int keyHome = 0;
	const std::uintptr_t key = reinterpret_cast<std::uintptr_t>( &keyHome );
	auto diff = []( const std::vector<unsigned char> & a, const std::vector<unsigned char> & b ) {
		if ( a.size() != b.size() )
			return std::size_t( -1 );
		std::size_t d = 0;
		for ( std::size_t i = 0; i < a.size(); i++ )
			d += a[i] != b[i];
		return d;
	};
	bool ok = true;
	std::string why;
	std::vector<unsigned char> got;
	for ( int run = 0; run < 2; run++ ) {  // run 1: the same twins again (no regrow), after GL changed one attribute
		if ( run == 1 ) {
			std::vector<float> a( p.verts * 3 );
			for ( float & x : a )
				x = uf( rng );
			f.bindBuffer( kCopyWrite, p.vbo[2] );
			f.bufferData( kCopyWrite, std::ptrdiff_t( a.size() * 4 ), a.data(), kStatic );
			f.bindBuffer( kCopyWrite, 0 );
			std::memcpy( &want[p.verts * ( 3 + 4 ) * 4], a.data(), a.size() * 4 );
		}
		const bool ran = poolCross( key, p, 0, got, why );
		const std::size_t d = ran ? diff( got, want ) : std::size_t( -1 );
		std::size_t bytes = 0;
		const int twins = objBlPoolCount( &bytes );
		std::printf( "BLVIEW pool run%d %s (%zu bytes, %zu differ; %zu verts x %u floats + %zu indices; %d class twins, %.2f MB%s%s)\n", run,
			ran && d == 0 ? "EXACT" : "FAIL", want.size(), d, p.verts, unsigned( 3 + 4 + 3 + 3 + 3 + 2 ), p.indices, twins,
			double( bytes ) / 1048576.0, ran ? "" : "; ", ran ? "" : why.c_str() );
		ok = ok && ran && d == 0 && twins == 1;
	}
	{  // red: each attribute copied from the buffer of the one before it
		const bool ran = poolCross( key, p, 1, got, why );
		const std::size_t d = ran ? diff( got, want ) : std::size_t( -1 );
		const bool fails = !ran || d != 0;
		std::printf( "BLVIEW RED pool %s (each attribute from its neighbour's buffer; %zu bytes differ)\n", fails ? "FAIL" : "EXACT", d );
		ok = ok && fails;
	}
	// the layout: the class's attributes as the twin's vertex input, every other location fo4_default.vert reads on the constant
	const std::vector<std::uint32_t> vs = readSpv( spvDir + "/fo4_defaultbl_vs.pipe.spv" ), ps = readSpv( spvDir + "/fo4_cellbl_ps.pipe.spv" );
	ObjBlTarget tg;
	tg.colors[0] = tg.colors[1] = VK_FORMAT_R16G16B16A16_SFLOAT;
	tg.colorCount = 2;
	tg.depth = VK_FORMAT_D24_UNORM_S8_UINT;
	tg.samples = 4;
	tg.depthOp = VK_COMPARE_OP_EQUAL;
	const std::uint32_t need = 0x3EFFu;  // GL locations 0..7, 9..13
	std::vector<ObjBlBuffer> bufs;
	std::vector<ObjBlAttr> attrs;
	objBlPoolLayout( mask, bufs, attrs, need );
	int onConst = 0;
	for ( const ObjBlAttr & a : attrs )
		onConst += a.binding == kObjBlConstBinding;
	ObjBlPipeInfo in;
	const bool made = !vs.empty() && !ps.empty() && objBlPipe( 0, vs.data(), vs.size(), ps.data(), ps.size(), tg, bufs, attrs, in, why );
	std::printf( "BLVIEW pool layout: %zu buffers, %zu attributes (%d on the constant binding): pipeline %s%s%s\n", bufs.size(), attrs.size(),
		onConst, made ? "made" : "REFUSED", made ? "" : ": ", made ? "" : why.c_str() );
	ok = ok && made && onConst == 7 && attrs.size() == 13;
	// red: the layout without the constant binding (need 0) must be refused for the location it does not feed
	objBlPoolLayout( mask, bufs, attrs, 0 );
	std::string w;
	const bool made2 = objBlPipe( 0, vs.data(), vs.size(), ps.data(), ps.size(), tg, bufs, attrs, in, w );
	const bool own = !made2 && w.find( "no attribute feeds" ) != std::string::npos;
	std::printf( "BLVIEW RED poolconst %s (%s)\n", own ? "FAIL" : "PASSED", made2 ? "made" : w.c_str() );
	ok = ok && own;
	csmBufFree( poolRb.rb, false );  // the GL buffers stay to the context's end (the harness exits)
	if ( const GLenum e = glGetError(); e != GL_NO_ERROR ) {
		std::printf( "BLVIEW pool FAIL (GL error 0x%x left)\n", unsigned( e ) );
		ok = false;
	}
	return ok;
}
