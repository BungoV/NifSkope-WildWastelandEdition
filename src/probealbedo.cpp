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

#include "data/niftypes.h"
#include "io/material.h"
#include "lodgen.h"
#include "nativeemit.h"
#include "probeplace.h"

#include "ddstxt16.hpp"

#include <QByteArray>
#include <QStringList>

#include <algorithm>
#include <cmath>

ProbeAlbedo::ProbeAlbedo( const QString & dataRoot ) : root( dataRoot )
{
}

ProbeAlbedo::~ProbeAlbedo()
{
	qDeleteAll( cache );
	qDeleteAll( fine );
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

const DDSTexture16 * ProbeAlbedo::load( const QString & texIn, int cap )
{
	QHash<QString, DDSTexture16 *> & cache = cap > 64 ? fine : this->cache;   // lane CAPTURE1: two sizes
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
			while ( off + 1 < int( mips ) && ( std::max( w, h ) >> ( off + 1 ) ) >= quint32( cap ) )
				off++;
		try {
			tex = new DDSTexture16( reinterpret_cast<const unsigned char *>( dds.constData() ), size_t( dds.size() ), off );
		} catch ( std::exception & ) {
			tex = nullptr;
		}
	}
	if ( cap > 64 )
		finesRead += tex ? 1 : 0;
	else
		( tex ? texturesRead : texturesMissing )++;
	cache.insert( key, tex );
	return tex;
}

const DDSTexture16 * ProbeAlbedo::loadFine( const QString & tex )
{
	return tex.isEmpty() ? nullptr : load( tex, 512 );
}

void ProbeAlbedo::sizeOf( const DDSTexture16 * t, int * w, int * h )
{
	*w = t ? t->getWidth() : 1;
	*h = t ? t->getHeight() : 1;
}

void ProbeAlbedo::sampleLod( const DDSTexture16 * t, const DDSTexture16 * pal, float u, float v, float lod, float row,
	const float vc[3], float out[3] )
{
	u -= std::floor( u );
	v -= std::floor( v );
	lod = std::clamp( lod, 0.0f, float( t->getMaxMipLevel() ) );
	const FloatVector4 c = t->getPixelT( u, v, lod );
	if ( pal ) {
		const float g = std::clamp( t->isSRGBTexture() ? linearToSrgb( c[1] ) : c[1], 0.0f, 1.0f );
		const FloatVector4 p = pal->getPixelB( g, std::clamp( row, 0.0f, 1.0f ), 0 );
		for ( int k = 0; k < 3; k++ )
			out[k] = pal->isSRGBTexture() ? std::clamp( p[size_t( k )], 0.0f, 1.0f ) : srgbToLinear( p[size_t( k )] );
		return;
	}
	for ( int k = 0; k < 3; k++ ) {
		const float g = t->isSRGBTexture() ? linearToSrgb( c[size_t( k )] ) : c[size_t( k )];
		out[k] = srgbToLinear( g * vc[k] );
	}
}

void ProbeAlbedo::sampleNormal( const DDSTexture16 * t, float u, float v, float lod, float out[3] )
{
	u -= std::floor( u );
	v -= std::floor( v );
	lod = std::clamp( lod, 0.0f, float( t->getMaxMipLevel() ) );
	const FloatVector4 c = t->getPixelT( u, v, lod );
	for ( int k = 0; k < 2; k++ )
		out[k] = std::clamp( ( t->isSRGBTexture() ? linearToSrgb( c[size_t( k )] ) : c[size_t( k )] ) * 2.0f - 1.0f, -1.0f, 1.0f );
	out[2] = std::sqrt( std::max( 0.0f, 1.0f - out[0] * out[0] - out[1] * out[1] ) );
}

bool ProbeAlbedo::sample( const QString & tex, float u, float v, const float vc[3], float out[3], float * alpha )
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
	if ( alpha )   // lane BAKE4: alpha is linear in every format
		*alpha = std::clamp( c[3], 0.0f, 1.0f );
	return true;
}

bool ProbeAlbedo::samplePalette( const QString & tex, const QString & palette, float u, float v, float row,
	float out[3] )
{
	const DDSTexture16 * t = tex.isEmpty() ? nullptr : load( tex );
	const DDSTexture16 * p = palette.isEmpty() ? nullptr : load( palette );
	if ( !t || !p )
		return false;
	u -= std::floor( u );
	v -= std::floor( v );
	const float g0 = t->getPixelB( u, v, 0 )[1];
	const float g = std::clamp( t->isSRGBTexture() ? linearToSrgb( g0 ) : g0, 0.0f, 1.0f );
	const FloatVector4 c = p->getPixelB( g, std::clamp( row, 0.0f, 1.0f ), 0 );
	for ( int k = 0; k < 3; k++ )
		out[k] = p->isSRGBTexture() ? std::clamp( c[size_t( k )], 0.0f, 1.0f ) : srgbToLinear( c[size_t( k )] );
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

// lane BAKE4: a real cell's glass into the soup (the rule and the formula are in the header)
int probeGlassFeed( ProbeSoup & soup, ProbeAlbedo & alb, const NativeSrcShape & s, const Vector3 & pos,
	const Matrix & rot, float scale, quint32 ref, const QString & model, QString * census )
{
	static const QByteArray red = qgetenv( "WW_CELL_PROBE_GLASS_RED" );
	if ( !s.bakeBlend && !s.nearFacts.alphaBlend && !s.nearFacts.effectShader )
		return 0;
	bool pane = s.bakePane();
	if ( red == "haze" )
		pane = s.bakeBlend && !s.nearFacts.decal && s.matAlpha > 0.0f;
	else if ( red == "ignored" )
		pane = false;
	const size_t nv = s.geom.pos.size() / 3;
	int fed = 0;
	double sum[3] = { 0.0, 0.0, 0.0 };
	if ( pane ) {
		const QString & tex = s.nearFacts.effectShader ? s.effectTex0 : s.tex0;
		for ( size_t t = 0; t + 2 < s.geom.tris.size(); t += 3 ) {
			float w[3][3], uv[2] = { 0.5f, 0.5f }, vc[4] = { 1, 1, 1, 1 }, lin[3] = { 1, 1, 1 }, ma = 1.0f;
			size_t vi[3];
			bool okTri = true;
			for ( int k = 0; k < 3; k++ ) {
				vi[k] = size_t( s.geom.tris[t + size_t( k )] );
				if ( vi[k] >= nv ) {
					okTri = false;
					break;
				}
				const Vector3 wp = pos + rot * ( Vector3( s.geom.pos[vi[k] * 3 + 0], s.geom.pos[vi[k] * 3 + 1],
					s.geom.pos[vi[k] * 3 + 2] ) * scale );
				for ( int a = 0; a < 3; a++ )
					w[k][a] = wp[a];
			}
			if ( !okTri )
				continue;
			if ( s.geom.uv.size() >= nv * 2 )
				for ( int k = 0; k < 2; k++ )
					uv[k] = ( s.geom.uv[vi[0] * 2 + size_t( k )] + s.geom.uv[vi[1] * 2 + size_t( k )]
						+ s.geom.uv[vi[2] * 2 + size_t( k )] ) / 3.0f;
			if ( s.geom.rgba.size() == nv * 4 )
				for ( int k = 0; k < 4; k++ )
					vc[k] = ( s.geom.rgba[vi[0] * 4 + size_t( k )] + s.geom.rgba[vi[1] * 4 + size_t( k )]
						+ s.geom.rgba[vi[2] * 4 + size_t( k )] ) / ( 3.0f * 255.0f );
			if ( !alb.sample( tex, uv[0], uv[1], vc, lin, &ma ) )
				for ( int k = 0; k < 3; k++ )
					lin[k] = ProbeAlbedo::srgbToLinear( vc[k] );   // no map: the vertex color alone
			// the effect shader multiplies the material's opacity in twice, and tints by its base color
			const float op = s.nearFacts.effectShader ? s.matAlpha * s.matAlpha : s.matAlpha;
			if ( s.nearFacts.effectShader )
				for ( int k = 0; k < 3; k++ )
					lin[k] = std::clamp( lin[k] * s.bakeColor[k], 0.0f, 1.0f );
			const float a = std::clamp( op * ma * vc[3], 0.0f, 1.0f );
			quint8 tr[3];
			for ( int k = 0; k < 3; k++ ) {
				tr[k] = quint8( std::lround( std::clamp( 1.0f - a * ( 1.0f - lin[k] ), 0.0f, 1.0f ) * 255.0f ) );
				sum[k] += tr[k] / 255.0;
			}
			soup.addGlass( w[0], w[1], w[2], tr );
			fed++;
		}
	}
	if ( census ) {
		const auto clean = []( QString t ) { return t.replace( QChar( '\t' ), QChar( ' ' ) ); };
		const bool named = s.matName.endsWith( QStringLiteral( ".bgsm" ), Qt::CaseInsensitive )
			|| s.matName.endsWith( QStringLiteral( ".bgem" ), Qt::CaseInsensitive );
		QStringList f;
		f << QString::number( ref, 16 ).rightJustified( 8, QChar( '0' ) ) << clean( model )
		  << QString::number( s.nearFacts.block ) << clean( s.nearFacts.name ) << ( named ? clean( s.matName ) : QString() )
		  << ( s.nearFacts.effectShader ? QStringLiteral( "E" ) : QStringLiteral( "L" ) )
		  << QString::number( int( s.nearFacts.effectShader ? s.effectMatRead : s.nearFacts.bgsmRead ) )
		  << QString::number( int( s.bakeBlend ) ) << QString::number( s.bakeBlendSrc ) << QString::number( s.bakeBlendDst )
		  << QString::number( int( s.nearFacts.decal ) ) << QString::number( int( s.bakeEnv ) )
		  << QString::number( int( s.bakeSoft ) ) << QString::number( double( s.matAlpha ), 'f', 6 )
		  << QString::number( int( pane ) ) << QString::number( pane ? fed : int( s.geom.tris.size() / 3 ) );
		for ( int k = 0; k < 3; k++ )
			f << QString::number( fed ? sum[k] / fed : 1.0, 'f', 4 );
		*census += f.join( QChar( '\t' ) ) + QChar( '\n' );
	}
	return fed;
}
