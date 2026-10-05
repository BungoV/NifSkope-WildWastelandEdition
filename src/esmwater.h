#ifndef ESMWATER_H
#define ESMWATER_H

/*! lane WATER1 (2026-10-04): ONE WATR reader for the cell view's water draw and the probe bake.
 *
 *  WwWaterRecord is the record as stored (DNAM in xEdit's order, NAM2-4 the three noise layers).
 *  wwWaterMaterial() turns it into the constant block the game's water shader reads, the way the game's
 *  own material update does it (measured on 1.10.155; notes in the lane folder):
 *    colors        pow( byte / 255, 2.2 ), each with the w lane named below
 *    varAmounts    ( sun specular power, reflectivity, opacity byte x 0.01, 250 )
 *    params1       ( clamp( layer 1 noise falloff, 1, 8191 ), 0.5, sparkle magnitude, max( depth amount, 1 ) )
 *    params2       ( surface effect falloff, normal magnitude, shallow normal falloff, deep normal falloff )
 *    params3       ( alpha shallow range, alpha deep range, shallow alpha, deep alpha )
 *    params4       ( color shallow range, color deep range, Fresnel amount, interior specular power )
 *    amplitude     ( layer 1..3 amplitude scale, displacement dampener )
 *    uvScale       ( layer 1..3 UV scale, 0 )
 */

#include <QString>

#include <algorithm>
#include <cmath>
#include <cstring>

struct WwWaterRecord
{
	quint32 form = 0;
	QString editorId;
	quint8 opacity = 0;		// ANAM (unused by the game's look, kept for the w lane it feeds)
	// DNAM: fog
	float depthAmount = 8192.0f;
	quint8 shallow[4] = {}, deep[4] = {};
	float colorShallowRange = 0.6f, colorDeepRange = 0.3f;
	float shallowAlpha = 0.0f, deepAlpha = 1.0f, alphaShallowRange = 1.0f, alphaDeepRange = 0.5f;
	quint8 underwater[4] = {};
	float uwFogAmount = 1.0f, uwFogNear = 0.0f, uwFogFar = 1000.0f;
	// physical
	float normalMagnitude = 0.5f, shallowNormalFalloff = 1.0f, deepNormalFalloff = 0.75f;
	float reflectivity = 0.5f, fresnel = 0.025f, surfaceEffectFalloff = 0.9f;
	float displacement[5] = { 0.4f, 0.6f, 0.985f, 10.0f, 0.05f };	// force, velocity, falloff, dampener, size
	quint8 reflection[4] = {};
	// specular
	float sunSpecPower = 50.0f, sunSpecMagnitude = 1.0f, sparklePower = 1.0f, sparkleMagnitude = 1.0f;
	float interiorRadius = 100000.0f, interiorBrightness = 1.0f, interiorPower = 100.0f;
	// noise, per layer
	float windDirection[3] = {}, windSpeed[3] = {}, amplitude[3] = {};
	float uvScale[3] = { 100.0f, 100.0f, 100.0f }, noiseFalloff[3] = { 300.0f, 300.0f, 300.0f };
	// silt
	float siltAmount = 0.0f;
	quint8 lightSilt[4] = {}, darkSilt[4] = {};
	bool ssr = true;
	int dnamBytes = 0;		// how much DNAM the record carried (the fields past it keep xEdit's defaults)
	QString noise[3];		// NAM2..NAM4, as stored
};

struct WwWaterMaterial
{
	float shallow[4], deep[4], reflection[4], underwater[4], lightSilt[4], darkSilt[4];
	float varAmounts[4], params1[4], params2[4], params3[4], params4[4], amplitude[4], uvScale[4];
};

inline float wwWaterLin( quint8 b )
{
	return std::pow( float( b ) * 0.003921568859368563f, 2.2f );
}

inline WwWaterMaterial wwWaterMaterial( const WwWaterRecord & r )
{
	WwWaterMaterial m;
	auto col = [&]( float o[4], const quint8 c[4], float w ) {
		for ( int i = 0; i < 3; i++ )
			o[i] = wwWaterLin( c[i] );
		o[3] = w;
	};
	col( m.shallow, r.shallow, r.sparklePower );
	col( m.deep, r.deep, r.sunSpecMagnitude );
	col( m.reflection, r.reflection, 0.0f );
	col( m.underwater, r.underwater, 0.0f );
	col( m.lightSilt, r.lightSilt, r.siltAmount );
	col( m.darkSilt, r.darkSilt, 0.0f );
	const float v[4] = { r.sunSpecPower, r.reflectivity, float( r.opacity ) * 0.01f, 250.0f };
	const float p1[4] = { std::clamp( r.noiseFalloff[0], 1.0f, 8191.0f ), 0.5f, r.sparkleMagnitude,
		std::max( r.depthAmount, 1.0f ) };
	const float p2[4] = { r.surfaceEffectFalloff, r.normalMagnitude, r.shallowNormalFalloff, r.deepNormalFalloff };
	const float p3[4] = { r.alphaShallowRange, r.alphaDeepRange, r.shallowAlpha, r.deepAlpha };
	const float p4[4] = { r.colorShallowRange, r.colorDeepRange, r.fresnel, r.interiorPower };
	const float a[4] = { r.amplitude[0], r.amplitude[1], r.amplitude[2], r.displacement[3] };
	const float u[4] = { r.uvScale[0], r.uvScale[1], r.uvScale[2], 0.0f };
	std::memcpy( m.varAmounts, v, sizeof v );
	std::memcpy( m.params1, p1, sizeof p1 );
	std::memcpy( m.params2, p2, sizeof p2 );
	std::memcpy( m.params3, p3, sizeof p3 );
	std::memcpy( m.params4, p4, sizeof p4 );
	std::memcpy( m.amplitude, a, sizeof a );
	std::memcpy( m.uvScale, u, sizeof u );
	return m;
}

//! one line per record for the notes and the reader gate: "WATR <form> <edid> <the material, %.6g each>"
QString wwWaterDescribe( const WwWaterRecord & r );

#endif // ESMWATER_H
