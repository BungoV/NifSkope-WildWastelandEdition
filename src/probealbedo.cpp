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

#include "probealbedo.h"

#include "io/material.h"
#include "lodgen.h"

#include "ddstxt16.hpp"

#include <algorithm>
#include <cmath>

ProbeAlbedo::ProbeAlbedo( const QString & dataRoot ) : root( dataRoot )
{
}

ProbeAlbedo::~ProbeAlbedo()
{
	qDeleteAll( cache );
}

float ProbeAlbedo::srgbToLinear( float c )
{
	c = std::clamp( c, 0.0f, 1.0f );
	return c <= 0.04045f ? c / 12.92f : std::pow( ( c + 0.055f ) / 1.055f, 2.4f );
}

float ProbeAlbedo::linearToSrgb( float c )
{
	c = std::clamp( c, 0.0f, 1.0f );
	return c <= 0.0031308f ? c * 12.92f : 1.055f * std::pow( c, 1.0f / 2.4f ) - 0.055f;
}

const DDSTexture16 * ProbeAlbedo::load( const QString & texIn )
{
	const QString key = texIn.toLower();
	const auto it = cache.constFind( key );
	if ( it != cache.constEnd() )
		return *it;
	QString path = texIn;
	path.replace( QChar( '\\' ), QChar( '/' ) );
	if ( path.endsWith( QLatin1String( ".bgsm" ), Qt::CaseInsensitive ) ) {
		// a material-backed landscape texture: its diffuse slot
		const int pmi = path.lastIndexOf( QLatin1String( "materials/" ), -1, Qt::CaseInsensitive );
		if ( pmi > 0 )
			path.remove( 0, pmi );
		QByteArray mb;
		const bool have = lodgenProbeAsset( root, path, nullptr, nullptr, nullptr, &mb );
		path.clear();
		if ( have ) {
			const ShaderMaterial sm( mb );
			if ( sm.isValid() && !sm.textures().isEmpty() )
				path = sm.textures().first();
		}
		path.replace( QChar( '\\' ), QChar( '/' ) );
	}
	if ( !path.isEmpty() && !path.startsWith( QLatin1String( "textures/" ), Qt::CaseInsensitive ) )
		path.prepend( QLatin1String( "textures/" ) );
	DDSTexture16 * tex = nullptr;
	QByteArray dds;
	if ( !path.isEmpty() && lodgenProbeAsset( root, path, nullptr, nullptr, nullptr, &dds ) ) {
		quint32 dxgi = 0, w = 0, h = 0, mips = 0;
		int off = 0;
		if ( lodgenTextureInfo( root, path, &dxgi, &w, &h, &mips ) && mips > 1 )
			while ( off + 1 < int( mips ) && ( std::max( w, h ) >> ( off + 1 ) ) >= 64 )
				off++;
		try {
			tex = new DDSTexture16( reinterpret_cast<const unsigned char *>( dds.constData() ), size_t( dds.size() ), off );
		} catch ( std::exception & ) {
			tex = nullptr;
		}
	}
	( tex ? texturesRead : texturesMissing )++;
	cache.insert( key, tex );
	return tex;
}

bool ProbeAlbedo::sample( const QString & tex, float u, float v, const float vc[3], float out[3] )
{
	const DDSTexture16 * t = tex.isEmpty() ? nullptr : load( tex );
	if ( !t )
		return false;
	u -= std::floor( u );
	v -= std::floor( v );
	const FloatVector4 c = t->getPixelB( u, v, 0 );
	for ( int k = 0; k < 3; k++ ) {
		const float g = t->isSRGBTexture() ? linearToSrgb( c[size_t( k )] ) : c[size_t( k )];
		out[k] = srgbToLinear( g * vc[k] );
	}
	return true;
}

bool ProbeAlbedo::meanGamma( const QString & tex, float out[3] )
{
	const DDSTexture16 * t = tex.isEmpty() ? nullptr : load( tex );
	if ( !t )
		return false;
	const FloatVector4 c = t->getPixelB( 0.5f, 0.5f, t->getMaxMipLevel() );
	for ( int k = 0; k < 3; k++ )
		out[k] = t->isSRGBTexture() ? linearToSrgb( c[size_t( k )] ) : std::clamp( c[size_t( k )], 0.0f, 1.0f );
	return true;
}
