// lane SKINSSS2 (EYECORNEA-CELL): src/wwfacegeneye.h against the census of the vanilla facegen heads (1094 FaceGeom
// NIFs, Fallout4 - Meshes.ba2: every opaque Shader Type 1 shape, by its slot-4 cube), plus pins of the lodgen, cell
// writer and renderer text that carry the eye type to the cornea gate. usage: eyecornea <tree root>; rc 0 = all pass.
// Built with -DWWFACEGENEYE_RED (the old rule: the eye keeps type 1) it must fail (tests/eyecornea/eyecornea.sh).
#include "wwfacegeneye.h"

#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>

static int fails = 0;
static void check( bool ok, const char * what, double v = 0.0 )
{
	std::printf( "%s %s %.6g\n", ok ? "ok  " : "FAIL", what, v );
	if ( !ok )
		fails++;
}

static std::string readAll( const std::string & p )
{
	std::ifstream f( p, std::ios::binary );
	std::stringstream s;
	s << f.rdbuf();
	return s.str();
}

// notes/skinsss2/tmp/facegen_eye_tex.tsv: opaque Shader Type 1 shapes of FaceGeom, no BGSM, by (shape class, slot 4)
struct Row
{
	const char * shape;
	const char * cube;
	int count;
	bool eye;
};
static const Row census[] = {
	{ "*EyesHuman* / *EyesGhoul*", "shared/cubemaps/eyecubemap.dds", 1046, true },
	{ "SupermutantEyes", "shared/cubemaps/mipblur_eye1.dds", 13, true },
	{ "SynthGen2HeadRear", "shared/cubemaps/mipblur_defaultoutside1_dielectric.dds", 18, false },
	{ "SynthGen2PartsValentine", "shared/cubemaps/mipblur_defaultoutside1.dds", 1, false },
};

int main( int argc, char ** argv )
{
	std::setvbuf( stdout, nullptr, _IONBF, 0 );
	const std::string root = argc > 1 ? argv[1] : ".";

	int eyes = 0, wrong = 0, total = 0;
	for ( const Row & r : census ) {
		const bool tagged = wwFacegenSrcType( 1u, false, r.cube ) == 16u;
		total += r.count;
		eyes += tagged ? r.count : 0;
		wrong += ( tagged != r.eye ) ? r.count : 0;
	}
	check( total == 1078, "census: opaque type-1 facegen shapes", total );
	check( eyes == 1059, "census: eyes tagged 16 (1046 + 13 supermutant)", eyes );
	check( wrong == 0, "census: shapes classified wrong (synth rears stay 1)", wrong );

	// the rule's edges
	check( wwFacegenSrcType( 1u, false, "Shared\\Cubemaps\\EyeCubeMap.dds" ) == 16u, "backslash + case", 0 );
	check( wwFacegenSrcType( 1u, false, "eyecubemap.dds" ) == 16u, "bare name", 0 );
	check( wwFacegenSrcType( 1u, true, "shared/cubemaps/eyecubemap.dds" ) == 1u, "a BGSM wins (its own eye flag)", 0 );
	check( wwFacegenSrcType( 4u, false, "shared/cubemaps/eyecubemap.dds" ) == 4u, "face type kept", 0 );
	check( wwFacegenSrcType( 0u, false, "shared/cubemaps/eyecubemap.dds" ) == 0u, "type 0 kept", 0 );
	check( wwFacegenSrcType( 1u, false, "" ) == 1u, "empty slot", 0 );
	check( wwFacegenSrcType( 1u, false, nullptr ) == 1u, "no slot", 0 );
	check( wwFacegenSrcType( 1u, false, "shared/cubemaps/myeyecubemap.dds" ) == 1u, "name must START with the word", 0 );
	check( wwFacegenSrcType( 1u, false, "textures/eyecubemap/other.dds" ) == 1u, "a folder name is not the file", 0 );

	// pins: the type reaches the shader and the gate
	const std::string lg = readAll( root + "/src/lodgen.cpp" );
	const std::string cv = readAll( root + "/src/cellview.cpp" );
	const std::string rd = readAll( root + "/src/gl/renderer.cpp" );
	const std::string gp = readAll( root + "/src/gl/glproperty.cpp" );
	check( lg.find( "#include \"wwfacegeneye.h\"" ) != std::string::npos, "pin: lodgen includes the rule" );
	check( lg.find( "s.srcShaderType = wwFacegenSrcType( 1u," ) != std::string::npos, "pin: lodgen applies it" );
	check( lg.find( "WW_CELL_EYEST_RED" ) != std::string::npos, "pin: lodgen red key" );
	check( cv.find( "|| s.srcShaderType == 16u ) ? s.srcShaderType : 0u;" ) != std::string::npos,
		"pin: cell bucket keeps type 16" );
	check( cv.find( "\"WWSrcShaderType\"" ) != std::string::npos, "pin: cell writer tags the shader" );
	check( gp.find( "\"WWSrcShaderType\"" ) != std::string::npos, "pin: property reads the tag" );
	check( rd.find( "|| lsp->wwSrcShaderType == int( ShaderFlags::ST_EyeEnvmap );" ) != std::string::npos,
		"pin: eye gate reads the carried type" );

	std::printf( "%s fails=%d\n", fails ? "EYECORNEA FAIL" : "EYECORNEA PASS", fails );
	return fails ? 1 : 0;
}
