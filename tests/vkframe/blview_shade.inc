/* lane VKOBJ4 slice 7: one class's multi-draw through the fo4_cellbl twins (objBlDrawPrep + objBlDrawRecord) into shared
 * targets (two RGBA16F + D32F, 256 x 128), against GL drawing the same pool, rows and commands (glMultiDrawElementsIndirect,
 * gl_BaseInstance, the same wwDrawBase) with a position-only program. Fixture: 64 shapes (a 4 x 4 vertex grid each, 16
 * verts, 54 u16 indices) in one class pool, the commands in reverse shape order (base vertex, first index and base instance
 * per command), 5 rows before the shapes' (wwDrawBase 5; those rows scale by 40), half the shapes mirrored (back-facing:
 * culled on both sides), a perspective projection in globalUniforms.
 * BAR (stated before the first run, never widened): depth vs GL EXACT, or BOUND with pixels covered on one side only
 * <= 0.2 % of the covered pixels and |depth difference| <= 2e-6 elsewhere; a run with < 2000 covered pixels FAILS. The
 * three record routes (MDI, one indirect per command, direct) EXACT to each other in all three targets.
 * Reds (each must FAIL against GL): base (wwDrawBase 0 in the Vulkan state), inst (every base instance 0), cull (twoSided).
 * Every Vulkan run crosses through objBlRun (GL's signal, the worker's submit, GL's wait), as the app's would.
 * Included inside namespace blv after blview_pool.inc. */

struct ShadeFns
{
	void( WINAPI * vertexAttribPointer )( GLuint, GLint, GLenum, GLboolean, GLsizei, const void * ) = nullptr;
	void( WINAPI * enableVertexAttribArray )( GLuint ) = nullptr;
	void( WINAPI * multiDrawElementsIndirect )( GLenum, GLenum, const void *, GLsizei, GLsizei ) = nullptr;
	GLint( WINAPI * getUniformLocation )( GLuint, const char * ) = nullptr;
	void( WINAPI * uniformMatrix4fv )( GLint, GLsizei, GLboolean, const GLfloat * ) = nullptr;
	void( WINAPI * uniform1ui )( GLint, GLuint ) = nullptr;
	void( WINAPI * drawBuffers )( GLsizei, const GLenum * ) = nullptr;
	void( WINAPI * clearBufferfv )( GLenum, GLint, const GLfloat * ) = nullptr;
};
ShadeFns sf;

struct ShadeFix
{
	WwVkPresent::ObjBlPool pool;
	std::uintptr_t key = 0, c0 = 0, c1 = 0, dk = 0;
	GLuint t0 = 0, t1 = 0, td = 0, fbo = 0, prog = 0, vao = 0, rowsGl = 0, ind = 0;
	std::vector<unsigned char> rows;        // GL form
	std::vector<std::uint32_t> cmds;        // 5 words per command
	std::vector<std::uint32_t> vs, ps;
	std::vector<std::pair<std::uint32_t, std::vector<std::uint8_t>>> ubos;
	std::vector<std::pair<std::size_t, std::uint32_t>> baseAt;  // ubos[i] holds wwDrawBase at this offset
	float proj[16] = {};
	std::uint32_t base = 5;
	int shapes = 64;
};
ShadeFix sx;
constexpr int kSW = 256, kSH = 128;

void shadeClear( float k = 0.0f, bool depth = true )
{
	f.bindFramebuffer( kFb, sx.fbo );
	const GLenum dbs[2] = { kColorAtt0, kColorAtt0 + 1 };
	sf.drawBuffers( 2, dbs );
	const GLfloat zero[4] = { k, k, k, k }, one = 1.0f;
	glDepthMask( GL_TRUE );
	glColorMask( GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE );
	sf.clearBufferfv( 0x1800, 0, zero );  // GL_COLOR
	sf.clearBufferfv( 0x1800, 1, zero );
	if ( depth )
		sf.clearBufferfv( 0x1801, 0, &one );  // GL_DEPTH
	f.bindFramebuffer( kFb, 0 );
}

void shadeRead( std::vector<float> & depth, std::vector<std::uint16_t> * c0, std::vector<std::uint16_t> * c1 )
{
	depth.assign( std::size_t( kSW ) * kSH, 0.0f );
	f.getTextureImage( sx.td, 0, 0x1902, 0x1406, GLsizei( depth.size() * 4 ), depth.data() );  // DEPTH_COMPONENT, FLOAT
	std::vector<std::uint16_t> * cs[2] = { c0, c1 };
	const GLuint ts[2] = { sx.t0, sx.t1 };
	for ( int c = 0; c < 2; c++ )
		if ( cs[c] ) {
			cs[c]->assign( std::size_t( kSW ) * kSH * 4, 0 );
			f.getTextureImage( ts[c], 0, 0x1908, kHalf, GLsizei( cs[c]->size() * 2 ), cs[c]->data() );
		}
}

// one Vulkan draw: the pool copied, the draw prepared, GL's signal, the worker records + submits, GL waits; the targets read
bool shadeVk( int mode, int red, std::vector<float> & depth, std::vector<std::uint16_t> & c0, std::vector<std::uint16_t> & c1,
	WwVkPresent::ObjBlDrawStat & st, std::string & why, const WwVkPresent::ObjBlBlend * blend = nullptr, float clearK = 0.0f )
{
	using namespace WwVkPresent;
	shadeClear( clearK, !blend );  // a blended run: over the depth of the run before it, EQUAL and no depth write (one fragment a pixel)
	std::vector<unsigned> names;
	if ( !objBlPoolCopy( sx.key, sx.pool, names, why ) )
		return false;
	ObjBlDraw d;
	d.twin = 0;
	d.vs = sx.vs.data();
	d.vsWords = sx.vs.size();
	d.ps = sx.ps.data();
	d.psWords = sx.ps.size();
	d.target.colors[0] = d.target.colors[1] = VK_FORMAT_R16G16B16A16_SFLOAT;
	d.target.colorCount = 2;
	d.target.depth = VK_FORMAT_D32_SFLOAT;
	d.target.depthOp = VK_COMPARE_OP_LESS;
	d.target.depthWrite = true;
	d.target.twoSided = red == 3;
	if ( blend ) {
		d.target.blend = *blend;
		d.target.depthOp = VK_COMPARE_OP_EQUAL;
		d.target.depthWrite = false;
	}
	d.colorKeys[0] = sx.c0;
	d.colorKeys[1] = sx.c1;
	d.depthKey = sx.dk;
	d.w = kSW;
	d.h = kSH;
	d.poolKey = sx.key;
	d.attrMask = sx.pool.attrMask;
	d.need = 0x3EFFu;
	d.ubos = sx.ubos;
	if ( red == 1 )  // wwDrawBase 0
		for ( const auto & at : sx.baseAt )
			std::memset( d.ubos[at.first].second.data() + at.second, 0, 4 );
	std::vector<std::uint32_t> cmds = sx.cmds;
	if ( red == 2 )
		for ( std::size_t c = 0; c < cmds.size(); c += 5 )
			cmds[c + 4] = 0;
	d.glRows = sx.rows.data();
	d.rows = sx.rows.size() / kBlRowBytes;
	d.cmds = cmds.data();
	d.cmdCount = cmds.size() / 5;
	objBlDrawsReset();
	if ( !objBlDrawPrep( d, st, why ) )
		return false;
	if ( const GLenum e = glGetError(); e != GL_NO_ERROR )
		return why = "GL error " + std::to_string( e ) + " before the signal", false;
	if ( !objBlRun( names, mode, why ) )
		return false;
	shadeRead( depth, &c0, &c1 );
	glFinish();
	if ( const GLenum e = glGetError(); e != GL_NO_ERROR )
		return why = "GL error " + std::to_string( e ) + " at the semaphores", false;
	return true;
}

// GL's draw of the same pool, rows and commands: depth only
bool shadeGl( std::vector<float> & depth, std::string & why )
{
	shadeClear();
	f.bindFramebuffer( kFb, sx.fbo );
	glViewport( 0, 0, kSW, kSH );
	glEnable( GL_DEPTH_TEST );
	glDepthFunc( GL_LESS );
	glDepthMask( GL_TRUE );
	glColorMask( GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE );
	glEnable( GL_CULL_FACE );
	glCullFace( GL_BACK );
	glFrontFace( GL_CCW );
	f.useProgram( sx.prog );
	sf.uniformMatrix4fv( sf.getUniformLocation( sx.prog, "proj" ), 1, GL_FALSE, sx.proj );
	sf.uniform1ui( sf.getUniformLocation( sx.prog, "base" ), sx.base );
	f.bindVertexArray( sx.vao );
	f.bindBufferBase( kSsbo, 12, sx.rowsGl );
	f.bindBuffer( 0x8F3F, sx.ind );  // DRAW_INDIRECT_BUFFER
	sf.multiDrawElementsIndirect( GL_TRIANGLES, GL_UNSIGNED_SHORT, nullptr, GLsizei( sx.cmds.size() / 5 ), 20 );
	f.bindBuffer( 0x8F3F, 0 );
	f.bindVertexArray( 0 );
	f.useProgram( 0 );
	glDisable( GL_CULL_FACE );
	glDisable( GL_DEPTH_TEST );
	glColorMask( GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE );
	f.bindFramebuffer( kFb, 0 );
	shadeRead( depth, nullptr, nullptr );
	glFinish();
	if ( const GLenum e = glGetError(); e != GL_NO_ERROR )
		return why = "GL error " + std::to_string( e ) + " in GL's draw", false;
	return true;
}

struct DepthCmp
{
	std::size_t covered = 0, oneSide = 0, differ = 0;
	float maxAbs = 0.0f;
	bool exact() const { return covered >= 2000 && oneSide == 0 && differ == 0; }
	bool bound() const { return covered >= 2000 && double( oneSide ) <= 0.002 * double( covered ) && maxAbs <= 2e-6f; }
	const char * word() const { return exact() ? "EXACT" : bound() ? "BOUND" : "FAIL"; }
};

DepthCmp depthCmp( const std::vector<float> & a, const std::vector<float> & b )
{
	DepthCmp c;
	if ( a.size() != b.size() )
		return c;
	for ( std::size_t i = 0; i < a.size(); i++ ) {
		const bool ca = a[i] < 1.0f, cb = b[i] < 1.0f;
		if ( !ca && !cb )
			continue;
		c.covered++;
		if ( ca != cb ) {
			c.oneSide++;
			continue;
		}
		if ( a[i] != b[i] ) {
			c.differ++;
			c.maxAbs = std::max( c.maxAbs, std::fabs( a[i] - b[i] ) );
		}
	}
	return c;
}

// column-major GL mat4 m into a uniform block member at the module's layout (objBlLoose's placement)
void shadePutMat( std::vector<std::uint8_t> & out, const WwVkPresent::SpvMember & m, const float * glm )
{
	for ( std::uint32_t c = 0; c < m.cols; c++ )
		for ( std::uint32_t r = 0; r < m.comps; r++ ) {
			const bool across = m.cols > 1 && m.rowMajor;
			const std::uint32_t at = m.offset + ( across ? r * m.colStride + c * 4 : c * m.colStride + r * 4 );
			if ( at + 4 <= out.size() )
				std::memcpy( out.data() + at, &glm[c * 4 + r], 4 );
		}
}

bool shadeBuild( const std::string & spvDir, std::string & why )
{
	using namespace WwVkPresent;
	if ( !load( sf.vertexAttribPointer, "glVertexAttribPointer" ) || !load( sf.enableVertexAttribArray, "glEnableVertexAttribArray" )
		|| !load( sf.multiDrawElementsIndirect, "glMultiDrawElementsIndirect" ) || !load( sf.getUniformLocation, "glGetUniformLocation" )
		|| !load( sf.uniformMatrix4fv, "glUniformMatrix4fv" ) || !load( sf.uniform1ui, "glUniform1ui" )
		|| !load( sf.drawBuffers, "glDrawBuffers" ) || !load( sf.clearBufferfv, "glClearBufferfv" ) )
		return why = "a GL entry point is missing", false;
	sx.vs = readSpv( spvDir + "/fo4_defaultbl_vs.pipe.spv" );
	sx.ps = readSpv( spvDir + "/fo4_cellbl_ps.pipe.spv" );
	if ( sx.vs.empty() || sx.ps.empty() )
		return why = "no modules", false;
	static int keys[4];
	sx.key = reinterpret_cast<std::uintptr_t>( &keys[0] );
	sx.c0 = reinterpret_cast<std::uintptr_t>( &keys[1] );
	sx.c1 = reinterpret_cast<std::uintptr_t>( &keys[2] );
	sx.dk = reinterpret_cast<std::uintptr_t>( &keys[3] );
	if ( !sharedTarget( sx.c0, 0, kSW, kSH, 1, kRgba16fi, sx.t0, why ) || !sharedTarget( sx.c1, 0, kSW, kSH, 1, kRgba16fi, sx.t1, why )
		|| !sharedTarget( sx.dk, 0, kSW, kSH, 1, 0x8CAC, sx.td, why ) )
		return why = "sharedTarget: " + why, false;
	f.genFramebuffers( 1, &sx.fbo );
	f.bindFramebuffer( kFb, sx.fbo );
	f.framebufferTexture2D( kFb, kColorAtt0, GL_TEXTURE_2D, sx.t0, 0 );
	f.framebufferTexture2D( kFb, kColorAtt0 + 1, GL_TEXTURE_2D, sx.t1, 0 );
	f.framebufferTexture2D( kFb, 0x8D00, GL_TEXTURE_2D, sx.td, 0 );  // DEPTH_ATTACHMENT
	const GLenum st = f.checkFramebufferStatus( kFb );
	f.bindFramebuffer( kFb, 0 );
	if ( st != kComplete )
		return why = "the GL framebuffer is not complete", false;
	// the pool: 64 shapes of 4 x 4 verts over [-1,1]^2 with a bump, position 3, colour 4, normal 3, tangent 3, bitangent 3, uv 2
	const int nv = 16, ni = 54;
	sx.pool.attrMask = 3ull | 4ull << 4 | 3ull << 8 | 3ull << 12 | 3ull << 16 | 2ull << 28;
	sx.pool.verts = std::size_t( sx.shapes * nv );
	sx.pool.indices = std::size_t( sx.shapes * ni );
	std::vector<float> a[8];
	std::vector<std::uint16_t> idx;
	for ( int s = 0; s < sx.shapes; s++ ) {
		for ( int y = 0; y < 4; y++ )
			for ( int x = 0; x < 4; x++ ) {
				const float px = -1.0f + x * ( 2.0f / 3.0f ), py = -1.0f + y * ( 2.0f / 3.0f );
				a[0].insert( a[0].end(), { px, py, 0.25f * std::sin( px * 2.1f + py * 1.3f + float( s ) ) } );
				a[1].insert( a[1].end(), { 1.0f, 1.0f, 1.0f, 1.0f } );
				a[2].insert( a[2].end(), { 0.0f, 0.0f, 1.0f } );
				a[3].insert( a[3].end(), { 1.0f, 0.0f, 0.0f } );
				a[4].insert( a[4].end(), { 0.0f, 1.0f, 0.0f } );
				a[7].insert( a[7].end(), { px * 0.5f + 0.5f, py * 0.5f + 0.5f } );
			}
		for ( int y = 0; y < 3; y++ )
			for ( int x = 0; x < 3; x++ ) {
				const std::uint16_t i0 = std::uint16_t( y * 4 + x ), i1 = std::uint16_t( i0 + 1 ), i2 = std::uint16_t( i0 + 4 ), i3 = std::uint16_t( i0 + 5 );
				idx.insert( idx.end(), { i0, i1, i3, i0, i3, i2 } );  // CCW seen from +z
			}
	}
	constexpr GLenum kCopyWrite = 0x8F37, kStatic = 0x88E4;
	for ( int i : { 0, 1, 2, 3, 4, 7 } ) {
		f.genBuffers( 1, &sx.pool.vbo[i] );
		f.bindBuffer( kCopyWrite, sx.pool.vbo[i] );
		f.bufferData( kCopyWrite, std::ptrdiff_t( a[i].size() * 4 ), a[i].data(), kStatic );
	}
	f.genBuffers( 1, &sx.pool.ebo );
	f.bindBuffer( kCopyWrite, sx.pool.ebo );
	f.bufferData( kCopyWrite, std::ptrdiff_t( idx.size() * 2 ), idx.data(), kStatic );
	f.bindBuffer( kCopyWrite, 0 );
	// the rows: 5 decoys (scale 40), then one per shape on an 8 x 8 grid at depth 12..30, turned about y, odd ones mirrored
	const int rowsN = int( sx.base ) + sx.shapes;
	sx.rows.assign( std::size_t( rowsN ) * kBlRowBytes, 0 );
	for ( int r = 0; r < rowsN; r++ ) {
		float m[16] = {};
		const int s = r - int( sx.base );
		if ( s < 0 ) {
			m[0] = m[5] = m[10] = 40.0f;
			m[15] = 1.0f;
			m[14] = -20.0f;
		} else {
			const float ang = 0.35f * float( s % 7 ) - 1.0f, sc = 1.1f, mir = ( s & 1 ) ? -1.0f : 1.0f;
			const float tx = ( float( s % 8 ) - 3.5f ) * 3.4f, ty = ( float( s / 8 ) - 3.5f ) * 1.7f, tz = -12.0f - float( ( s * 5 ) % 19 );
			m[0] = sc * std::cos( ang ) * mir;
			m[2] = -sc * std::sin( ang ) * mir;
			m[5] = sc;
			m[8] = sc * std::sin( ang );
			m[10] = sc * std::cos( ang );
			m[12] = tx * ( -tz / 12.0f );
			m[13] = ty * ( -tz / 12.0f );
			m[14] = tz;
			m[15] = 1.0f;
		}
		unsigned char * row = sx.rows.data() + std::size_t( r ) * kBlRowBytes;
		std::memcpy( row, m, 64 );
		const float nm[12] = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0 };
		std::memcpy( row + 64, nm, 48 );
	}
	// the commands, last shape first
	for ( int c = 0; c < sx.shapes; c++ ) {
		const int s = sx.shapes - 1 - c;
		sx.cmds.insert( sx.cmds.end(), { std::uint32_t( ni ), 1u, std::uint32_t( s * ni ), std::uint32_t( s * nv ), std::uint32_t( s ) } );
	}
	// a perspective projection (60 degrees, 2:1, near 1, far 100), GL column-major
	const float fy = 1.0f / std::tan( 0.5235988f ), n = 1.0f, fa = 100.0f;
	sx.proj[0] = fy / 2.0f;
	sx.proj[5] = fy;
	sx.proj[10] = ( fa + n ) / ( n - fa );
	sx.proj[11] = -1.0f;
	sx.proj[14] = 2.0f * fa * n / ( n - fa );
	// the state: every uniform block zero but globalUniforms.projectionMatrix and LooseUniforms.wwDrawBase
	const std::vector<std::uint32_t> * mods[2] = { &sx.vs, &sx.ps };
	for ( const std::vector<std::uint32_t> * w : mods ) {
		SpvModule m;
		if ( !spvReflect( w->data(), w->size(), m, why ) )
			return false;
		for ( const SpvBind & b : m.binds ) {
			if ( b.type != VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER || b.set != 0 )
				continue;
			std::vector<std::uint8_t> bytes( b.bytes, 0 );
			for ( const SpvMember & mm : b.members ) {
				if ( b.name.find( "globalUniforms" ) != std::string::npos && mm.name == "projectionMatrix" )
					shadePutMat( bytes, mm, sx.proj );
				if ( b.name.find( "LooseUniforms" ) != std::string::npos && mm.name == "wwDrawBase" ) {
					std::memcpy( bytes.data() + mm.offset, &sx.base, 4 );
					sx.baseAt.emplace_back( sx.ubos.size(), mm.offset );
				}
			}
			sx.ubos.emplace_back( b.binding, std::move( bytes ) );
		}
	}
	if ( sx.baseAt.empty() )
		return why = "wwDrawBase is in no LooseUniforms", false;
	// GL's reference: the positions, the rows as GL binds them (binding 12), the commands as its indirect buffer
	const std::string glVs = "#version 460 core\nlayout(location = 0) in vec3 pos;\nstruct Row { mat4 mv; vec4 nm[3]; uvec2 tex[8]; };\n"
		"layout(std430, binding = 12) readonly buffer Rows { Row rows[]; };\nuniform mat4 proj;\nuniform uint base;\n"
		"void main() { gl_Position = proj * (rows[base + uint(gl_BaseInstance)].mv * vec4(pos, 1.0)); }\n";
	const std::string glFs = "#version 460 core\nvoid main() {}\n";
	sx.prog = program( glVs, glFs );
	if ( !sx.prog )
		return why = "GL's reference program", false;
	f.genVertexArrays( 1, &sx.vao );
	f.bindVertexArray( sx.vao );
	f.bindBuffer( 0x8892, sx.pool.vbo[0] );  // ARRAY_BUFFER
	sf.vertexAttribPointer( 0, 3, GL_FLOAT, GL_FALSE, 0, nullptr );
	sf.enableVertexAttribArray( 0 );
	f.bindBuffer( 0x8893, sx.pool.ebo );  // ELEMENT_ARRAY_BUFFER (the VAO's)
	f.bindVertexArray( 0 );
	f.bindBuffer( 0x8892, 0 );
	f.genBuffers( 1, &sx.rowsGl );
	f.bindBuffer( kSsbo, sx.rowsGl );
	f.bufferData( kSsbo, std::ptrdiff_t( sx.rows.size() ), sx.rows.data(), kStatic );
	f.bindBuffer( kSsbo, 0 );
	f.genBuffers( 1, &sx.ind );
	f.bindBuffer( 0x8F3F, sx.ind );
	f.bufferData( 0x8F3F, std::ptrdiff_t( sx.cmds.size() * 4 ), sx.cmds.data(), kStatic );
	f.bindBuffer( 0x8F3F, 0 );
	if ( const GLenum e = glGetError(); e != GL_NO_ERROR )
		return why = "GL error " + std::to_string( e ) + " making the fixture", false;
	return true;
}

/* lane VKOBJ4 step 6: GL's texture units as set 0 (objBlBindUnits) for the CSM twin. A GL program binds csmMap (a shared
 * 2D array, DEPTH_COMPONENT16, compare LEQUAL, as sunshadow makes it), volTex (a shared 3D, as the air volume) and
 * ussContactMap (GL's own 2D) on their units. BAR: 2 shared images on the twin's csmMap / volTex bindings, 1 GL-only,
 * 0 misfits, the csmMap sampler GL's compare state; prep writes both images and GL's samplers; the run crosses.
 * Reds (each must FAIL): swap (the two shared textures on each other's unit), fit (the CSM given to volTex's binding
 * straight to prep), cmp (the CSM's compare mode off in GL). */
/* Open item 5 (a cube-array shared type): cellShadow is a shared type 3 (32 x 32, 12 layers = 2 cubes, DEPTH_COMPONENT16, compare
 * LEQUAL, as celllamps makes it). BAR: it imports as GL_TEXTURE_CUBE_MAP_ARRAY (GL clears layer 7 to 0.5 and reads 0.5 back), a
 * type 3 of 8 layers is refused, objBlBindUnits places it on cellShadow's binding with GL's compare sampler (3 shared images,
 * 2 compare samplers), prep writes 3 images. Red cubefit (must FAIL): the cube given to csmMap's binding straight to prep. */
struct UnitsFix
{
	GLuint csm = 0, vol = 0, own = 0, cube = 0, prog = 0;
	std::uintptr_t csmKey = 0, volKey = 0, cubeKey = 0, cubeBadKey = 0;
	std::vector<std::uint32_t> ps;
	std::vector<std::pair<std::uint32_t, std::vector<std::uint8_t>>> ubos;
	std::uint32_t csmB = ~0u, volB = ~0u, csmS = ~0u, cubeB = ~0u, cubeS = ~0u;
	bool cubeRead = false, cubeBadRefused = false;
	void( WINAPI * bindTextureUnit )( GLuint, GLuint ) = nullptr;
	void( WINAPI * textureParameteri )( GLuint, GLenum, GLint ) = nullptr;
	void( WINAPI * createTextures )( GLenum, GLsizei, GLuint * ) = nullptr;
	void( WINAPI * textureStorage2D )( GLuint, GLsizei, GLenum, GLsizei, GLsizei ) = nullptr;
};
UnitsFix ux;

bool unitsBuild( const std::string & spvDir, std::string & why )
{
	using namespace WwVkPresent;
	if ( !load( ux.bindTextureUnit, "glBindTextureUnit" ) || !load( ux.textureParameteri, "glTextureParameteri" )
		|| !load( ux.createTextures, "glCreateTextures" ) || !load( ux.textureStorage2D, "glTextureStorage2D" ) )
		return why = "a GL entry point is missing", false;
	ux.ps = readSpv( spvDir + "/fo4_cellcsmbl_ps.pipe.spv" );
	if ( ux.ps.empty() )
		return why = "no csm pixel module", false;
	static int keys[4];
	ux.csmKey = reinterpret_cast<std::uintptr_t>( &keys[0] );
	ux.volKey = reinterpret_cast<std::uintptr_t>( &keys[1] );
	ux.cubeKey = reinterpret_cast<std::uintptr_t>( &keys[2] );
	ux.cubeBadKey = reinterpret_cast<std::uintptr_t>( &keys[3] );
	if ( !sharedTarget( ux.csmKey, 2, 64, 64, 4, 0x81A5, ux.csm, why ) || !sharedTarget( ux.volKey, 1, 16, 16, 16, kRgba16fi, ux.vol, why )
		|| !sharedTarget( ux.cubeKey, 3, 32, 32, 12, 0x81A5, ux.cube, why ) )
		return why = "sharedTarget: " + why, false;
	{
		GLuint bad = 0;
		std::string w;
		ux.cubeBadRefused = !sharedTarget( ux.cubeBadKey, 3, 32, 32, 8, 0x81A5, bad, w ) && !bad;
		while ( glGetError() != GL_NO_ERROR ) {}
	}
	{
		// GL's view of the import: a cube array (its binding query), layer 7 cleared to 0.5 and read back
		void( WINAPI * clearSub )( GLuint, GLint, GLint, GLint, GLint, GLsizei, GLsizei, GLsizei, GLenum, GLenum, const void * ) = nullptr;
		void( WINAPI * getSub )( GLuint, GLint, GLint, GLint, GLint, GLsizei, GLsizei, GLsizei, GLenum, GLenum, GLsizei, void * ) = nullptr;
		if ( !load( clearSub, "glClearTexSubImage" ) || !load( getSub, "glGetTextureSubImage" ) )
			return why = "no glClearTexSubImage / glGetTextureSubImage", false;
		GLint was = 0, now = 0;
		glGetIntegerv( 0x900A, &was );  // TEXTURE_BINDING_CUBE_MAP_ARRAY
		glBindTexture( 0x9009, ux.cube );
		glGetIntegerv( 0x900A, &now );
		const bool isCube = glGetError() == GL_NO_ERROR && GLuint( now ) == ux.cube;
		glBindTexture( 0x9009, GLuint( was ) );
		const float half = 0.5f;
		clearSub( ux.cube, 0, 0, 0, 7, 32, 32, 1, 0x1902, GL_FLOAT, &half );  // DEPTH_COMPONENT
		std::vector<float> back( 32 * 32, 0.0f );
		getSub( ux.cube, 0, 0, 0, 7, 32, 32, 1, 0x1902, GL_FLOAT, GLsizei( back.size() * 4 ), back.data() );
		std::size_t at = 0;
		for ( float x : back )
			at += std::fabs( x - 0.5f ) <= 1.0f / 65535.0f;
		ux.cubeRead = isCube && at == back.size() && glGetError() == GL_NO_ERROR;
	}
	for ( GLuint t : { ux.csm, ux.vol, ux.cube } ) {
		ux.textureParameteri( t, 0x2801, 0x2601 );  // LINEAR
		ux.textureParameteri( t, 0x2800, 0x2601 );
		for ( GLenum wr : { 0x2802u, 0x2803u, 0x8072u } )
			ux.textureParameteri( t, wr, 0x812F );  // CLAMP_TO_EDGE
	}
	for ( GLuint t : { ux.csm, ux.cube } ) {
		ux.textureParameteri( t, 0x884C, 0x884E );  // COMPARE_REF_TO_TEXTURE
		ux.textureParameteri( t, 0x884D, 0x0203 );  // LEQUAL
	}
	ux.createTextures( GL_TEXTURE_2D, 1, &ux.own );
	ux.textureStorage2D( ux.own, 1, 0x8058, 8, 8 );
	const std::string vs = "#version 460 core\nvoid main() { gl_Position = vec4( 0.0 ); }\n";
	const std::string fs = "#version 460 core\nlayout ( binding = 3 ) uniform sampler2DArrayShadow csmMap;\n"
		"layout ( binding = 4 ) uniform sampler3D volTex;\nlayout ( binding = 5 ) uniform sampler2D ussContactMap;\n"
		"layout ( binding = 6 ) uniform samplerCubeArrayShadow cellShadow;\n"
		"layout ( location = 0 ) out vec4 o;\nvoid main() { o = vec4( texture( csmMap, vec4( 0.5, 0.5, 0.0, 0.5 ) ) + texture( volTex, vec3( 0.5 ) ).x"
		" + texture( ussContactMap, vec2( 0.5 ) ).x + texture( cellShadow, vec4( 1.0, 0.0, 0.0, 1.0 ), 0.5 ) ); }\n";
	ux.prog = program( vs, fs );
	if ( !ux.prog )
		return why = "the units program did not link", false;
	const std::vector<std::uint32_t> * mods[2] = { &sx.vs, &ux.ps };
	for ( const std::vector<std::uint32_t> * w : mods ) {
		SpvModule m;
		if ( !spvReflect( w->data(), w->size(), m, why ) )
			return false;
		for ( const SpvBind & b : m.binds ) {
			if ( b.set != 0 )
				continue;
			if ( b.type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER ) {
				ux.ubos.emplace_back( b.binding, std::vector<std::uint8_t>( b.bytes, 0 ) );
				continue;
			}
			if ( w != &ux.ps )
				continue;
			if ( b.name == "csmMap" )
				ux.csmB = b.binding;
			else if ( b.name == "volTex" )
				ux.volB = b.binding;
			else if ( b.name == "_csmMap_sampler" )
				ux.csmS = b.binding;
			else if ( b.name == "cellShadow" )
				ux.cubeB = b.binding;
			else if ( b.name == "_cellShadow_sampler" )
				ux.cubeS = b.binding;
		}
	}
	if ( ux.csmB == ~0u || ux.volB == ~0u || ux.csmS == ~0u || ux.cubeB == ~0u || ux.cubeS == ~0u )
		return why = "the csm twin has no csmMap / volTex / _csmMap_sampler / cellShadow / _cellShadow_sampler", false;
	if ( const GLenum e = glGetError(); e != GL_NO_ERROR )
		return why = "GL error " + std::to_string( e ) + " making the units fixture", false;
	return true;
}

struct UnitsOut
{
	WwVkPresent::ObjBlBindStat bind;
	WwVkPresent::ObjBlDrawStat prep;
	bool placed = false, cmp = false, cubeCmp = false, prepped = false, ran = false;
	std::string why;
	bool good() const
	{
		return bind.shared == 3 && bind.compare == 2 && bind.glOwn == 1 && bind.misfit == 0 && placed && cmp && cubeCmp && prepped && ran
			&& prep.images == 3 && prep.glSamplers >= 4 && ux.cubeRead && ux.cubeBadRefused;
	}
};

void unitsRun( int red, UnitsOut & o )
{
	using namespace WwVkPresent;
	ux.bindTextureUnit( 3, red == 1 ? ux.vol : ux.csm );
	ux.bindTextureUnit( 4, red == 1 ? ux.csm : ux.vol );
	ux.bindTextureUnit( 5, ux.own );
	ux.bindTextureUnit( 6, ux.cube );  // a cube array on unit 6
	if ( red == 3 )
		ux.textureParameteri( ux.csm, 0x884C, 0 );
	f.useProgram( ux.prog );
	ObjBlDraw d;
	d.twin = 1;
	d.vs = sx.vs.data();
	d.vsWords = sx.vs.size();
	d.ps = ux.ps.data();
	d.psWords = ux.ps.size();
	const bool bound = objBlBindUnits( glProc, ux.prog, d, o.bind, o.why );
	f.useProgram( 0 );
	for ( GLuint u : { 3u, 4u, 5u, 6u } )
		ux.bindTextureUnit( u, 0 );
	if ( red == 3 )
		ux.textureParameteri( ux.csm, 0x884C, 0x884E );
	if ( !bound )
		return;
	if ( red == 2 )
		d.images.emplace_back( ux.volB, ux.csmKey );
	if ( red == 4 )
		d.images.emplace_back( ux.csmB, ux.cubeKey );
	int csmAt = 0, volAt = 0, cubeAt = 0;
	for ( const auto & im : d.images ) {
		csmAt += im.first == ux.csmB && im.second == ux.csmKey;
		volAt += im.first == ux.volB && im.second == ux.volKey;
		cubeAt += im.first == ux.cubeB && im.second == ux.cubeKey;
	}
	o.placed = csmAt == 1 && volAt == 1 && cubeAt == 1 && d.images.size() == 3;
	for ( const auto & s : d.samplers ) {
		const bool c = s.second.compareMode == 0x884E && s.second.compareFunc == 0x0203 && s.second.minFilter == 0x2601;
		if ( s.first == ux.csmS )
			o.cmp = c;
		if ( s.first == ux.cubeS )
			o.cubeCmp = c;
	}
	shadeClear();
	std::vector<unsigned> names;
	if ( !objBlPoolCopy( sx.key, sx.pool, names, o.why ) )
		return;
	d.target.colors[0] = d.target.colors[1] = VK_FORMAT_R16G16B16A16_SFLOAT;
	d.target.colorCount = 2;
	d.target.depth = VK_FORMAT_D32_SFLOAT;
	d.target.depthOp = VK_COMPARE_OP_LESS;
	d.target.depthWrite = true;
	d.colorKeys[0] = sx.c0;
	d.colorKeys[1] = sx.c1;
	d.depthKey = sx.dk;
	d.w = kSW;
	d.h = kSH;
	d.poolKey = sx.key;
	d.attrMask = sx.pool.attrMask;
	d.need = 0x3EFFu;
	d.ubos = ux.ubos;
	d.glRows = sx.rows.data();
	d.rows = sx.rows.size() / kBlRowBytes;
	d.cmds = sx.cmds.data();
	d.cmdCount = sx.cmds.size() / 5;
	objBlDrawsReset();
	o.prepped = objBlDrawPrep( d, o.prep, o.why );
	if ( !o.prepped )
		return;
	o.ran = objBlRun( names, 0, o.why );
	glFinish();
}

bool unitsGate( const std::string & spvDir )
{
	std::string why;
	if ( !unitsBuild( spvDir, why ) ) {
		std::printf( "BLVIEW units FAIL (the fixture: %s)\n", why.c_str() );
		return false;
	}
	UnitsOut o;
	unitsRun( 0, o );
	bool ok = o.good();
	std::printf( "BLVIEW units csmbl %s (%d shared images, %d compare samplers, %d GL samplers mirrored, %d GL-only, %d misfit, %d unbound, "
		"%d GL texel buffers; placed %d, csm compare %d; set 0: %d images given, %d GL samplers, %d stand-ins; crossing %s)%s%s\n",
		ok ? "OK" : "FAIL", o.bind.shared, o.bind.compare, o.bind.samplers, o.bind.glOwn, o.bind.misfit, o.bind.unbound, o.bind.texelGl,
		int( o.placed ), int( o.cmp ), o.prep.images, o.prep.glSamplers, o.prep.standIns, o.ran ? "OK" : "no", o.why.empty() ? "" : ": ",
		o.why.c_str() );
	std::printf( "BLVIEW units cube %s (cellShadow a shared cube array: GL reads layer 7 back %d, 8 layers refused %d, placed on its binding %d, "
		"compare %d)\n", ux.cubeRead && ux.cubeBadRefused && o.placed && o.cubeCmp ? "OK" : "FAIL", int( ux.cubeRead ), int( ux.cubeBadRefused ),
		int( o.placed ), int( o.cubeCmp ) );
	const char * reds[4] = { "swap", "fit", "cmp", "cubefit" };
	const char * redWhy[4] = { "the shared textures on each other's unit", "the CSM given to volTex's binding", "the CSM's compare mode off",
		"the cube given to csmMap's binding" };
	for ( int r = 1; r <= 4; r++ ) {
		UnitsOut x;
		unitsRun( r, x );
		const bool fails = !x.good();
		std::printf( "BLVIEW RED units%s %s (%s; %d shared, %d misfit, %d compare, placed %d, prep %d%s%s)\n", reds[r - 1], fails ? "FAIL" : "OK",
			redWhy[r - 1], x.bind.shared, x.bind.misfit, x.bind.compare, int( x.placed ), int( x.prepped ), x.why.empty() ? "" : ": ", x.why.c_str() );
		ok = ok && fails;
	}
	if ( const GLenum e = glGetError(); e != GL_NO_ERROR ) {
		std::printf( "BLVIEW units FAIL (GL error 0x%x left)\n", unsigned( e ) );
		ok = false;
	}
	return ok;
}

/* lane VKOBJ4 open item 4: GL's light texel buffers shared. A GL program binds cellLights (a samplerBuffer, RGBA32F over bytes
 * 256..767 of a 1 KiB buffer by glTextureBufferRange) and cellCluster (an isamplerBuffer, RGBA32I over a whole 512-byte buffer by
 * glTextureBuffer) on units 7 and 8, both buffers known bytes. BAR: objBlBindUnits copies both (2 GL texel buffers, 2 shared,
 * 0 refused, each on the CSM twin's own binding in d.texels, 2 twin names); each twin read back through GL holds GL's range with
 * 0 bytes differing; an RGB32F buffer texture is refused by objBlTexelCopy; prep writes both from their twins (2 from GL) and the
 * crossing runs. Reds (each must FAIL): shift (the copy from 16 bytes past the range's start), class (cellLights' float twin given
 * to cellCluster's int binding straight to prep). */
struct TexelFix
{
	GLuint lightBuf = 0, clusterBuf = 0, rgbBuf = 0, lightTex = 0, clusterTex = 0, rgbTex = 0, prog = 0;
	std::vector<std::uint8_t> light, cluster;  // GL's ranges' bytes
	std::uint32_t lightB = ~0u, clusterB = ~0u;
	bool rgbRefused = false;
	std::string rgbWhy;
};
TexelFix tx;

bool texelBuild( std::string & why )
{
	using namespace WwVkPresent;
	void( WINAPI * createBuffers )( GLsizei, GLuint * ) = nullptr;
	void( WINAPI * namedBufferData )( GLuint, std::intptr_t, const void *, GLenum ) = nullptr;
	void( WINAPI * textureBuffer )( GLuint, GLenum, GLuint ) = nullptr;
	void( WINAPI * textureBufferRange )( GLuint, GLenum, GLuint, std::intptr_t, std::intptr_t ) = nullptr;
	if ( !ux.prog || !ux.createTextures )
		return why = "no units fixture", false;
	if ( !load( createBuffers, "glCreateBuffers" ) || !load( namedBufferData, "glNamedBufferData" ) || !load( textureBuffer, "glTextureBuffer" )
		|| !load( textureBufferRange, "glTextureBufferRange" ) )
		return why = "a GL entry point is missing", false;
	std::vector<std::uint8_t> lightAll( 1024 ), clusterAll( 512 );
	std::uint32_t seed = 0x9E3779B9u;
	for ( std::uint8_t & b : lightAll )
		b = std::uint8_t( ( seed = seed * 1664525u + 1013904223u ) >> 24 );
	for ( std::uint8_t & b : clusterAll )
		b = std::uint8_t( ( seed = seed * 1664525u + 1013904223u ) >> 24 );
	tx.light.assign( lightAll.begin() + 256, lightAll.begin() + 768 );
	tx.cluster = clusterAll;
	createBuffers( 1, &tx.lightBuf );
	createBuffers( 1, &tx.clusterBuf );
	createBuffers( 1, &tx.rgbBuf );
	namedBufferData( tx.lightBuf, 1024, lightAll.data(), 0x88E4 );  // STATIC_DRAW
	namedBufferData( tx.clusterBuf, 512, clusterAll.data(), 0x88E4 );
	namedBufferData( tx.rgbBuf, 384, lightAll.data(), 0x88E4 );
	ux.createTextures( 0x8C2A, 1, &tx.lightTex );  // TEXTURE_BUFFER
	ux.createTextures( 0x8C2A, 1, &tx.clusterTex );
	ux.createTextures( 0x8C2A, 1, &tx.rgbTex );
	textureBufferRange( tx.lightTex, 0x8814, tx.lightBuf, 256, 512 );  // RGBA32F
	textureBuffer( tx.clusterTex, 0x8D82, tx.clusterBuf );              // RGBA32I
	textureBuffer( tx.rgbTex, 0x8815, tx.rgbBuf );                      // RGB32F
	{
		std::vector<unsigned> names;
		tx.rgbRefused = !objBlTexelCopy( glProc, tx.rgbTex, names, tx.rgbWhy ) && names.empty();
	}
	const std::string vs = "#version 460 core\nvoid main() { gl_Position = vec4( 0.0 ); }\n";
	const std::string fs = "#version 460 core\nlayout ( binding = 7 ) uniform samplerBuffer cellLights;\n"
		"layout ( binding = 8 ) uniform isamplerBuffer cellCluster;\nlayout ( location = 0 ) out vec4 o;\n"
		"void main() { o = texelFetch( cellLights, 3 ) + vec4( texelFetch( cellCluster, 5 ) ); }\n";
	tx.prog = program( vs, fs );
	if ( !tx.prog )
		return why = "the texel program did not link", false;
	SpvModule m;
	if ( !spvReflect( ux.ps.data(), ux.ps.size(), m, why ) )
		return false;
	for ( const SpvBind & b : m.binds )
		if ( b.set == 0 && b.type == VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER ) {
			if ( b.name == "cellLights" )
				tx.lightB = b.binding;
			else if ( b.name == "cellCluster" )
				tx.clusterB = b.binding;
		}
	if ( tx.lightB == ~0u || tx.clusterB == ~0u )
		return why = "the csm twin has no cellLights / cellCluster texel buffer", false;
	if ( const GLenum e = glGetError(); e != GL_NO_ERROR )
		return why = "GL error " + std::to_string( e ) + " making the texel fixture", false;
	return true;
}

struct TexelOut
{
	WwVkPresent::ObjBlBindStat bind;
	WwVkPresent::ObjBlDrawStat prep;
	bool placed = false, prepped = false, ran = false;
	std::size_t differ = ~std::size_t( 0 );
	std::string why;
	bool good() const
	{
		return bind.texelGl == 2 && bind.texelShared == 2 && bind.texelRefused == 0 && bind.glBufs.size() == 2 && placed && differ == 0
			&& tx.rgbRefused && prepped && prep.texelsGl == 2 && ran;
	}
};

void texelRun( int red, TexelOut & o )
{
	using namespace WwVkPresent;
	void( WINAPI * getSub )( GLuint, std::intptr_t, std::intptr_t, void * ) = nullptr;
	if ( !load( getSub, "glGetNamedBufferSubData" ) ) {
		o.why = "no glGetNamedBufferSubData";
		return;
	}
	ux.bindTextureUnit( 7, tx.lightTex );
	ux.bindTextureUnit( 8, tx.clusterTex );
	f.useProgram( tx.prog );
	ObjBlDraw d;
	d.twin = 1;
	d.vs = sx.vs.data();
	d.vsWords = sx.vs.size();
	d.ps = ux.ps.data();
	d.psWords = ux.ps.size();
	const bool bound = objBlBindUnits( glProc, tx.prog, d, o.bind, o.why );
	f.useProgram( 0 );
	ux.bindTextureUnit( 7, 0 );
	ux.bindTextureUnit( 8, 0 );
	if ( !bound )
		return;
	int lightAt = 0, clusterAt = 0;
	for ( const auto & t : d.texels ) {
		lightAt += t.first == tx.lightB && t.second == tx.lightTex;
		clusterAt += t.first == tx.clusterB && t.second == tx.clusterTex;
	}
	o.placed = lightAt == 1 && clusterAt == 1 && d.texels.size() == 2;
	if ( red == 1 ) {
		std::vector<unsigned> extra;
		if ( !objBlTexelCopy( glProc, tx.lightTex, extra, o.why, 1 ) )
			return;
	}
	glFinish();
	// each twin read back through GL against GL's own range (d.texels and glBufs pair up in order)
	if ( d.texels.size() == o.bind.glBufs.size() ) {
		o.differ = 0;
		for ( std::size_t i = 0; i < d.texels.size(); i++ ) {
			const std::vector<std::uint8_t> & want = d.texels[i].second == tx.lightTex ? tx.light : tx.cluster;
			std::vector<std::uint8_t> got( want.size(), 0 );
			getSub( o.bind.glBufs[i], 0, std::intptr_t( got.size() ), got.data() );
			for ( std::size_t k = 0; k < want.size(); k++ )
				o.differ += got[k] != want[k];
		}
	}
	if ( red == 2 )
		for ( auto & t : d.texels )
			if ( t.first == tx.clusterB )
				t.second = tx.lightTex;
	shadeClear();
	std::vector<unsigned> names;
	if ( !objBlPoolCopy( sx.key, sx.pool, names, o.why ) )
		return;
	names.insert( names.end(), o.bind.glBufs.begin(), o.bind.glBufs.end() );
	d.target.colors[0] = d.target.colors[1] = VK_FORMAT_R16G16B16A16_SFLOAT;
	d.target.colorCount = 2;
	d.target.depth = VK_FORMAT_D32_SFLOAT;
	d.target.depthOp = VK_COMPARE_OP_LESS;
	d.target.depthWrite = true;
	d.colorKeys[0] = sx.c0;
	d.colorKeys[1] = sx.c1;
	d.depthKey = sx.dk;
	d.w = kSW;
	d.h = kSH;
	d.poolKey = sx.key;
	d.attrMask = sx.pool.attrMask;
	d.need = 0x3EFFu;
	d.ubos = ux.ubos;
	d.glRows = sx.rows.data();
	d.rows = sx.rows.size() / kBlRowBytes;
	d.cmds = sx.cmds.data();
	d.cmdCount = sx.cmds.size() / 5;
	objBlDrawsReset();
	o.prepped = objBlDrawPrep( d, o.prep, o.why );
	if ( !o.prepped )
		return;
	o.ran = objBlRun( names, 0, o.why );
	glFinish();
}

bool texelGate()
{
	std::string why;
	if ( !texelBuild( why ) ) {
		std::printf( "BLVIEW texel FAIL (the fixture: %s)\n", why.c_str() );
		return false;
	}
	TexelOut o;
	texelRun( 0, o );
	bool ok = o.good();
	std::printf( "BLVIEW texel lights %s (%d GL texel buffers, %d shared, %d refused, %zu twins; placed %d; %zu bytes differ read back; "
		"RGB32F refused %d (%s); set 0: %d from GL of %d texel buffers; crossing %s)%s%s%s%s\n", ok ? "OK" : "FAIL", o.bind.texelGl,
		o.bind.texelShared, o.bind.texelRefused, o.bind.glBufs.size(), int( o.placed ), o.differ, int( tx.rgbRefused ), tx.rgbWhy.c_str(),
		o.prep.texelsGl, o.prep.texels, o.ran ? "OK" : "no", o.bind.texelWhy.empty() ? "" : "; ", o.bind.texelWhy.c_str(),
		o.why.empty() ? "" : ": ", o.why.c_str() );
	const char * reds[2] = { "shift", "class" };
	const char * redWhy[2] = { "the copy from 16 bytes past the range's start", "cellLights' float twin on cellCluster's int binding" };
	for ( int r = 1; r <= 2; r++ ) {
		TexelOut x;
		texelRun( r, x );
		const bool fails = !x.good();
		std::printf( "BLVIEW RED texel%s %s (%s; %zu bytes differ, placed %d, prep %d%s%s)\n", reds[r - 1], fails ? "FAIL" : "OK", redWhy[r - 1],
			x.differ, int( x.placed ), int( x.prepped ), x.why.empty() ? "" : ": ", x.why.c_str() );
		ok = ok && fails;
	}
	if ( const GLenum e = glGetError(); e != GL_NO_ERROR ) {
		std::printf( "BLVIEW texel FAIL (GL error 0x%x left)\n", unsigned( e ) );
		ok = false;
	}
	return ok;
}

/* lane VKOBJ4 gap 2: GL's light buffers as stores (objBlTexelStore: exported memory of the present device that GL writes;
 * the crossing reads it in place, nothing copied). BAR (stated before the first run, never widened): a 4 KiB RGBA32F light
 * buffer filled through objBlTexelFill is a store; its buffer texture is read in place (objBlTexelImported; 0 bytes copied;
 * the crossing's GL names are the store's alone) and Vulkan's bytes of the view (objBlTexelRead: the view's buffer, offset
 * and range copied back after a crossing) equal the fill EXACT; frame 2: GL's patch (512 bytes at 512, the flicker's write)
 * read back EXACT; frame 3: a 96 KiB fill grows the store (the name moves, the texture re-attached) read back EXACT; the int
 * class (a 2 KiB RGBA32I cluster list) EXACT. Red (must FAIL): skip (frame 2's write skipped: Vulkan reads frame 1's bytes).
 * Cost, measured offline (256 KiB, 200 crossings): objBlTexelCopy of a GL buffer (the copy each crossing) against a store,
 * CPU ms and GPU ms (GL timer) per crossing, printed on the `BLVIEW store cost` line. */
struct StoreOut
{
	std::size_t d1 = ~std::size_t( 0 ), d2 = ~std::size_t( 0 ), d3 = ~std::size_t( 0 ), di = ~std::size_t( 0 );
	bool stored = false, imported = true, namesOk = true, moved = false;
	std::uint64_t copiedBytes = 0;
	int reads = 0, subData = -1;
	std::string why;
	bool good() const
	{
		return stored && imported && namesOk && moved && copiedBytes == 0 && reads == 4 && d1 == 0 && d2 == 0 && d3 == 0 && di == 0;
	}
};

void storeRun( int red, StoreOut & o )
{
	using namespace WwVkPresent;
	void( WINAPI * textureBuffer )( GLuint, GLenum, GLuint ) = nullptr;
	if ( !ux.createTextures || !load( textureBuffer, "glTextureBuffer" ) ) {
		o.why = "no glCreateTextures / glTextureBuffer";
		return;
	}
	std::uint32_t seed = 0x2545F491u + std::uint32_t( red );
	auto fill = [&seed]( std::vector<std::uint8_t> & v ) {
		for ( std::uint8_t & b : v )
			b = std::uint8_t( ( seed = seed * 1664525u + 1013904223u ) >> 24 );
	};
	unsigned buf = 0, ibuf = 0;
	bool stored = false, istored = false, moved = false;
	GLuint tex = 0, itex = 0;
	// one crossing: GL's buffer texture through objBlTexelCopy, then Vulkan's bytes of its view against `want`
	auto check = [&]( GLuint t, unsigned b, const std::vector<std::uint8_t> & want, std::size_t & differ ) {
		std::vector<unsigned> names;
		const ObjBlStoreStat before = objBlTexelStoreStat();
		if ( !objBlTexelCopy( glProc, t, names, o.why ) )
			return;
		o.copiedBytes += objBlTexelStoreStat().copiedBytes - before.copiedBytes;
		o.imported = o.imported && objBlTexelImported( t );
		o.namesOk = o.namesOk && names.size() == 1 && names[0] == b;
		std::vector<std::uint8_t> got;
		if ( !objBlTexelRead( t, got, o.why ) )
			return;
		o.reads++;
		differ = want.size() > got.size() ? want.size() : 0;
		for ( std::size_t k = 0; k < want.size() && k < got.size(); k++ )
			differ += got[k] != want[k];
	};
	std::vector<std::uint8_t> a( 4096 );
	fill( a );
	objBlTexelFill( buf, stored, true, { { a.data(), a.size() } }, moved );
	o.stored = stored && objBlTexelStoreIs( buf );
	if ( !o.stored ) {
		o.why = "the fill made no store";
		return;
	}
	ux.createTextures( 0x8C2A, 1, &tex );  // TEXTURE_BUFFER
	textureBuffer( tex, 0x8814, GLuint( buf ) );  // RGBA32F, the whole store (as the owners attach)
	check( tex, buf, a, o.d1 );
	// frame 2: GL's patch into the store (the flicker's glBufferSubData)
	std::vector<std::uint8_t> b2 = a, patch( 512 );
	fill( patch );
	std::copy( patch.begin(), patch.end(), b2.begin() + 512 );
	std::string w;
	if ( red != 1 )
		objBlTexelStoreWrite( buf, 512, patch.size(), patch.data(), w );
	check( tex, buf, b2, o.d2 );
	// frame 3: a bigger fill grows the store; the owner re-attaches the moved name
	std::vector<std::uint8_t> c( 96 * 1024 );
	fill( c );
	const unsigned was = buf;
	objBlTexelFill( buf, stored, true, { { c.data(), c.size() / 2 }, { c.data() + c.size() / 2, c.size() - c.size() / 2 } }, moved );
	o.moved = moved && buf != was && objBlTexelStoreIs( buf ) && !objBlTexelStoreIs( was );
	if ( moved )
		textureBuffer( tex, 0x8814, GLuint( buf ) );
	check( tex, buf, c, o.d3 );
	// the int class (the cluster lists)
	std::vector<std::uint8_t> iv( 2048 );
	fill( iv );
	bool imoved = false;
	objBlTexelFill( ibuf, istored, true, { { iv.data(), iv.size() } }, imoved );
	ux.createTextures( 0x8C2A, 1, &itex );
	textureBuffer( itex, 0x8D82, GLuint( ibuf ) );  // RGBA32I
	check( itex, ibuf, iv, o.di );
	o.subData = objBlTexelStoreStat().subData;
	glDeleteTextures( 1, &tex );
	glDeleteTextures( 1, &itex );
	objBlTexelStoreDrop( buf );
	objBlTexelStoreDrop( ibuf );
}

// the copy each crossing against a store: CPU and GPU ms per crossing, 256 KiB, 200 crossings
bool storeCost( double & copyCpu, double & copyGpu, double & impCpu, double & impGpu, std::string & why )
{
	using namespace WwVkPresent;
	void( WINAPI * createBuffers )( GLsizei, GLuint * ) = nullptr;
	void( WINAPI * deleteBuffers )( GLsizei, const GLuint * ) = nullptr;
	void( WINAPI * namedBufferData )( GLuint, std::intptr_t, const void *, GLenum ) = nullptr;
	void( WINAPI * textureBuffer )( GLuint, GLenum, GLuint ) = nullptr;
	void( WINAPI * genQueries )( GLsizei, GLuint * ) = nullptr;
	void( WINAPI * deleteQueries )( GLsizei, const GLuint * ) = nullptr;
	void( WINAPI * beginQuery )( GLenum, GLuint ) = nullptr;
	void( WINAPI * endQuery )( GLenum ) = nullptr;
	void( WINAPI * queryU64 )( GLuint, GLenum, std::uint64_t * ) = nullptr;
	if ( !load( createBuffers, "glCreateBuffers" ) || !load( deleteBuffers, "glDeleteBuffers" ) || !load( namedBufferData, "glNamedBufferData" )
		|| !load( textureBuffer, "glTextureBuffer" ) || !load( genQueries, "glGenQueries" ) || !load( deleteQueries, "glDeleteQueries" )
		|| !load( beginQuery, "glBeginQuery" ) || !load( endQuery, "glEndQuery" ) || !load( queryU64, "glGetQueryObjectui64v" ) )
		return why = "a GL entry point is missing", false;
	const std::size_t kBytes = 256 * 1024;
	const int kN = 200;
	std::vector<std::uint8_t> z( kBytes );
	for ( std::size_t k = 0; k < z.size(); k++ )
		z[k] = std::uint8_t( k * 131u );
	GLuint plain = 0, texCopy = 0, texImp = 0, q = 0;
	createBuffers( 1, &plain );
	namedBufferData( plain, std::intptr_t( kBytes ), z.data(), 0x88E4 );  // STATIC_DRAW
	unsigned sb = 0;
	bool st = false, mv = false;
	if ( !objBlTexelFill( sb, st, true, { { z.data(), z.size() } }, mv ) )
		return why = "the store was not made", false;
	ux.createTextures( 0x8C2A, 1, &texCopy );
	ux.createTextures( 0x8C2A, 1, &texImp );
	textureBuffer( texCopy, 0x8814, plain );
	textureBuffer( texImp, 0x8814, GLuint( sb ) );
	genQueries( 1, &q );
	bool ok = true;
	auto time = [&]( GLuint tex, double & cpu, double & gpu ) {
		std::vector<unsigned> names;
		ok = ok && objBlTexelCopy( glProc, tex, names, why );  // the twin made outside the timing
		glFinish();
		beginQuery( 0x88BF, q );  // TIME_ELAPSED
		const auto t0 = std::chrono::steady_clock::now();
		for ( int i = 0; i < kN && ok; i++ ) {
			names.clear();
			ok = objBlTexelCopy( glProc, tex, names, why );
		}
		const auto t1 = std::chrono::steady_clock::now();
		endQuery( 0x88BF );
		glFinish();
		std::uint64_t ns = 0;
		queryU64( q, 0x8866, &ns );  // QUERY_RESULT
		cpu = std::chrono::duration<double, std::milli>( t1 - t0 ).count() / kN;
		gpu = double( ns ) * 1e-6 / kN;
	};
	time( texCopy, copyCpu, copyGpu );
	time( texImp, impCpu, impGpu );
	deleteQueries( 1, &q );
	glDeleteTextures( 1, &texCopy );
	glDeleteTextures( 1, &texImp );
	deleteBuffers( 1, &plain );
	objBlTexelStoreDrop( sb );
	return ok;
}

bool storeGate()
{
	StoreOut o;
	storeRun( 0, o );
	bool ok = o.good();
	std::printf( "BLVIEW store lights %s (store %d, read in place %d, GL names the store's %d, copied %llu bytes; Vulkan reads %d: "
		"fill %zu, patch %zu, grown %zu (moved %d), int %zu bytes off; GL writes %s)%s%s\n", ok ? "OK" : "FAIL", int( o.stored ),
		int( o.imported ), int( o.namesOk ), (unsigned long long)o.copiedBytes, o.reads, o.d1, o.d2, o.d3, int( o.moved ), o.di,
		o.subData == 1 ? "glNamedBufferSubData into the store" : o.subData == 0 ? "staged (the store refuses SubData)" : "none",
		o.why.empty() ? "" : ": ", o.why.c_str() );
	StoreOut x;
	storeRun( 1, x );
	const bool fails = !x.good();
	std::printf( "BLVIEW RED storeskip %s (frame 2's write skipped: patch %zu bytes off, fill %zu, grown %zu%s%s)\n", fails ? "FAIL" : "OK",
		x.d2, x.d1, x.d3, x.why.empty() ? "" : ": ", x.why.c_str() );
	ok = ok && fails;
	double cc = 0, cg = 0, ic = 0, ig = 0;
	std::string why;
	const bool timed = storeCost( cc, cg, ic, ig, why );
	std::printf( "BLVIEW store cost %s (256 KiB, 200 crossings, measured offline): copy cpu %.4f gpu %.4f ms per crossing, "
		"import cpu %.4f gpu %.4f ms per crossing%s%s\n", timed ? "OK" : "FAIL", cc, cg, ic, ig, why.empty() ? "" : ": ", why.c_str() );
	ok = ok && timed;
	if ( const GLenum e = glGetError(); e != GL_NO_ERROR ) {
		std::printf( "BLVIEW store FAIL (GL error 0x%x left)\n", unsigned( e ) );
		ok = false;
	}
	return ok;
}

/* lane VKOBJ4 open item 3: the cell frame as shared targets (objBlFrame: colour RGBA16F, depth / stencil D24S8) read by
 * objBlFrameRead from GL's draw framebuffer with GL's stencil state (front and back: ALWAYS, reference 1, KEEP / KEEP /
 * REPLACE), then the slice-7 fixture's 64 shapes drawn into it on Vulkan through objBlRun. The app's shape (4 samples) is
 * made first. BAR (stated before the first run, never widened): the 4-sample frame is made and known (objBlFrameIs); the read
 * takes both colour draw buffers and the depth / stencil as shared targets with GL's stencil state (on, ALWAYS, ref 1,
 * REPLACE on pass, front = back); after the run the stencil is 1 exactly where the depth is under 1 and 0 elsewhere (0
 * mismatches, >= 2000 covered); once dropped the frame is no longer known. Reds (each must FAIL): stoff (the stencil read
 * as off: none written), glown (GL's own D24S8 texture as the depth: the read refuses it).
 * Scissor arm (stated before its first run, never widened): GL's scissor on with the box (64, 21) 128x42 (rows not centred,
 * so a flipped box lands elsewhere); the run is the frame run's with 0 pixels outside the box touched (depth 1, stencil 0)
 * and every pixel inside the box equal, depth and stencil bits, to the same run without a scissor; that run covers >= 200
 * pixels inside the box and >= 200 outside. Red scoff (objBlFrameRead red 3: the test read as off) must FAIL.
 * Tone arm (stated before its first run, never widened): the tone frame (toneFrame, 4 samples) with a 4-sample shared second
 * target as GL's draw framebuffer: the read takes the tone frame as shared targets (4 samples, both colour draw buffers, D24S8
 * with GL's stencil state), the run goes, and after a sample resolve of the depth / stencil the frame bar holds (0
 * mismatches, >= 2000 covered); dropped, the tone frame is no longer known. Red toneunshared (red 4: the borrowed entries
 * not found, as before the share) must FAIL. */
struct FrameOut
{
	bool made4 = false, read = false, stateOk = false, prepped = false, ran = false, dropped = false;
	std::size_t covered = 0, mismatch = ~std::size_t( 0 ), outside = 0;  // outside: pixels touched outside the scissor box
	std::size_t minCovered = 2000;  // the scissor arm: 0 (its floor is frameGate's: >= 200 inside the box, from the base run)
	std::vector<std::uint32_t> px;  // the depth / stencil after the run (1 sample)
	std::string why;
	bool good() const
	{
		return made4 && read && stateOk && prepped && ran && covered >= minCovered && mismatch == 0 && outside == 0 && dropped;
	}
};
constexpr int kScX = kSW / 4, kScY = kSH / 6, kScW = kSW / 2, kScH = kSH / 3;  // the scissor arm's box
bool inBox( std::size_t i )
{
	const int x = int( i % kSW ), y = int( i / kSW );
	return x >= kScX && x < kScX + kScW && y >= kScY && y < kScY + kScH;
}

// arm 0 the cell frame (objBlFrame), 1 the same with GL's scissor on (base: the arm-0 run inside the box), 2 the tone frame
void frameRun( int red, FrameOut & o, int arm = 0, const FrameOut * base = nullptr )
{
	using namespace WwVkPresent;
	void( WINAPI * clearBufferfi )( GLenum, GLint, GLfloat, GLint ) = nullptr;
	if ( !load( clearBufferfi, "glClearBufferfi" ) ) {
		o.why = "no glClearBufferfi";
		return;
	}
	unsigned color = 0, ds = 0, t1 = sx.t1;
	static char toneT1;  // the tone arm's second colour target (4 samples, as the frame)
	const bool tone = arm == 2;
	if ( tone ) {
		o.made4 = toneFrame( kSW, kSH, 4, color, ds, o.why ) && toneFrameIs( color )
			&& sharedTarget( reinterpret_cast<std::uintptr_t>( &toneT1 ), 0, kSW, kSH, 1, kRgba16fi, t1, o.why, 1, 4 );
		if ( !o.made4 ) {
			toneFrameDrop();
			sharedTargetDrop( reinterpret_cast<std::uintptr_t>( &toneT1 ) );
			return;
		}
	} else {
		o.made4 = objBlFrame( kSW, kSH, 4, kRgba16fi, 0x88F0, color, ds, o.why ) && objBlFrameIs( color );  // DEPTH24_STENCIL8
		if ( !o.made4 || !objBlFrame( kSW, kSH, 1, kRgba16fi, 0x88F0, color, ds, o.why ) )
			return;
	}
	const GLenum texT = tone ? 0x9100 : GL_TEXTURE_2D;  // TEXTURE_2D_MULTISAMPLE
	GLuint own = 0, fbo = 0;
	if ( red == 2 ) {  // GL's own depth / stencil texture
		ux.createTextures( GL_TEXTURE_2D, 1, &own );
		ux.textureStorage2D( own, 1, 0x88F0, kSW, kSH );
	}
	f.genFramebuffers( 1, &fbo );
	f.bindFramebuffer( kFb, fbo );
	f.framebufferTexture2D( kFb, kColorAtt0, texT, color, 0 );
	f.framebufferTexture2D( kFb, kColorAtt0 + 1, texT, t1, 0 );
	f.framebufferTexture2D( kFb, 0x821A, red == 2 ? GL_TEXTURE_2D : texT, red == 2 ? own : ds, 0 );  // DEPTH_STENCIL_ATTACHMENT
	const GLenum dbs[2] = { kColorAtt0, kColorAtt0 + 1 };
	sf.drawBuffers( 2, dbs );
	if ( f.checkFramebufferStatus( kFb ) != kComplete ) {
		o.why = "the frame's GL framebuffer is not complete";
		f.bindFramebuffer( kFb, 0 );
		return;
	}
	const GLfloat zero[4] = {};
	glDepthMask( GL_TRUE );
	glStencilMask( 0xFF );
	sf.clearBufferfv( 0x1800, 0, zero );
	sf.clearBufferfv( 0x1800, 1, zero );
	clearBufferfi( 0x84F9, 0, 1.0f, 0 );  // GL_DEPTH_STENCIL
	glViewport( 0, 0, kSW, kSH );
	glEnable( GL_DEPTH_TEST );
	glDepthFunc( GL_LESS );
	glEnable( GL_CULL_FACE );
	glCullFace( GL_BACK );
	glFrontFace( GL_CCW );
	glEnable( GL_STENCIL_TEST );
	glStencilFunc( GL_ALWAYS, 1, 0xFF );
	glStencilOp( GL_KEEP, GL_KEEP, GL_REPLACE );
	if ( arm == 1 ) {
		glEnable( GL_SCISSOR_TEST );
		glScissor( kScX, kScY, kScW, kScH );
	}
	ObjBlDraw d;
	o.read = objBlFrameRead( glProc, d, o.why, red == 1 ? 2 : red >= 3 ? red : 0 );
	glDisable( GL_SCISSOR_TEST );
	glDisable( GL_STENCIL_TEST );
	glDisable( GL_CULL_FACE );
	glDisable( GL_DEPTH_TEST );
	f.bindFramebuffer( kFb, 0 );
	if ( o.read ) {
		const ObjBlStencil & sg = d.target.stencil;
		const bool same = sg.func[0] == sg.func[1] && sg.pass[0] == sg.pass[1] && sg.ref[0] == sg.ref[1] && sg.fail[0] == sg.fail[1]
			&& sg.depthFail[0] == sg.depthFail[1];
		o.stateOk = d.target.colorCount == 2 && d.target.depth == VK_FORMAT_D24_UNORM_S8_UINT && d.w == kSW && d.h == kSH && sg.on == 1
			&& sg.func[0] == VK_COMPARE_OP_ALWAYS && sg.ref[0] == 1 && sg.pass[0] == VK_STENCIL_OP_REPLACE && sg.fail[0] == VK_STENCIL_OP_KEEP
			&& sg.depthFail[0] == VK_STENCIL_OP_KEEP && same && d.target.samples == ( tone ? 4u : 1u );
		if ( red == 1 )
			o.stateOk = d.target.colorCount == 2 && d.target.depth == VK_FORMAT_D24_UNORM_S8_UINT;  // the red draws on, stencil off
		std::vector<unsigned> names;
		if ( objBlPoolCopy( sx.key, sx.pool, names, o.why ) ) {
			d.twin = 0;
			d.vs = sx.vs.data();
			d.vsWords = sx.vs.size();
			d.ps = sx.ps.data();
			d.psWords = sx.ps.size();
			d.poolKey = sx.key;
			d.attrMask = sx.pool.attrMask;
			d.need = 0x3EFFu;
			d.ubos = sx.ubos;
			d.glRows = sx.rows.data();
			d.rows = sx.rows.size() / kBlRowBytes;
			d.cmds = sx.cmds.data();
			d.cmdCount = sx.cmds.size() / 5;
			objBlDrawsReset();
			ObjBlDrawStat pst;
			o.prepped = objBlDrawPrep( d, pst, o.why );
			if ( o.prepped )
				o.ran = objBlRun( names, 0, o.why );
		}
		glFinish();
		GLuint readDs = ds, one = 0, rfbo = 0;
		void( WINAPI * blit )( GLint, GLint, GLint, GLint, GLint, GLint, GLint, GLint, GLbitfield, GLenum ) = nullptr;
		if ( tone && load( blit, "glBlitFramebuffer" ) ) {  // the 4 samples resolved to one (depth and stencil from one sample)
			ux.createTextures( GL_TEXTURE_2D, 1, &one );
			ux.textureStorage2D( one, 1, 0x88F0, kSW, kSH );
			f.genFramebuffers( 1, &rfbo );
			f.bindFramebuffer( kFb, rfbo );
			f.framebufferTexture2D( kFb, 0x821A, GL_TEXTURE_2D, one, 0 );
			f.bindFramebuffer( 0x8CA8, fbo );  // READ_FRAMEBUFFER
			blit( 0, 0, kSW, kSH, 0, 0, kSW, kSH, GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT, GL_NEAREST );
			f.bindFramebuffer( kFb, 0 );
			readDs = one;
		}
		std::vector<std::uint32_t> px( std::size_t( kSW ) * kSH, 0 );
		f.getTextureImage( readDs, 0, 0x84F9, 0x84FA, GLsizei( px.size() * 4 ), px.data() );  // DEPTH_STENCIL, UNSIGNED_INT_24_8
		o.covered = 0;
		o.mismatch = 0;
		o.outside = 0;
		for ( std::size_t i = 0; i < px.size(); i++ ) {
			const std::uint32_t p = px[i];
			const bool covered = ( p >> 8 ) < 0xFFFFFFu;
			o.covered += covered;
			o.mismatch += ( p & 0xFFu ) != ( covered ? 1u : 0u );
			if ( arm == 1 && !inBox( i ) )
				o.outside += p != 0xFFFFFF00u;
			else if ( arm == 1 )
				o.mismatch += !base || base->px.size() != px.size() || base->px[i] != p;
		}
		if ( arm == 1 )
			o.minCovered = 0;
		o.px = px;
		if ( one )
			glDeleteTextures( 1, &one );
		void( WINAPI * delFbo )( GLsizei, const GLuint * ) = nullptr;
		if ( rfbo && load( delFbo, "glDeleteFramebuffers" ) )
			delFbo( 1, &rfbo );
	}
	void( WINAPI * deleteFramebuffers )( GLsizei, const GLuint * ) = nullptr;
	if ( load( deleteFramebuffers, "glDeleteFramebuffers" ) )
		deleteFramebuffers( 1, &fbo );
	if ( own )
		glDeleteTextures( 1, &own );
	if ( tone ) {
		toneFrameDrop();
		sharedTargetDrop( reinterpret_cast<std::uintptr_t>( &toneT1 ) );
		o.dropped = !toneFrameIs( color );
	} else {
		objBlFrameDrop();
		o.dropped = !objBlFrameIs( color );
	}
}

bool frameGate()
{
	FrameOut o;
	frameRun( 0, o );
	bool ok = o.good();
	std::printf( "BLVIEW frame shared %s (4-sample frame made %d; read %d, stencil state %d; prep %d, crossing %d; %zu covered, %zu stencil "
		"mismatches; dropped %d)%s%s\n", ok ? "OK" : "FAIL", int( o.made4 ), int( o.read ), int( o.stateOk ), int( o.prepped ), int( o.ran ),
		o.covered, o.mismatch, int( o.dropped ), o.why.empty() ? "" : ": ", o.why.c_str() );
	const char * reds[2] = { "stoff", "glown" };
	const char * redWhy[2] = { "the stencil read as off", "GL's own D24S8 texture as the depth" };
	for ( int r = 1; r <= 2; r++ ) {
		FrameOut x;
		frameRun( r, x );
		const bool fails = !x.good();
		std::printf( "BLVIEW RED frame%s %s (%s; read %d, %zu covered, %zu mismatches%s%s)\n", reds[r - 1], fails ? "FAIL" : "OK", redWhy[r - 1],
			int( x.read ), x.covered, x.mismatch, x.why.empty() ? "" : ": ", x.why.c_str() );
		ok = ok && fails;
	}
	{  // the scissor arm: GL's box as the draw's
		FrameOut b, s, r;
		frameRun( 0, b );
		frameRun( 0, s, 1, &b );
		frameRun( 3, r, 1, &b );
		std::size_t bIn = 0, bOut = 0;
		for ( std::size_t i = 0; i < b.px.size(); i++ )
			( inBox( i ) ? bIn : bOut ) += ( b.px[i] >> 8 ) < 0xFFFFFFu;
		const bool sOk = b.good() && s.good() && bIn >= 200 && bOut >= 200;
		std::printf( "BLVIEW frame scissor %s (box %d,%d %dx%d; without it %zu covered inside, %zu outside; with it %zu touched outside, %zu "
			"inside mismatches%s%s)\n", sOk ? "OK" : "FAIL", kScX, kScY, kScW, kScH, bIn, bOut, s.outside, s.mismatch, s.why.empty() ? "" : ": ",
			s.why.c_str() );
		const bool rFails = !r.good();
		std::printf( "BLVIEW RED framescoff %s (the scissor read as off; %zu touched outside the box, %zu inside mismatches)\n", rFails ? "FAIL" : "OK",
			r.outside, r.mismatch );
		ok = ok && sOk && rFails;
	}
	{  // the tone arm: the tone frame as the shared frame
		FrameOut t, r;
		frameRun( 0, t, 2 );
		frameRun( 4, r, 2 );
		std::printf( "BLVIEW frame tone %s (4-sample tone frame made %d; read %d, state %d; prep %d, crossing %d; %zu covered, %zu stencil "
			"mismatches; dropped %d)%s%s\n", t.good() ? "OK" : "FAIL", int( t.made4 ), int( t.read ), int( t.stateOk ), int( t.prepped ),
			int( t.ran ), t.covered, t.mismatch, int( t.dropped ), t.why.empty() ? "" : ": ", t.why.c_str() );
		const bool rFails = !r.good();
		std::printf( "BLVIEW RED frametoneunshared %s (the borrowed entries not found; read %d%s%s)\n", rFails ? "FAIL" : "OK", int( r.read ),
			r.why.empty() ? "" : ": ", r.why.c_str() );
		ok = ok && t.good() && rFails;
	}
	if ( const GLenum e = glGetError(); e != GL_NO_ERROR ) {
		std::printf( "BLVIEW frame FAIL (GL error 0x%x left)\n", unsigned( e ) );
		ok = false;
	}
	return ok;
}

/* lane VKOBJ4 open item 6: GL's blend as the pipeline's (objBlTargetState, ObjBlTarget.blend).
 * BAR (stated before the first run, never widened):
 * (a) GL's state SRC_ALPHA / ONE_MINUS_SRC_ALPHA on colour, ONE / ONE on alpha, equations ADD / MAX, the mask RGB on and
 *     A off, on both draw buffers, reads as VkBlendFactor 6 / 7 / 1 / 1, VkBlendOp 0 / 4, writeMask 0x7, blend on;
 * (b) the shade run into targets cleared to 0.25 with ONE / ONE ADD on colour and ONE / ONE MAX on alpha gives, in every value of
 *     both targets, 0.25 + off on rgb and max(0.25, off) on alpha (off: the same run without blend into targets cleared to 0),
 *     within one half ulp of that sum; the run without blend has >= 2000 non-zero values.
 * Reds (each must FAIL): swap (objBlTargetState's red 1: source and destination read swapped), off (the blended run with blend
 * off in the target). */
float fromHalf( std::uint16_t h )
{
	const int e = ( h >> 10 ) & 0x1F, m = h & 0x3FF;
	const float v = e == 0 ? std::ldexp( float( m ), -24 ) : e == 31 ? 65504.0f * 2.0f : std::ldexp( float( m | 0x400 ), e - 25 );
	return ( h & 0x8000 ) ? -v : v;
}

struct BlendFns
{
	void( WINAPI * funcSep )( GLenum, GLenum, GLenum, GLenum ) = nullptr;
	void( WINAPI * eqSep )( GLenum, GLenum ) = nullptr;
	void( WINAPI * colorMaski )( GLuint, GLboolean, GLboolean, GLboolean, GLboolean ) = nullptr;
};

bool blendStateRead( int red, WwVkPresent::ObjBlTarget & t, std::string & why )
{
	BlendFns b;
	if ( !load( b.funcSep, "glBlendFuncSeparate" ) || !load( b.eqSep, "glBlendEquationSeparate" ) || !load( b.colorMaski, "glColorMaski" ) )
		return why = "a GL blend entry point is missing", false;
	glEnable( GL_DEPTH_TEST );
	glEnable( GL_CULL_FACE );
	glCullFace( GL_BACK );
	glFrontFace( GL_CCW );
	glEnable( GL_BLEND );
	b.funcSep( 0x0302, 0x0303, 1, 1 );
	b.eqSep( 0x8006, 0x8008 );
	for ( GLuint c = 0; c < 2; c++ )
		b.colorMaski( c, GL_TRUE, GL_TRUE, GL_TRUE, GL_FALSE );
	const bool ok = WwVkPresent::objBlTargetState( glProc, 2, t, why, red );
	for ( GLuint c = 0; c < 2; c++ )
		b.colorMaski( c, GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE );
	b.funcSep( 1, 0, 1, 0 );
	b.eqSep( 0x8006, 0x8006 );
	glDisable( GL_BLEND );
	glDisable( GL_CULL_FACE );
	glDisable( GL_DEPTH_TEST );
	return ok;
}

bool blendStateGood( const WwVkPresent::ObjBlTarget & t )
{
	const WwVkPresent::ObjBlBlend & b = t.blend;
	return b.on == 1 && b.srcColor == 6 && b.dstColor == 7 && b.srcAlpha == 1 && b.dstAlpha == 1 && b.colorOp == 0 && b.alphaOp == 4
		&& b.writeMask == 0x7 && !t.twoSided;
}

struct BlendCmp
{
	std::size_t values = 0, lit = 0, off = 0;
	float maxUlps = 0.0f;
	bool good() const { return values > 0 && lit >= 2000 && off == 0; }
};

BlendCmp blendCmp( const std::vector<std::uint16_t> * base, const std::vector<std::uint16_t> * got, float k )
{
	BlendCmp c;
	for ( int t = 0; t < 2; t++ ) {
		if ( base[t].size() != got[t].size() || base[t].empty() )
			return BlendCmp();
		for ( std::size_t i = 0; i < base[t].size(); i++ ) {
			const float o = fromHalf( base[t][i] ), g = fromHalf( got[t][i] );
			c.lit += base[t][i] != 0;
			const float e = ( i % 4 ) == 3 ? std::max( k, o ) : k + o;
			int ex = 0;
			std::frexp( e, &ex );
			const float ulp = std::max( std::ldexp( 1.0f, ex - 11 ), std::ldexp( 1.0f, -24 ) );
			const float u = std::fabs( g - e ) / ulp;
			c.values++;
			c.maxUlps = std::max( c.maxUlps, u );
			c.off += u > 1.0f;
		}
	}
	return c;
}

bool blendGate()
{
	using namespace WwVkPresent;
	std::string why;
	ObjBlTarget t;
	const bool read = blendStateRead( 0, t, why );
	bool ok = read && blendStateGood( t );
	std::printf( "BLVIEW blend state %s (on %u, colour %u / %u op %u, alpha %u / %u op %u, mask 0x%x)%s%s\n", ok ? "OK" : "FAIL", t.blend.on,
		t.blend.srcColor, t.blend.dstColor, t.blend.colorOp, t.blend.srcAlpha, t.blend.dstAlpha, t.blend.alphaOp, t.blend.writeMask,
		why.empty() ? "" : ": ", why.c_str() );
	{
		ObjBlTarget r;
		std::string w;
		const bool fails = !blendStateRead( 1, r, w ) || !blendStateGood( r );
		std::printf( "BLVIEW RED blendswap %s (source and destination read swapped: colour %u / %u)\n", fails ? "FAIL" : "OK", r.blend.srcColor,
			r.blend.dstColor );
		ok = ok && fails;
	}
	const float k = 0.25f;
	ObjBlBlend add;
	add.on = 1;
	add.srcColor = add.dstColor = add.srcAlpha = add.dstAlpha = 1;
	add.colorOp = 0;
	add.alphaOp = 4;
	std::vector<float> d0, d1, d2;
	std::vector<std::uint16_t> base[2], got[2], red[2];
	ObjBlDrawStat st;
	const bool ran = shadeVk( 0, 0, d0, base[0], base[1], st, why ) && shadeVk( 0, 0, d1, got[0], got[1], st, why, &add, k );
	const BlendCmp c = ran ? blendCmp( base, got, k ) : BlendCmp();
	std::printf( "BLVIEW blend run %s (%zu values, %zu lit without blend, %zu off by more than one half ulp, max %.3g ulp)%s%s\n",
		c.good() ? "OK" : "FAIL", c.values, c.lit, c.off, double( c.maxUlps ), ran ? "" : ": ", ran ? "" : why.c_str() );
	ok = ok && c.good();
	{
		ObjBlBlend none = add;
		none.on = 0;
		const bool rr = ran && shadeVk( 0, 0, d2, red[0], red[1], st, why, &none, k );
		const BlendCmp x = rr ? blendCmp( base, red, k ) : BlendCmp();
		std::printf( "BLVIEW RED blendoff %s (the run with blend off; %zu off)\n", x.good() ? "OK" : "FAIL", x.off );
		ok = ok && !x.good();
	}
	return ok;
}

bool shadeGate( const std::string & spvDir )
{
	using namespace WwVkPresent;
	std::string why;
	if ( !shadeBuild( spvDir, why ) ) {
		std::printf( "BLVIEW shade FAIL (the fixture: %s)\n", why.c_str() );
		return false;
	}
	std::vector<float> glDepth;
	if ( !shadeGl( glDepth, why ) ) {
		std::printf( "BLVIEW shade FAIL (GL's draw: %s)\n", why.c_str() );
		return false;
	}
	bool ok = true;
	takeObjBlRunStat();  // the count starts here
	std::vector<float> dep[3];
	std::vector<std::uint16_t> c0[3], c1[3];
	const char * routes[3] = { "mdi", "per-command", "direct" };
	const Vk & v = vk();
	for ( int m = 0; m < 3; m++ ) {
		ObjBlDrawStat st;
		if ( !shadeVk( m, 0, dep[m], c0[m], c1[m], st, why ) ) {
			std::printf( "BLVIEW shade %s FAIL (%s)\n", routes[m], why.c_str() );
			ok = false;
			continue;
		}
		const DepthCmp c = depthCmp( dep[m], glDepth );
		if ( m == 0 )
			std::printf( "BLVIEW shade set 0: %d ubos, %d images given, %d stand-ins, %d texel buffers, %d samplers, %d row buffers; %zu rows whole; "
				"device multiDrawIndirect %d, firstInstance %d\n", st.ubos, st.images, st.standIns, st.texels, st.samplers, st.rowBuffers, st.whole,
				int( v.multiDrawIndirect ), int( v.firstInstance ) );
		std::printf( "BLVIEW shade %s vs GL depth %s (%zu covered, %zu on one side, %zu differ, max %.3g; %zu commands)\n", routes[m], c.word(),
			c.covered, c.oneSide, c.differ, double( c.maxAbs ), sx.cmds.size() / 5 );
		ok = ok && ( c.exact() || c.bound() );
		if ( m > 0 && !dep[0].empty() ) {
			const bool same = dep[m] == dep[0] && c0[m] == c0[0] && c1[m] == c1[0];
			std::size_t lit = 0;
			for ( std::size_t i = 0; i < c0[0].size(); i++ )
				lit += c0[0][i] != 0 || c1[0][i] != 0;
			std::printf( "BLVIEW shade %s vs mdi %s (depth + both targets; %zu target values non-zero)\n", routes[m], same ? "EXACT" : "FAIL", lit );
			ok = ok && same;
		}
	}
	{
		const ObjBlRunStat rs = takeObjBlRunStat();
		const bool good = rs.runs == 3 && rs.draws == 3 && rs.fails == 0 && rs.cmds == int( 3 * sx.cmds.size() / 5 );
		std::printf( "BLVIEW shade crossing %s (%d runs, %d draws, %d commands, %d failed; %.3f ms mean, %.3f max)%s%s\n", good ? "OK" : "FAIL", rs.runs,
			rs.draws, rs.cmds, rs.fails, rs.runs ? rs.cpuMs / rs.runs : 0.0, rs.maxMs, rs.failWhy.empty() ? "" : ": ", rs.failWhy.c_str() );
		ok = ok && good;
	}
	const char * reds[3] = { "base", "inst", "cull" };
	const char * redWhy[3] = { "wwDrawBase 0 in the Vulkan state", "every base instance 0", "twoSided: the mirrored shapes drawn" };
	for ( int r = 1; r <= 3; r++ ) {
		ObjBlDrawStat st;
		std::vector<float> d;
		std::vector<std::uint16_t> a, b;
		const bool ran = shadeVk( 0, r, d, a, b, st, why );
		const DepthCmp c = ran ? depthCmp( d, glDepth ) : DepthCmp();
		const bool fails = !ran || !( c.exact() || c.bound() );
		std::printf( "BLVIEW RED shade%s %s (%s; %zu on one side, %zu differ%s%s)\n", reds[r - 1], fails ? "FAIL" : c.word(), redWhy[r - 1],
			c.oneSide, c.differ, ran ? "" : "; ", ran ? "" : why.c_str() );
		ok = ok && fails;
	}
	ok = unitsGate( spvDir ) && ok;
	ok = texelGate() && ok;  // lane VKOBJ4 open item 4
	ok = storeGate() && ok;  // lane VKOBJ4 gap 2
	ok = frameGate() && ok;  // lane VKOBJ4 open item 3
	ok = blendGate() && ok;
	if ( const GLenum e = glGetError(); e != GL_NO_ERROR ) {
		std::printf( "BLVIEW shade FAIL (GL error 0x%x left)\n", unsigned( e ) );
		ok = false;
	}
	return ok;
}
