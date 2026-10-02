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

/*! THE BAKE'S ALBEDO (lane PRTPBAKE, 2026-09-30). The surfels store LINEAR albedo
 *  (FO4CS reads its G-buffer albedo through an _SRGB view, so its surfels are linear
 *  too). A diffuse map is read at a coarse mip only (at most 64 texels across): a
 *  surfel is a 70-unit cell's mean, and decoding every map of a block at full size
 *  would cost gigabytes. The game multiplies the vertex color into the map's
 *  gamma-space value, so the product is decoded, not the factors. */

#include <QHash>
#include <QString>

class DDSTexture16;

class ProbeAlbedo
{
public:
	explicit ProbeAlbedo( const QString & dataRoot );
	~ProbeAlbedo();
	ProbeAlbedo( const ProbeAlbedo & ) = delete;
	ProbeAlbedo & operator=( const ProbeAlbedo & ) = delete;

	//! The map at (u, v), times `vc` (0..1, gamma), as linear 0..1. False = no map read.
	//! lane BAKE4: `alpha` (optional) takes the map's alpha there (glass coverage).
	bool sample( const QString & tex, float u, float v, const float vc[3], float out[3], float * alpha = nullptr );
	/*! A Greyscale_To_PaletteColor surface, as the game paints it: the map's green at (u, v)
	 *  picks the column and `row` (the palette scale x vertex red, or a CNAM/MODC index) the
	 *  row of `palette`; the palette color replaces the albedo (no vertex color on top). Linear
	 *  0..1. False = either map unread. */
	bool samplePalette( const QString & tex, const QString & palette, float u, float v, float row, float out[3] );
	//! The map's mean, GAMMA space (its coarsest mip). False = no map read.
	bool meanGamma( const QString & tex, float out[3] );

	static float srgbToLinear( float c );
	static float linearToSrgb( float c );

	int texturesRead = 0, texturesMissing = 0;

private:
	const DDSTexture16 * load( const QString & tex );
	QString root;
	QHash<QString, DDSTexture16 *> cache;
};

#endif // PROBEALBEDO_H
