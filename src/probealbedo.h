/***** BEGIN LICENSE BLOCK *****

BSD License

Copyright (c) 2005-2015, NIF File Format Library and Tools
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:
1. Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.
3. The name of the NIF File Format Library and Tools project may not be
   used to endorse or promote products derived from this software
   without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

***** END LICENSE BLOCK *****/

#ifndef PROBEALBEDO_H
#define PROBEALBEDO_H

/*! THE BAKE'S ALBEDO. The surfels store LINEAR albedo
 *  (FO4CS reads its G-buffer albedo through an _SRGB view, so its surfels are linear
 *  too). A diffuse map is read at a coarse mip only (at most 64 texels across): a
 *  surfel is a 70-unit cell's mean, and decoding every map of a block at full size
 *  would cost gigabytes. The game multiplies the vertex color into the map's
 *  gamma-space value, so the product is decoded, not the factors. */

#include <QHash>
#include <QString>

#include <vector>

class DDSTexture16;
struct PbrmMaterial;

class ProbeAlbedo
{
public:
	explicit ProbeAlbedo( const QString & dataRoot );
	~ProbeAlbedo();
	ProbeAlbedo( const ProbeAlbedo & ) = delete;
	ProbeAlbedo & operator=( const ProbeAlbedo & ) = delete;

	//! The map at (u, v), times `vc` (0..1, gamma), as linear 0..1. False = no map read.
	//! `alpha` (optional) takes the map's alpha there (glass coverage).
	bool sample( const QString & tex, float u, float v, const float vc[3], float out[3], float * alpha = nullptr );
	//! A Greyscale_To_PaletteColor surface, as the game paints it: the map's green at (u, v) picks the column
	//! and `row` (the palette scale x vertex red, or a CNAM/MODC index) the row of `palette`; the palette color
	//! replaces the albedo (no vertex color on top).
	bool samplePalette( const QString & tex, const QString & palette, float u, float v, float row, float out[3] );
	/*! a surface a .pbrm serves, the diffuse albedo pbrm_default.frag shades
	 *  with: base = the base map (sRGB-decoded as the shader decodes it, the same 64-texel cap) x the base
	 *  colour, else the base colour; x the vertex colour (the shader's base *= C.rgb); metallic = RMAOS
	 *  green where the material samples it, else the constant; out = base x (1 - metallic), linear 0..1.
	 *  Specular is not the albedo's (out of scope); the tint mask is not modelled. False = a map the
	 *  material samples did not read: the renderer then aborts the PBR binding and draws the shape the
	 *  legacy way, so the caller reads the legacy diffuse. */
	bool samplePbrm( const PbrmMaterial & m, float u, float v, const float vc[3], float out[3] );
	//! The map's mean, GAMMA space (its coarsest mip). False = no map read.
	bool meanGamma( const QString & tex, float out[3] );

	static float srgbToLinear( float c );
	static float linearToSrgb( float c );

	/*! the map at up to 512 texels across, for the hit and cube albedo ways (they
	 *  choose a mip by the sample's footprint). Null = no map read. */
	const DDSTexture16 * loadFine( const QString & tex );
	/*! thread-safe (reads loaded maps only): `t` at (u, v), mip `lod` (trilinear,
	 *  clamped to the map's), times `vc` (gamma), as linear 0..1; with `pal`, the palette's color at
	 *  (the map's green, `row`) instead, as samplePalette. */
	static void sampleLod( const DDSTexture16 * t, const DDSTexture16 * pal, float u, float v, float lod, float row,
		const float vc[3], float out[3] );
	/*! a tangent-space normal map at (u, v) and mip `lod`, decoded as the cell view's shader
	 *  decodes it (x, y = rg * 2 - 1, z = sqrt(1 - x^2 - y^2)); the raw texel, no sRGB curve. */
	static void sampleNormal( const DDSTexture16 * t, float u, float v, float lod, float out[3] );
	//! the map's mip-0 size (texels), for the footprint's mip choice
	static void sizeOf( const DDSTexture16 * t, int * w, int * h );
	/*! the map's alpha as bytes (0..255, row 0 = v 0) at its largest mip of at most
	 *  1024 texels across, decoded once and dropped (the bake's alpha-test masks). False = no map read.
	 *  `minA` takes the smallest alpha (a map whose every texel passes the test needs no mask). */
	bool alphaBytes( const QString & tex, int * w, int * h, std::vector<unsigned char> * a, int * minA );
	/*! the map's RGB as stored (0..255, not sRGB-decoded: the glow map the shader adds in
	 *  sqrt-of-linear space), 3 bytes a texel, row 0 = v 0, at its largest mip of at most 1024 texels across.
	 *  False = no map read. */
	bool rgbBytes( const QString & tex, int * w, int * h, std::vector<unsigned char> * rgb, int cap = 1024 );   // cap

	int texturesRead = 0, texturesMissing = 0;
	int finesRead = 0;

private:
	const DDSTexture16 * load( const QString & tex, int cap = 64 );
	DDSTexture16 * decode( const QString & tex, int cap );
	QString root;
	QHash<QString, DDSTexture16 *> cache;
	QHash<QString, DDSTexture16 *> fine;
};

struct NativeSrcShape;
struct ProbeSoup;
class Vector3;
class Matrix;

//! GLASS FROM A REAL CELL.
int probeGlassFeed( ProbeSoup & soup, ProbeAlbedo & alb, const NativeSrcShape & s, const Vector3 & pos,
	const Matrix & rot, float scale, quint32 ref, const QString & model, QString * census );

#endif // PROBEALBEDO_H
