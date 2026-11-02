#pragma once
// lane SKINSSS2 (EYECORNEA-CELL): which source shape is a facegen EYE. Vanilla FaceGeom heads (1094 NIFs) name no
// BGSM and give their eyes Shader Type 1 (EnvironmentMap), never 16, and no Eye_Environment_Mapping bit; the synth
// head rear has the same type and flags. The eye cube in texture slot 4 (eyecubemap.dds, the supermutants'
// mipblur_eye1.dds) tells them apart. Pure C, no Qt: tests/eyecornea checks it against the census.
#include <cctype>
#include <cstring>

//! The texture names an eye cube: its file name (any folder, any case) starts with "eyecubemap" or "mipblur_eye".
inline bool wwFacegenEyeCube( const char * path )
{
	if ( !path )
		return false;
	const char *	b = path;
	for ( const char * p = path; *p; p++ )
		if ( *p == '/' || *p == '\\' )
			b = p + 1;
	auto	starts = [b]( const char * w ) {
		for ( size_t i = 0; w[i]; i++ )
			if ( !b[i] || std::tolower( (unsigned char) b[i] ) != w[i] )
				return false;
		return true;
	};
	return starts( "eyecubemap" ) || starts( "mipblur_eye" );
}

//! The Shader Type the cell view carries for a source shape: 16 (EyeEnvmap) for an eye without a material, else its own.
inline unsigned wwFacegenSrcType( unsigned shaderType, bool hasMaterial, const char * envCube )
{
#ifdef WWFACEGENEYE_RED
	(void) hasMaterial; (void) envCube;
	return shaderType;
#else
	if ( shaderType == 1u && !hasMaterial && wwFacegenEyeCube( envCube ) )
		return 16u;
	return shaderType;
#endif
}
