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
