// Lane VKOBJ4 (2026-11-13, row G4 slice 5): GL's program state mirrored into the bl twins' set 0 (vkobjpipe.inc: objBlLoose,
// objBlBlock, objBlTextures), read through the SHIPPED modules (objbl_*.spv.h). Included by blview.cpp inside namespace blv.
// Fixture: per twin one GL program declaring every LooseUniforms member of the vertex and the pixel module by name (GLSL
// types from the module: float / int / uint, a uint member is GLSL bool or uint by its name's hash, matrices matCxR,
// arrays), each used; values by the name's hash per element and component, set with glUniform*; the real
// res/shaders/uniforms.glsl globalUniforms and bonetransform.glsl skinningUniforms blocks on random bytes, globalUniforms
// bound by range at an offset; the first 24 pixel-stage images declared as GL samplers on their own units, a texture of
// the binding's target on each. Lines: `BLVIEW uni <twin> loose|block|tex ... EXACT|FAIL`, `BLVIEW RED transpose|stride|
// binding ...` (each must FAIL). BAR (stated before the first run, never widened): EXACT (0 bytes differ; every member
// mirrored, none absent; every declared sampler's unit and texture found).
#include "features/vkframe/objbl_vs.spv.h"
#include "features/vkframe/objbl_cell_ps.spv.h"
#include "features/vkframe/objbl_cellcsm_ps.spv.h"

std::uint32_t uniHash( const std::string & s )
{
	std::uint32_t h = 2166136261u;
	for ( char c : s )
		h = ( h ^ std::uint8_t( c ) ) * 16777619u;
	return h;
}

// a GLSL bool or uint for a module uint (the twins' cbuffer bools are uints in the module)
bool uniIsBool( const WwVkPresent::SpvMember & m )
{
	return m.base == 2 && m.cols == 1 && m.comps == 1 && ( uniHash( m.name ) & 1u );
}

std::string uniGlslType( const WwVkPresent::SpvMember & m )
{
	if ( m.cols > 1 )
		return m.cols == m.comps ? "mat" + std::to_string( m.cols ) : "mat" + std::to_string( m.cols ) + "x" + std::to_string( m.comps );
	if ( uniIsBool( m ) )
		return "bool";
	const char * scalar[3] = { "float", "int", "uint" }, * vec[3] = { "vec", "ivec", "uvec" };
	return m.comps == 1 ? scalar[m.base] : vec[m.base] + std::to_string( m.comps );
}

// the value of element e, column c, row r (4 bytes, as the module holds it)
std::uint32_t uniValue( const WwVkPresent::SpvMember & m, std::uint32_t e, std::uint32_t c, std::uint32_t r )
{
	const std::uint32_t h = uniHash( m.name ) + e * 977u + c * 31u + r * 7u;
	if ( uniIsBool( m ) )
		return ( h >> 3 ) & 1u;
	if ( m.base == 0 ) {
		const float v = float( int( h % 20001u ) - 10000 ) * 0.03125f;
		std::uint32_t b = 0;
		std::memcpy( &b, &v, 4 );
		return b;
	}
	return m.base == 1 ? std::uint32_t( int( h % 200001u ) - 100000 ) : h % 4000000000u;
}

// GLSL that reads every component of the member (so GL keeps it)
std::string uniUse( const WwVkPresent::SpvMember & m )
{
	std::string s;
	for ( std::uint32_t e = 0; e < m.count; e++ ) {
		const std::string x = m.count > 1 ? m.name + "[" + std::to_string( e ) + "]" : m.name;
		if ( m.cols > 1 )
			for ( std::uint32_t c = 0; c < m.cols; c++ )
				s += "\ts += dot( " + x + "[" + std::to_string( c ) + "], vec" + std::to_string( m.comps ) + "( 1.0 ) );\n";
		else if ( uniIsBool( m ) )
			s += "\ts += " + x + " ? 1.0 : 0.0;\n";
		else if ( m.comps == 1 )
			s += "\ts += float( " + x + " );\n";
		else
			s += "\ts += dot( vec" + std::to_string( m.comps ) + "( " + x + " ), vec" + std::to_string( m.comps ) + "( 1.0 ) );\n";
	}
	return s;
}

const WwVkPresent::SpvBind * uniBlock( const WwVkPresent::SpvModule & m, const char * name )
{
	for ( const WwVkPresent::SpvBind & b : m.binds )
		if ( b.type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER && b.name.find( name ) != std::string::npos )
			return &b;
	return nullptr;
}

// the bytes objBlLoose must give: every element's components at the module's offsets
std::vector<std::uint8_t> uniExpect( const WwVkPresent::SpvBind & b )
{
	std::vector<std::uint8_t> out( b.bytes, 0 );
	for ( const WwVkPresent::SpvMember & m : b.members )
		for ( std::uint32_t e = 0; e < m.count; e++ )
			for ( std::uint32_t c = 0; c < m.cols; c++ )
				for ( std::uint32_t r = 0; r < m.comps; r++ ) {
					const std::uint32_t at = m.offset + e * m.arrayStride
						+ ( m.cols > 1 ? ( m.rowMajor ? r * m.colStride + c * 4 : c * m.colStride + r * 4 ) : r * 4 );
					const std::uint32_t v = uniValue( m, e, c, r );
					if ( at + 4 <= out.size() )
						std::memcpy( out.data() + at, &v, 4 );
				}
	return out;
}

std::size_t uniDiff( const std::vector<std::uint8_t> & a, const std::vector<std::uint8_t> & b )
{
	std::size_t n = a.size() == b.size() ? 0 : std::max( a.size(), b.size() );
	for ( std::size_t i = 0; i < std::min( a.size(), b.size() ); i++ )
		n += a[i] != b[i];
	return n;
}

bool uniGate( const std::string & root )
{
	using namespace WwVkPresent;
	struct UniGl
	{
		void( WINAPI * uf[4] )( GLint, GLsizei, const GLfloat * ) = {};
		void( WINAPI * ui[4] )( GLint, GLsizei, const GLint * ) = {};
		void( WINAPI * uu[4] )( GLint, GLsizei, const GLuint * ) = {};
		void( WINAPI * um[3][3] )( GLint, GLsizei, GLboolean, const GLfloat * ) = {};  // [cols - 2][rows - 2]
		GLint( WINAPI * loc )( GLuint, const char * ) = nullptr;
		void( WINAPI * bindRange )( GLenum, GLuint, GLuint, std::intptr_t, std::intptr_t ) = nullptr;
		void( WINAPI * createTextures )( GLenum, GLsizei, GLuint * ) = nullptr;
		void( WINAPI * bindTextureUnit )( GLuint, GLuint ) = nullptr;
		void( WINAPI * textureStorage2D )( GLuint, GLsizei, GLenum, GLsizei, GLsizei ) = nullptr;
		void( WINAPI * textureStorage3D )( GLuint, GLsizei, GLenum, GLsizei, GLsizei, GLsizei ) = nullptr;
		void( WINAPI * textureBuffer )( GLuint, GLenum, GLuint ) = nullptr;
		void( WINAPI * deleteProgram )( GLuint ) = nullptr;
	} u;
	const char * n4 = "1234";
	bool okLoad = true;
	for ( int i = 0; i < 4; i++ ) {
		okLoad = okLoad && load( u.uf[i], ( std::string( "glUniform" ) + n4[i] + "fv" ).c_str() )
			&& load( u.ui[i], ( std::string( "glUniform" ) + n4[i] + "iv" ).c_str() )
			&& load( u.uu[i], ( std::string( "glUniform" ) + n4[i] + "uiv" ).c_str() );
	}
	for ( int c = 2; c <= 4; c++ )
		for ( int r = 2; r <= 4; r++ ) {
			const std::string nm = c == r ? "glUniformMatrix" + std::to_string( c ) + "fv" : "glUniformMatrix" + std::to_string( c ) + "x" + std::to_string( r ) + "fv";
			okLoad = okLoad && load( u.um[c - 2][r - 2], nm.c_str() );
		}
	okLoad = okLoad && load( u.loc, "glGetUniformLocation" ) && load( u.bindRange, "glBindBufferRange" ) && load( u.createTextures, "glCreateTextures" )
		&& load( u.bindTextureUnit, "glBindTextureUnit" ) && load( u.textureStorage2D, "glTextureStorage2D" )
		&& load( u.textureStorage3D, "glTextureStorage3D" ) && load( u.textureBuffer, "glTextureBuffer" ) && load( u.deleteProgram, "glDeleteProgram" );
	if ( !okLoad ) {
		std::printf( "BLVIEW uni: GL entry points missing\n" );
		return false;
	}
	std::string why;
	SpvModule vsM, psM[2];
	const char * twins[2] = { "objbl_cell_ps", "objbl_cellcsm_ps" };
	if ( !spvReflect( vkframe_objbl_vs_spv, sizeof( vkframe_objbl_vs_spv ) / 4, vsM, why )
		|| !spvReflect( vkframe_objbl_cell_ps_spv, sizeof( vkframe_objbl_cell_ps_spv ) / 4, psM[0], why )
		|| !spvReflect( vkframe_objbl_cellcsm_ps_spv, sizeof( vkframe_objbl_cellcsm_ps_spv ) / 4, psM[1], why ) ) {
		std::printf( "BLVIEW uni: reflection refused (%s)\n", why.c_str() );
		return false;
	}
	// the real GL blocks: globalUniforms from uniforms.glsl, skinningUniforms from bonetransform.glsl (its first block)
	std::string glob = readFile( root + "/res/shaders/uniforms.glsl" ), skin = readFile( root + "/res/shaders/bonetransform.glsl" );
	const std::size_t ge = glob.find( "};" ), se = skin.find( "};" );
	if ( ge == std::string::npos || se == std::string::npos || glob.find( "uniform globalUniforms" ) == std::string::npos
		|| skin.find( "uniform skinningUniforms" ) == std::string::npos ) {
		std::printf( "BLVIEW uni: uniforms.glsl / bonetransform.glsl not as expected\n" );
		return false;
	}
	glob = glob.substr( 0, ge + 2 );
	skin = skin.substr( 0, se + 2 );
	const std::string gTag = "layout ( std140 ) uniform globalUniforms", sTag = "layout ( std140 ) uniform skinningUniforms";
	if ( glob.find( gTag ) != 0 || skin.find( sTag ) != 0 ) {
		std::printf( "BLVIEW uni: the blocks' layout lines moved\n" );
		return false;
	}
	glob.replace( 0, gTag.size(), "layout ( std140, binding = 3 ) uniform globalUniforms" );
	skin.replace( 0, sTag.size(), "layout ( std140, binding = 4 ) uniform skinningUniforms" );
	// the block buffers: random bytes, globalUniforms bound by range at 256
	std::mt19937 rng( 77 );
	std::vector<std::uint8_t> gBytes( 1024 ), sBytes( 256 * 48 );
	for ( auto & b : gBytes )
		b = std::uint8_t( rng() );
	for ( auto & b : sBytes )
		b = std::uint8_t( rng() );
	GLuint ubo[2] = {};
	f.genBuffers( 2, ubo );
	f.bindBuffer( 0x8A11, ubo[0] );  // UNIFORM_BUFFER
	f.bufferData( 0x8A11, std::ptrdiff_t( gBytes.size() ), gBytes.data(), 0x88E4 );
	f.bindBuffer( 0x8A11, ubo[1] );
	f.bufferData( 0x8A11, std::ptrdiff_t( sBytes.size() ), sBytes.data(), 0x88E4 );
	u.bindRange( 0x8A11, 3, ubo[0], 256, 512 );
	f.bindBufferBase( 0x8A11, 4, ubo[1] );
	// one texture per target the census meets
	std::map<unsigned, GLuint> texOf;
	auto texFor = [&]( unsigned target ) {
		if ( texOf.count( target ) )
			return texOf[target];
		GLuint t = 0;
		u.createTextures( target, 1, &t );
		if ( target == 0x8C2A ) {
			GLuint b = 0;
			f.genBuffers( 1, &b );
			f.bindBuffer( 0x8C2A, b );
			f.bufferData( 0x8C2A, 256, nullptr, 0x88E4 );
			u.textureBuffer( t, 0x8814, b );  // RGBA32F
		} else if ( target == 0x806F || target == 0x8C1A || target == 0x9009 )
			u.textureStorage3D( t, 1, 0x8058, 4, 4, target == 0x9009 ? 6 : 2 );
		else
			u.textureStorage2D( t, 1, 0x8058, 4, 4 );
		texOf[target] = t;
		return t;
	};
	bool ok = true;
	int redFail[3] = {};
	for ( int t = 0; t < 2; t++ ) {
		const SpvBind * vl = uniBlock( vsM, "LooseUniforms" ), * pl = uniBlock( psM[t], "LooseUniforms" );
		const SpvBind * vg = uniBlock( vsM, "globalUniforms" ), * pg = uniBlock( psM[t], "globalUniforms" ), * vsk = uniBlock( vsM, "skinningUniforms" );
		if ( !vl || !pl || !vg || !pg || !vsk ) {
			std::printf( "BLVIEW uni %s: a block is missing from the modules\n", twins[t] );
			ok = false;
			continue;
		}
		std::string vsSrc = "#version 450\n" + glob + "\n" + skin + "\n", fsSrc = "#version 450\n" + glob + "\n";
		std::string vsUse, fsUse;
		for ( const SpvMember & m : vl->members ) {
			vsSrc += "uniform " + uniGlslType( m ) + " " + m.name + ( m.count > 1 ? "[" + std::to_string( m.count ) + "]" : "" ) + ";\n";
			vsUse += uniUse( m );
		}
		for ( const SpvMember & m : pl->members ) {
			fsSrc += "uniform " + uniGlslType( m ) + " " + m.name + ( m.count > 1 ? "[" + std::to_string( m.count ) + "]" : "" ) + ";\n";
			fsUse += uniUse( m );
		}
		// the census's samplers: the first 24 pixel-stage images, unit = their order
		const char * glslSampler[6] = { "sampler1D", "sampler2D", "sampler3D", "samplerCube", "", "samplerBuffer" };
		std::vector<const SpvBind *> imgs;
		for ( const SpvBind & b : psM[t].binds )
			if ( b.set == 0 && ( b.type == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE || b.type == VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER ) && b.dim <= 5 && b.dim != 4
				&& imgs.size() < 24 )
				imgs.push_back( &b );
		for ( std::size_t i = 0; i < imgs.size(); i++ ) {
			const SpvBind & b = *imgs[i];
			fsSrc += "layout ( binding = " + std::to_string( i ) + " ) uniform " + glslSampler[b.dim] + ( b.arrayed ? "Array " : " " ) + b.name + ";\n";
			fsUse += b.dim == 5 ? "\ts += float( textureSize( " + b.name + " ) );\n" : "\ts += float( textureSize( " + b.name + ", 0 ).x );\n";
		}
		vsSrc += "void main()\n{\n\tfloat s = toneMapScale + boneTransforms[7][1][2];\n" + vsUse + "\tgl_Position = vec4( s );\n}\n";
		fsSrc += "layout ( location = 0 ) out vec4 o;\nvoid main()\n{\n\tfloat s = glowScale + float( viewportDimensions.z );\n" + fsUse + "\to = vec4( s );\n}\n";
		const GLuint prog = program( vsSrc, fsSrc );
		if ( !prog ) {
			std::printf( "BLVIEW uni %s: the fixture program did not link\n", twins[t] );
			ok = false;
			continue;
		}
		f.useProgram( prog );
		for ( const SpvBind * bl : { vl, pl } )
			for ( const SpvMember & m : bl->members )
				for ( std::uint32_t e = 0; e < m.count; e++ ) {
					const std::string nm = m.count > 1 ? m.name + "[" + std::to_string( e ) + "]" : m.name;
					const GLint at = u.loc( prog, nm.c_str() );
					std::uint32_t v[16] = {};
					for ( std::uint32_t c = 0; c < m.cols; c++ )
						for ( std::uint32_t r = 0; r < m.comps; r++ )
							v[c * m.comps + r] = uniValue( m, e, c, r );
					if ( at < 0 )
						continue;
					if ( m.cols > 1 )
						u.um[m.cols - 2][m.comps - 2]( at, 1, GL_FALSE, reinterpret_cast<const GLfloat *>( v ) );
					else if ( uniIsBool( m ) || m.base == 1 )
						u.ui[m.comps - 1]( at, 1, reinterpret_cast<const GLint *>( v ) );
					else if ( m.base == 2 )
						u.uu[m.comps - 1]( at, 1, v );
					else
						u.uf[m.comps - 1]( at, 1, reinterpret_cast<const GLfloat *>( v ) );
				}
		for ( std::size_t i = 0; i < imgs.size(); i++ ) {
			unsigned target = 0, query = 0;
			ouTarget( imgs[i]->dim, imgs[i]->arrayed, target, query );
			u.bindTextureUnit( GLuint( i ), texFor( target ) );
		}
		// loose: each stage's block, EXACT against the values set
		for ( const SpvBind * bl : { vl, pl } ) {
			std::vector<std::uint8_t> got;
			ObjBlUniStat st;
			const bool made = objBlLoose( glProc, prog, *bl, got, st, why );
			const std::size_t d = made ? uniDiff( got, uniExpect( *bl ) ) : 1;
			const bool exact = made && d == 0 && st.absent == 0 && st.mirrored == st.members;
			std::printf( "BLVIEW uni %s loose %s (binding %u): %d members, %d mirrored, %d absent, %u bytes, %zu differ: %s%s\n", twins[t],
				bl == vl ? "vertex" : "pixel", bl->binding, st.members, st.mirrored, st.absent, bl->bytes, d, exact ? "EXACT" : "FAIL",
				made ? "" : ( " (" + why + ")" ).c_str() );
			ok = ok && exact;
			for ( int red = 1; red <= 2; red++ ) {
				std::vector<std::uint8_t> r;
				if ( objBlLoose( glProc, prog, *bl, r, st, why, red ) && uniDiff( r, got ) )
					redFail[red - 1]++;
			}
		}
		// blocks: GL's buffer range, EXACT against the bytes uploaded
		const std::vector<std::uint8_t> * src[3] = { &gBytes, &gBytes, &sBytes };
		const std::size_t start[3] = { 256, 256, 0 };
		const SpvBind * blocks[3] = { vg, pg, vsk };
		for ( int k = 0; k < 3; k++ ) {
			std::vector<std::uint8_t> got;
			const bool made = objBlBlock( glProc, prog, *blocks[k], got, why );
			std::vector<std::uint8_t> want( src[k]->begin() + std::ptrdiff_t( start[k] ),
				src[k]->begin() + std::ptrdiff_t( std::min( src[k]->size(), start[k] + blocks[k]->bytes ) ) );
			const std::size_t d = made ? uniDiff( got, want ) : 1;
			std::printf( "BLVIEW uni %s block %s (%s binding %u, %u bytes, %zu members): %zu differ: %s%s\n", twins[t], blocks[k]->name.c_str(),
				k == 1 ? "pixel" : "vertex", blocks[k]->binding, blocks[k]->bytes, blocks[k]->members.size(), d, made && !d ? "EXACT" : "FAIL",
				made ? "" : ( " (" + why + ")" ).c_str() );
			ok = ok && made && !d;
			std::vector<std::uint8_t> r;
			if ( !objBlBlock( glProc, prog, *blocks[k], r, why, 1 ) || uniDiff( r, want ) )
				redFail[2]++;
		}
		// the census: every declared sampler's unit and texture; the rest absent
		std::vector<ObjBlTexUnit> units;
		const bool made = objBlTextures( glProc, prog, psM[t], units, why );
		int found = 0, wrong = 0, absent = 0, samplers = 0;
		for ( const ObjBlTexUnit & tu : units ) {
			std::size_t i = 0;
			while ( i < imgs.size() && imgs[i]->name != tu.name )
				i++;
			if ( tu.type == VK_DESCRIPTOR_TYPE_SAMPLER )
				samplers++;
			if ( i == imgs.size() ) {
				absent += tu.unit < 0;
				wrong += tu.unit >= 0;
				continue;
			}
			unsigned target = 0, query = 0;
			ouTarget( imgs[i]->dim, imgs[i]->arrayed, target, query );
			if ( tu.unit == int( i ) && tu.glTex == texOf[target] && tu.glTarget == target )
				found++;
			else
				wrong++;
		}
		int want = 0;  // the declared images and their samplers
		for ( const ObjBlTexUnit & tu : units )
			for ( const SpvBind * b : imgs )
				want += b->name == tu.name;
		const bool exact = made && wrong == 0 && found == want && want >= int( imgs.size() );
		std::printf( "BLVIEW uni %s tex: %zu set-0 bindings (%d samplers), %zu GL samplers declared, %d found, %d wrong, %d absent: %s\n", twins[t],
			units.size(), samplers, imgs.size(), found, wrong, absent, exact ? "EXACT" : "FAIL" );
		ok = ok && exact;
		f.useProgram( 0 );
		u.deleteProgram( prog );
	}
	const char * redName[3] = { "transpose (matrices the other way)", "stride (arrays packed tight)", "binding (the block read one binding on)" };
	for ( int r = 0; r < 3; r++ ) {
		std::printf( "BLVIEW RED %s %s (%d blocks differ)\n", redName[r], redFail[r] ? "FAIL" : "PASSED", redFail[r] );
		ok = ok && redFail[r] > 0;
	}
	return ok;
}

/* Lane VKOBJ4 open item 2: the mirror on a program linked from SPIR-V (no uniform names), through the location registry.
 * Fixture: GL's fo4_cellbl (fo4_defaultbl.vert + fo4_cellbl.frag) linked from the HLSL SPIR-V set build_all.py writes
 * (blview.sh: <spv dir>/spvset, --only fo4_cellbl), the registry its locs.txt. Each LooseUniforms member of the shipped
 * twins (objbl_vs, objbl_cell_ps) is set at the registry's location (element i at + i) by GL's own type there, values by
 * uniValue (a module uint GL holds as int: its low 31 bits); globalUniforms / skinningUniforms on buffers of random bytes
 * at build_spv.py's bindings 0 / 1; each active pixel-stage sampler on unit 30 - its order.
 * Lines: `BLVIEW locs nameless`, `BLVIEW locs loose vertex|pixel`, `BLVIEW locs block <name>`, `BLVIEW locs tex`,
 * `BLVIEW RED locshift|nolocs` (each must FAIL).
 * BAR (stated before the first run, never widened): the program is nameless (GL finds no member by name); per stage 0 bytes
 * differ from the values set at every location GL reports active (elements GL drops read 0), every member with an active
 * element mirrored and through the registry, the rest absent, at least one mirrored; each block 0 bytes differ; every
 * active sampler's unit found, at least one. */
bool locsGate( const std::string & spirvDir )
{
	using namespace WwVkPresent;
	struct LocGl
	{
		void( WINAPI * shaderBinary )( GLsizei, const GLuint *, GLenum, const void *, GLsizei ) = nullptr;
		void( WINAPI * specialize )( GLuint, const char *, GLuint, const GLuint *, const GLuint * ) = nullptr;
		void( WINAPI * ifaceIv )( GLuint, GLenum, GLenum, GLint * ) = nullptr;
		void( WINAPI * resIv )( GLuint, GLenum, GLuint, GLsizei, const GLenum *, GLsizei, GLsizei *, GLint * ) = nullptr;
		GLint( WINAPI * loc )( GLuint, const char * ) = nullptr;
		void( WINAPI * uf[4] )( GLint, GLsizei, const GLfloat * ) = {};
		void( WINAPI * ui[4] )( GLint, GLsizei, const GLint * ) = {};
		void( WINAPI * uu[4] )( GLint, GLsizei, const GLuint * ) = {};
		void( WINAPI * um[3][3] )( GLint, GLsizei, GLboolean, const GLfloat * ) = {};
		void( WINAPI * deleteProgram )( GLuint ) = nullptr;
	} u;
	const char * n4 = "1234";
	bool okLoad = load( u.shaderBinary, "glShaderBinary" ) && load( u.specialize, "glSpecializeShader" ) && load( u.ifaceIv, "glGetProgramInterfaceiv" )
		&& load( u.resIv, "glGetProgramResourceiv" ) && load( u.loc, "glGetUniformLocation" ) && load( u.deleteProgram, "glDeleteProgram" );
	for ( int i = 0; i < 4; i++ )
		okLoad = okLoad && load( u.uf[i], ( std::string( "glUniform" ) + n4[i] + "fv" ).c_str() )
			&& load( u.ui[i], ( std::string( "glUniform" ) + n4[i] + "iv" ).c_str() ) && load( u.uu[i], ( std::string( "glUniform" ) + n4[i] + "uiv" ).c_str() );
	for ( int c = 2; c <= 4; c++ )
		for ( int r = 2; r <= 4; r++ ) {
			const std::string nm = c == r ? "glUniformMatrix" + std::to_string( c ) + "fv" : "glUniformMatrix" + std::to_string( c ) + "x" + std::to_string( r ) + "fv";
			okLoad = okLoad && load( u.um[c - 2][r - 2], nm.c_str() );
		}
	if ( !okLoad ) {
		std::printf( "BLVIEW locs: GL entry points missing (GL_ARB_gl_spirv / program interface query)\n" );
		return false;
	}
	std::string why;
	const int names = objBlLocsLoad( spirvDir + "/locs.txt", why );
	std::map<std::string, std::string> index;
	{
		std::istringstream in( readFile( spirvDir + "/index.txt" ) );
		std::string ln;
		while ( std::getline( in, ln ) ) {
			const std::size_t t = ln.find( '\t' );
			if ( t != std::string::npos )
				index[ln.substr( 0, t )] = ln.substr( t + 1 );
		}
	}
	if ( !names || !index.count( "fo4_defaultbl.vert" ) || !index.count( "fo4_cellbl.frag" ) ) {
		std::printf( "BLVIEW locs: no SPIR-V set at %s (%d registry names; %s)\n", spirvDir.c_str(), names, why.c_str() );
		return false;
	}
	// GL's program from the set, as wwSpvAttach links it
	const GLuint prog = f.createProgram();
	const char * stage[2] = { "fo4_defaultbl.vert", "fo4_cellbl.frag" };
	for ( int s = 0; s < 2; s++ ) {
		const std::string full = readFile( spirvDir + "/" + index[stage[s]] );
		/* the set keeps OpName, and the driver reflects it: the fixture renames each loose uniform (a UniformConstant variable)
		 * `u<its location>`, so GL knows no member by its own name (the program the registry is for). Dropping the names
		 * instead does not link on NV: a uniform both stages hold gets a default name per stage ("Duplicate location") */
		auto word = [&]( std::size_t at ) {
			std::uint32_t w = 0;
			std::memcpy( &w, full.data() + at, 4 );
			return w;
		};
		std::map<std::uint32_t, bool> loose;
		std::map<std::uint32_t, std::uint32_t> locOf;
		for ( std::size_t at = 20; at + 4 <= full.size(); ) {
			const std::uint32_t w0 = word( at );
			const std::size_t len = std::size_t( w0 >> 16 ) * 4;
			if ( !len || at + len > full.size() )
				break;
			if ( ( w0 & 0xFFFFu ) == 59 && len >= 16 && word( at + 12 ) == 0 )  // OpVariable, UniformConstant
				loose[word( at + 8 )] = true;
			if ( ( w0 & 0xFFFFu ) == 71 && len >= 16 && word( at + 8 ) == 30 )  // OpDecorate Location
				locOf[word( at + 4 )] = word( at + 12 );
			at += len;
		}
		std::string bin = full.substr( 0, std::min<std::size_t>( full.size(), 20 ) );
		for ( std::size_t at = 20; at + 4 <= full.size(); ) {
			const std::uint32_t w0 = word( at );
			const std::size_t len = std::size_t( w0 >> 16 ) * 4;
			if ( !len || at + len > full.size() )
				break;
			const std::uint32_t id = len >= 8 ? word( at + 4 ) : 0;
			if ( ( w0 & 0xFFFFu ) != 5 || !loose.count( id ) ) {
				bin.append( full, at, len );
			} else {  // OpName of a loose uniform: `u<location>` (no location: past every location, by its id)
				std::string nm = "u" + std::to_string( locOf.count( id ) ? locOf[id] : 100000u + id );
				nm.resize( ( nm.size() / 4 + 1 ) * 4, '\0' );
				const std::uint32_t head[2] = { std::uint32_t( 2 + nm.size() / 4 ) << 16 | 5u, id };
				bin.append( reinterpret_cast<const char *>( head ), 8 );
				bin.append( nm );
			}
			at += len;
		}
		const GLuint sh = f.createShader( s ? kFrag : kVert );
		u.shaderBinary( 1, &sh, 0x9551, bin.data(), GLsizei( bin.size() ) );  // SHADER_BINARY_FORMAT_SPIR_V
		u.specialize( sh, "main", 0, nullptr, nullptr );
		GLint okS = 0;
		f.getShaderiv( sh, kCompiled, &okS );
		if ( !okS ) {
			std::printf( "BLVIEW locs: %s did not specialize\n", stage[s] );
			return false;
		}
		f.attachShader( prog, sh );
	}
	f.linkProgram( prog );
	GLint linked = 0;
	f.getProgramiv( prog, kLinked, &linked );
	if ( !linked ) {
		char log[1024] = {};
		void( WINAPI * infoLog )( GLuint, GLsizei, GLsizei *, char * ) = nullptr;
		if ( load( infoLog, "glGetProgramInfoLog" ) )
			infoLog( prog, GLsizei( sizeof( log ) - 1 ), nullptr, log );
		std::printf( "BLVIEW locs: fo4_cellbl from SPIR-V did not link (%s)\n", log );
		return false;
	}
	// GL's active default-block locations and their types (the fixture's own read)
	std::map<int, GLint> glType;
	GLint nRes = 0;
	u.ifaceIv( prog, 0x92E1, 0x92F5, &nRes );
	for ( GLint i = 0; i < nRes; i++ ) {
		const GLenum props[3] = { 0x930E, 0x92FB, 0x92FA };  // LOCATION, ARRAY_SIZE, TYPE
		GLint v[3] = { -1, 0, 0 };
		u.resIv( prog, 0x92E1, GLuint( i ), 3, props, 3, nullptr, v );
		for ( GLint e = 0; v[0] >= 0 && e < std::max( v[1], 1 ); e++ )
			glType[v[0] + e] = v[2];
	}
	auto kindOf = []( GLint t ) {  // 0 float / matrix, 1 int / bool, 2 uint, 3 sampler or other
		if ( t == 0x1406 || ( t >= 0x8B50 && t <= 0x8B52 ) || ( t >= 0x8B5A && t <= 0x8B5C ) || ( t >= 0x8B65 && t <= 0x8B6A ) )
			return 0;
		if ( t == 0x1404 || ( t >= 0x8B53 && t <= 0x8B59 ) )
			return 1;
		if ( t == 0x1405 || ( t >= 0x8DC6 && t <= 0x8DC8 ) )
			return 2;
		return 3;
	};
	SpvModule vsM, psM;
	if ( !spvReflect( vkframe_objbl_vs_spv, sizeof( vkframe_objbl_vs_spv ) / 4, vsM, why )
		|| !spvReflect( vkframe_objbl_cell_ps_spv, sizeof( vkframe_objbl_cell_ps_spv ) / 4, psM, why ) ) {
		std::printf( "BLVIEW locs: reflection refused (%s)\n", why.c_str() );
		return false;
	}
	const SpvBind * bl[2] = { uniBlock( vsM, "LooseUniforms" ), uniBlock( psM, "LooseUniforms" ) };
	if ( !bl[0] || !bl[1] ) {
		std::printf( "BLVIEW locs: a LooseUniforms block is missing from the twins\n" );
		return false;
	}
	std::map<std::string, int> reg;
	{
		std::istringstream in( readFile( spirvDir + "/locs.txt" ) );
		std::string ln;
		while ( std::getline( in, ln ) ) {
			const std::size_t t = ln.find( '\t' );
			if ( t != std::string::npos )
				reg[ln.substr( 0, t )] = std::atoi( ln.c_str() + t + 1 );
		}
	}
	// nameless: GL finds no member by name
	int named = 0;
	for ( const SpvBind * b : bl )
		for ( const SpvMember & m : b->members )
			named += u.loc( prog, ( m.count > 1 ? m.name + "[0]" : m.name ).c_str() ) >= 0;
	std::printf( "BLVIEW locs nameless: %d registry names, %d active locations, %d members GL finds by name: %s\n", names, int( glType.size() ), named,
		named ? "FAIL" : "OK" );
	bool ok = named == 0;
	f.useProgram( prog );
	std::vector<std::uint8_t> want[2];
	int wantMirrored[2] = {};
	for ( int s = 0; s < 2; s++ ) {
		want[s].assign( bl[s]->bytes, 0 );
		for ( const SpvMember & m : bl[s]->members ) {
			const auto it = reg.find( m.name );
			bool any = false;
			for ( std::uint32_t e = 0; it != reg.end() && e < m.count; e++ ) {
				const int at = it->second + int( e );
				if ( !glType.count( at ) )
					continue;
				const int kind = kindOf( glType[at] );
				std::uint32_t v[16] = {};
				for ( std::uint32_t c = 0; c < m.cols; c++ )
					for ( std::uint32_t r = 0; r < m.comps; r++ ) {
						v[c * m.comps + r] = uniValue( m, e, c, r );
						if ( m.base == 2 && kind == 1 )
							v[c * m.comps + r] &= 0x7FFFFFFFu;
					}
				if ( m.cols > 1 && kind == 0 )
					u.um[m.cols - 2][m.comps - 2]( at, 1, GL_FALSE, reinterpret_cast<const GLfloat *>( v ) );
				else if ( kind == 0 && m.cols == 1 )
					u.uf[m.comps - 1]( at, 1, reinterpret_cast<const GLfloat *>( v ) );
				else if ( kind == 1 && m.cols == 1 )
					u.ui[m.comps - 1]( at, 1, reinterpret_cast<const GLint *>( v ) );
				else if ( kind == 2 && m.cols == 1 )
					u.uu[m.comps - 1]( at, 1, v );
				else
					continue;
				any = true;
				for ( std::uint32_t c = 0; c < m.cols; c++ )
					for ( std::uint32_t r = 0; r < m.comps; r++ ) {
						const std::uint32_t off = m.offset + e * m.arrayStride
							+ ( m.cols > 1 ? ( m.rowMajor ? r * m.colStride + c * 4 : c * m.colStride + r * 4 ) : r * 4 );
						if ( off + 4 <= want[s].size() )
							std::memcpy( want[s].data() + off, &v[c * m.comps + r], 4 );
					}
			}
			wantMirrored[s] += any;
		}
	}
	// the blocks at build_spv.py's bindings
	std::mt19937 rng( 91 );
	std::vector<std::uint8_t> gBytes( 1024 ), sBytes( 256 * 48 );
	for ( auto & b : gBytes )
		b = std::uint8_t( rng() );
	for ( auto & b : sBytes )
		b = std::uint8_t( rng() );
	GLuint ubo[2] = {};
	f.genBuffers( 2, ubo );
	f.bindBuffer( 0x8A11, ubo[0] );
	f.bufferData( 0x8A11, std::ptrdiff_t( gBytes.size() ), gBytes.data(), 0x88E4 );
	f.bindBuffer( 0x8A11, ubo[1] );
	f.bufferData( 0x8A11, std::ptrdiff_t( sBytes.size() ), sBytes.data(), 0x88E4 );
	f.bindBufferBase( 0x8A11, 0, ubo[0] );
	f.bindBufferBase( 0x8A11, 1, ubo[1] );
	// the active pixel-stage samplers on units 30 - order
	std::map<std::string, int> unitSet;
	for ( const SpvBind & b : psM.binds ) {
		if ( b.set != 0 || ( b.type != VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE && b.type != VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER ) )
			continue;
		const auto it = reg.find( b.name );
		if ( it == reg.end() || !glType.count( it->second ) || kindOf( glType[it->second] ) != 3 )
			continue;
		const GLint unit = 30 - GLint( unitSet.size() );
		u.ui[0]( it->second, 1, &unit );
		unitSet[b.name] = unit;
	}
	// the mirror, then its reds
	auto looseRun = [&]( int s, ObjBlUniStat & st, std::size_t & d ) {
		std::vector<std::uint8_t> got;
		const bool made = objBlLoose( glProc, prog, *bl[s], got, st, why );
		d = made ? uniDiff( got, want[s] ) : 1;
		return made && d == 0 && st.mirrored == wantMirrored[s] && st.registry == st.mirrored && st.mirrored >= 1
			&& st.mirrored + st.absent == st.members;
	};
	for ( int s = 0; s < 2; s++ ) {
		ObjBlUniStat st;
		std::size_t d = 0;
		const bool exact = looseRun( s, st, d );
		std::printf( "BLVIEW locs loose %s: %d members, %d mirrored (%d through the registry, %d with an active location), %d absent, %u bytes, "
			"%zu differ: %s\n", s ? "pixel" : "vertex", st.members, st.mirrored, st.registry, wantMirrored[s], st.absent, bl[s]->bytes, d,
			exact ? "EXACT" : "FAIL" );
		ok = ok && exact;
	}
	const SpvBind * blocks[2] = { uniBlock( psM, "globalUniforms" ), uniBlock( vsM, "skinningUniforms" ) };
	const std::vector<std::uint8_t> * src[2] = { &gBytes, &sBytes };
	for ( int k = 0; k < 2; k++ ) {
		if ( !blocks[k] ) {
			std::printf( "BLVIEW locs block %d: missing from the twins: FAIL\n", k );
			ok = false;
			continue;
		}
		SpvBind named = *blocks[k];
		if ( named.name.compare( 0, 5, "type." ) == 0 )
			named.name = named.name.substr( 5 );
		std::vector<std::uint8_t> got;
		const bool made = objBlBlock( glProc, prog, named, got, why );
		const std::vector<std::uint8_t> w( src[k]->begin(), src[k]->begin() + std::ptrdiff_t( std::min<std::size_t>( src[k]->size(), named.bytes ) ) );
		const std::size_t d = made ? uniDiff( got, w ) : 1;
		std::printf( "BLVIEW locs block %s (%u bytes): %zu differ: %s%s\n", named.name.c_str(), named.bytes, d, made && !d ? "EXACT" : "FAIL",
			made ? "" : ( " (" + why + ")" ).c_str() );
		ok = ok && made && !d;
	}
	std::vector<ObjBlTexUnit> units;
	const bool madeT = objBlTextures( glProc, prog, psM, units, why );
	int found = 0, wrong = 0;
	for ( const ObjBlTexUnit & tu : units ) {
		if ( tu.type == VK_DESCRIPTOR_TYPE_SAMPLER )
			continue;
		const auto it = unitSet.find( tu.name );
		if ( it == unitSet.end() )
			wrong += tu.unit >= 0;
		else
			( tu.unit == it->second ? found : wrong )++;
	}
	const bool texOk = madeT && wrong == 0 && found == int( unitSet.size() ) && found >= 1;
	std::printf( "BLVIEW locs tex: %zu active samplers set, %d found, %d wrong: %s\n", unitSet.size(), found, wrong, texOk ? "EXACT" : "FAIL" );
	ok = ok && texOk;
	// reds: the registry one location on; no registry (the mirror before it)
	int redFail[2] = {};
	for ( int red = 0; red < 2; red++ ) {
		if ( red == 0 )
			objBlLocsLoad( spirvDir + "/locs.txt", why, 1 );
		else
			objBlLocsLoad( spirvDir + "/no_locs.txt", why );
		for ( int s = 0; s < 2; s++ ) {
			ObjBlUniStat st;
			std::size_t d = 0;
			redFail[red] += !looseRun( s, st, d );
		}
	}
	const char * redName[2] = { "locshift (every registry location one on)", "nolocs (no registry: the names only)" };
	for ( int r = 0; r < 2; r++ ) {
		std::printf( "BLVIEW RED %s %s (%d stages differ)\n", redName[r], redFail[r] ? "FAIL" : "PASSED", redFail[r] );
		ok = ok && redFail[r] > 0;
	}
	objBlLocsLoad( spirvDir + "/no_locs.txt", why );  // the GLSL gates after this one never see it
	f.useProgram( 0 );
	u.deleteProgram( prog );
	return ok;
}
