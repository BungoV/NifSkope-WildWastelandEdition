#ifndef WWLEAFCOV_H
#define WWLEAFCOV_H

/* lane TREELEAF2 (FIX1030B round 2): every pass that alpha-tests a leaf's base map tests the same texel set.
 *
 * TREELEAF1 gave the colour pass (Renderer::setupProgramCE1) a coverage-preserving copy of an alpha-tested base map
 * (the TexCache entry "<file>|wwcov<ref>", src/gl/wwalphacov.h). The side passes that alpha-test the same map -- the sun
 * cascades (sunshadow.cpp, per draw and queued), the cell-light shadow cubes (celllights.cpp), the TAA inputs
 * (taainputs.cpp) and the reflection-probe capture (cellreflprobe.cpp) -- still bound the plain file, so at a distance
 * a leaf's shadow and its motion mask cut a different silhouette from the leaf itself (the plain mips drift: down to
 * 0.68 of level 0's coverage at ref 185, up to 3 x at ref 35, TREELEAF1's census). And the side shaders discard
 * `a < threshold` (a texel AT the ref passes) where the colour pass with a material keeps only `a > ref` (alphaFlags 4).
 *
 * This header is the one rule, as plain values (no Qt, no GL), so the shapes (glshape.cpp) and the unit test
 * (tests/treeleaf2) run the same code:
 *  - colourRef(): the ref the colour pass arms its base-map bind with (0 = the plain file), moved here unchanged from
 *    setupProgramCE1, plus the WW_LODL_TREE_ALPHA dial (TREELEAF5: the colour pass tests at the dial, so its copy is
 *    built at the dial too);
 *  - sideThreshold(): the threshold a side pass uploads so that its `discard a < t` keeps exactly the colour pass's
 *    `a > ref / 255` texels (the next float above ref / 255) -- for a lighting shape whose colour test is strict;
 *  - passRed(): WW_TREELEAF_RED. "1" every pass binds the plain file (TREELEAF1's red); "side" or a comma list of
 *    sun,cube,taa,refl puts those side passes back to today's draw (the plain file and the old threshold).
 *
 * lane FIX1031B (bungo's sheet_3, 10-31: the Concord church's gothic window tracery vanished with the copy, the red drew
 * it): the copy keeps level 0's coverage of `tex.a > ref`, but the colour pass tests `C.a x tex.a x alpha > ref`
 * (fo4_default.frag). A shape whose vertex alpha is under 1 (and not a tree: NifSkope sets C.a = 1 on Tree_Anim shapes)
 * or whose material alpha is under 1 tests the texture at a HIGHER threshold, which the copy does not preserve: the
 * copy lowers the alpha of whole levels (x 0.3 at the small levels of ChurchSidingWindow01, ref 10), and
 * bldchurch01open.nif's window quad (vertex alpha 25 / 255: the game's "glass gone" trick, the panes at tex.a 80 fail,
 * the frame at 255 passes) then fails everywhere. Such a shape binds the plain file (In::modulated, colourRef 0). Census
 * (notes/fix1031b/leaves/census/modcensus.txt, vanilla Meshes + MeshesExtra + BNS): 168906 of 169251 alpha-tested foliage
 * shapes keep the copy (the 345 that lose it are grass objects whose vertex alpha fades to 0); 5414 of 202328 others
 * lose it. Red WW_TREELEAF_RED=modulate: the copy on those shapes too (TREELEAF1-3 as they shipped). */

#include <cmath>
#include <cstring>

namespace WwLeafCov
{

//! what the colour pass reads of one shape's alpha test (setupProgramCE1 + AlphaProperty::glProperty)
struct In
{
	bool lighting = false;	//!< a BSLightingShaderProperty draws it (the colour pass arms only that branch's base map)
	bool blendingOn = true;	//!< Scene::DoBlending (off: the colour pass runs no test)
	bool land = false;	//!< a blended-ground pass (its own alpha property, never the material's)
	bool hasMat = false;	//!< the shader property names a readable material file (it decides the test)
	bool matTest = false, matBlend = false;
	int matRef = 255;	//!< the material's iAlphaTestRef
	int treeDial = 0;	//!< WW_LODL_TREE_ALPHA (1..255) when the material's base map sits under trees/, else 0
	bool translucent = false;	//!< Shape::translucent (NIF path)
	bool nifTest = false, nifBlend = false;
	int nifFunc = -1;	//!< NiAlphaProperty test: 4 >, 6 >=
	float nifThreshold = 0.0f;	//!< NiAlphaProperty threshold (0..1)
	bool modulated = false;	//!< FIX1031B: the colour test multiplies tex.a by a factor under 1 (vertexAlphaModulates)
};

/*! FIX1031B: does the colour pass multiply this shape's texture alpha by something under 1? Mirrors renderer.cpp's
 *  vertexColorOverride (setupProgramCE1): the vertex alpha reaches C.a when the shape draws its vertex colours
 *  (`drawsColours`), the Vertex Alpha option is on, the shape is not a tree (`treeAnim`: C.a = 1), and before BSVER 130
 *  its SLSF1 Vertex_Alpha flag is set (`pre130NoAlphaFlag`: C.a = 1); `minVertexAlpha` is the lowest over its vertices
 *  (0..1). `materialAlpha` is the shader's alpha uniform (BGSM / NIF alpha). */
inline bool vertexAlphaModulates( bool drawsColours, bool vertexAlphaOn, bool treeAnim, bool pre130NoAlphaFlag,
	float minVertexAlpha, float materialAlpha )
{
	if ( materialAlpha < 1.0f )
		return true;
	if ( !drawsColours || !vertexAlphaOn || treeAnim || pre130NoAlphaFlag )
		return false;
	return minVertexAlpha < 1.0f;
}

//! the colour pass's base-map copy: alpha > ref with ref in 1..254, else 0 (the plain file)
inline int colourRef( const In & s )
{
	if ( !s.lighting || !s.blendingOn || s.land )
		return 0;
#ifndef WWLEAFCOV_RED_MODULATE	// the unit's red: TREELEAF1-3's rule, the copy whatever multiplies the texture alpha
	if ( s.modulated )
		return 0;	// FIX1031B: the test is not tex.a > ref, so the copy built at ref does not fit it
#endif
	int r = 0;
	if ( s.hasMat ) {
		if ( s.matTest && !s.matBlend )
			r = ( s.treeDial >= 1 && s.treeDial <= 255 ) ? s.treeDial : s.matRef;
	} else if ( !s.translucent && s.nifTest && !s.nifBlend ) {
		const int n = int( std::lround( s.nifThreshold * 255.0f ) );
		if ( s.nifFunc == 4 )	// alpha > n
			r = n;
		else if ( s.nifFunc == 6 )	// alpha >= n is alpha > n - 1
			r = n - 1;
	}
	return ( r > 0 && r < 255 ) ? r : 0;
}

/*! The threshold a side pass uploads for its `discard a < t` test, given the one it uploads today (`today`, from
 *  Shape::wwDrawAlpha: the material's ref / 255, or the NIF's threshold) and whether the material decided it. A lighting
 *  shape the colour pass tests strictly (a material: alphaFlags 4; a NIF: func 4) gets the next float above the colour
 *  pass's threshold, so the two keep the same texels; anything else keeps `today`. */
inline float sideThreshold( const In & s, bool fromMaterial, float today )
{
	if ( !s.lighting || s.land )
		return today;
	if ( fromMaterial ) {
		const int r = ( s.treeDial >= 1 && s.treeDial <= 255 ) ? s.treeDial : s.matRef;
		return std::nextafter( float( r ) / 255.0f, 2.0f );
	}
	if ( s.nifFunc == 4 )
		return std::nextafter( today, 2.0f );
	return today;
}

//! is `word` one of the comma-separated items of `spec`?
inline bool listHas( const char * spec, const char * word )
{
	const size_t n = std::strlen( word );
	for ( const char * p = spec; *p; ) {
		const char * e = std::strchr( p, ',' );
		const size_t len = e ? size_t( e - p ) : std::strlen( p );
		if ( len == n && std::strncmp( p, word, n ) == 0 )
			return true;
		if ( !e )
			break;
		p = e + 1;
	}
	return false;
}

//! WW_TREELEAF_RED for one pass ("colour", "sun", "cube", "taa", "refl"): true = that pass draws as before TREELEAF2/1
inline bool passRed( const char * spec, const char * pass )
{
	if ( !spec || !*spec )
		return false;
	if ( std::strcmp( spec, "1" ) == 0 )
		return true;	// TREELEAF1's red: the plain file everywhere
	if ( std::strcmp( pass, "colour" ) == 0 )
		return false;
	if ( std::strcmp( spec, "side" ) == 0 )
		return true;
	return listHas( spec, pass );
}

//! FIX1031B: WW_TREELEAF_RED holds "modulate" (alone or in its comma list): the copy also on shapes In::modulated names
inline bool modulateRed( const char * spec )
{
	return spec && listHas( spec, "modulate" );
}

}	// namespace WwLeafCov

#endif // WWLEAFCOV_H
