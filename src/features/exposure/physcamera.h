#ifndef WW_FEATURES_EXPOSURE_PHYSCAMERA_H
#define WW_FEATURES_EXPOSURE_PHYSCAMERA_H

/* lane MODULES1 G1 (bungo 11-09: "maybe auto exposure is not working properly"; a pinned EV 2.2 "worked, looks much
 * better"): the physical camera's law, std only, so the offline test (tests/features/exposure/physcamera) runs the
 * same code as the app. Turning on the spot at night swung the exposure 6.6 EV: the low-light key curve rose 2 EV
 * between 0.12 and 1 cd/m2, the highlight guard closed the camera another 1.4 EV whenever a sign was in view, and
 * nothing smoothed the steps.
 *  - Reflected meter (ISO 2720, K 12.5) with an adaptation floor: below 1 cd/m2 the camera stops opening, so every
 *    lit night street (EN 13201 road classes: 0.3..2 cd/m2) sits at one EV (2.21 at key 0.18) and darker views read
 *    darker. Games bound auto exposure the same way (Unreal: Min EV100 / Min Brightness; Lagarde & de Rousiers 2014).
 *  - The highlight guard works by day outdoors only, at most 1 EV, and never past the incident meter's own EV.
 *  - The meter caps each texel at the cover's median + 3 EV, so a few bright emitters cannot set the street's EV.
 *  - Adaptation in EV, exponential: Unreal's defaults (Speed Up 3 stops/s to a brighter scene, Speed Down 1 stop/s).
 * Red WW_PHYSUNITS_CAMERA_RED=1 (live): the law before G1 (Krawczyk 2005 key, guard everywhere, no cap, instant). */

#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>

namespace wwphys {

constexpr float kAdaptFloorCdm2 = 1.0f;	//!< the camera's lowest adaptation luminance, cd/m2
constexpr float kKeyDaylightCdm2 = 4000.0f;	//!< Krawczyk's key is 0 EV here; above it the old key stays
constexpr float kGuardMaxEv = 1.0f;		//!< the highlight guard's reach by day, EV
constexpr float kMeterCapEv = 3.0f;		//!< a texel counts at most this far over the cover's median, EV
constexpr float kAdaptTauBrighter = 0.48f;	//!< s, to a brighter scene: 1 / (3 stops/s x ln 2)
constexpr float kAdaptTauDarker = 1.44f;	//!< s, to a darker scene: 1 / (1 stop/s x ln 2)
constexpr float kAdaptSnapSeconds = 1.0f;	//!< a longer gap between frames is a load or a stall: no fade across it
constexpr float kAdaptSnapEv = 0.5f;		//!< a step of the incident meter (hour or weather set): no fade across it

struct CameraIn
{
	float la = 1.0f;		//!< the metered luminance, cd/m2
	bool hasKey = false;	//!< the imagespace gives a middle grey
	float key = 0.18f;
	bool day = false;		//!< outdoors with daylight (the incident meter's weight > 0)
	bool incident = false;	//!< the incident meter anchors the camera
	float incEv = 0.0f, dayW = 0.0f;
	float hiSpread = 0.0f;	//!< log2( 97th percentile / mean ) of the meter; 0 = no guard
	bool red = false;
};

struct CameraOut
{
	float ev = 0.0f, ecEye = 0.0f, ecKey = 0.0f, reflected = 0.0f, guard = 0.0f, target = 0.0f;
};

inline float krawczykKey( float l )
{
	return 1.03f - 2.0f / ( 2.0f + std::log10( l + 1.0f ) );
}

inline CameraOut cameraTarget( const CameraIn & in )
{
	CameraOut o;
	const float la = std::max( in.la, 1e-6f );
	o.ev = std::log2( la * 100.0f / 12.5f );
	// daylight and brighter (La >= 4000 cd/m2) keeps the key it had, so no day view is darker than before G1
	const float k = std::clamp( std::log2( std::max( krawczykKey( la ), 1e-3f ) / krawczykKey( 4000.0f ) ), -3.0f, 0.5f );
	o.ecEye = in.red || la >= kKeyDaylightCdm2 ? k : std::log2( la / std::max( la, kAdaptFloorCdm2 ) );
	// the imagespace's middle grey: the camera maps the mean to 1 / (1.2 x 8); HNAM's key as compensation
	o.ecKey = in.hasKey ? std::clamp( std::log2( std::max( in.key, 1e-3f ) * 1.2f * 8.0f ), -4.0f, 4.0f ) : 0.0f;
	float base = o.ev - o.ecEye - o.ecKey;
	o.reflected = base;
	if ( in.incident )
		base += ( std::clamp( base, in.incEv - 1.5f, in.incEv + 1.5f ) - base ) * in.dayW;
	if ( in.hiSpread > 0.0f ) {
		const float g = std::clamp( std::log2( la * std::exp2( in.hiSpread ) / 1.2f ) - base, 0.0f, 2.5f );
		if ( in.red )
			o.guard = g;
		else if ( in.day )
			o.guard = std::min( { g, kGuardMaxEv, std::max( 0.0f, in.incEv - base ) } ) * in.dayW;
	}
	o.target = std::clamp( base + o.guard, -6.0f, 18.0f );
	return o;
}

//! no fade: the first frame, a stall, a step of the daylight, or a world clock that stands (a harness still)
inline bool cameraSnap( bool valid, float dt, float incEv, float lastIncEv, bool clockMoves )
{
	return !valid || !clockMoves || !( dt <= kAdaptSnapSeconds ) || std::fabs( incEv - lastIncEv ) > kAdaptSnapEv;
}

//! one step of the adaptation, EV: a rising EV is a brighter scene
inline float cameraAdapt( float cur, float target, float dt )
{
	if ( !( dt > 0.0f ) )
		return cur;
	const float tau = target > cur ? kAdaptTauBrighter : kAdaptTauDarker;
	return cur + ( target - cur ) * ( 1.0f - std::exp( -dt / tau ) );
}

struct MeterOut
{
	float mean = -1.0f;		//!< cd/m2, < 0 = nothing metered
	float hiSpread = 0.0f;	//!< log2( 97th percentile / mean )
	float capShare = 0.0f;	//!< the cover's share the emitter cap held back
};

/*! The meter's core: the cover-weighted log average between the 2nd and 98th percentile of luminance, each texel
 *  capped at the cover's median + kMeterCapEv (red: no cap). t = (log2 luminance, cover) per texel; sorted here. */
inline MeterOut cameraMeter( std::vector<std::pair<float, float>> & t, bool red )
{
	MeterOut o;
	double cover = 0.0;
	for ( const auto & e : t )
		cover += e.second;
	if ( t.empty() || !( cover > 0.0 ) )
		return o;
	std::sort( t.begin(), t.end() );
	float cap = t.back().first;
	if ( !red ) {
		double run = 0.0;
		for ( const auto & e : t ) {
			run += e.second;
			if ( run >= 0.5 * cover ) {
				cap = std::min( cap, e.first + kMeterCapEv );
				break;
			}
		}
	}
	const double lo = 0.02 * cover, hi = 0.98 * cover;
	double run = 0.0, sum = 0.0, wsum = 0.0, capped = 0.0;
	float p97 = t.back().first;
	bool p97Set = false;
	for ( const auto & e : t ) {
		const float lg = e.first, a = e.second;
		const double w = std::max( 0.0, std::min( run + a, hi ) - std::max( run, lo ) );	// the cover inside [lo, hi]
		run += a;
		sum += w * std::min( lg, cap );
		wsum += w;
		if ( lg > cap )
			capped += a;
		if ( !p97Set && run >= 0.97 * cover ) {
			p97 = lg;
			p97Set = true;
		}
	}
	if ( !( wsum > 0.0 ) )
		return o;
	o.hiSpread = std::max( 0.0f, p97 - float( sum / wsum ) );
	o.mean = float( std::exp2( sum / wsum ) );
	o.capShare = float( capped / cover );
	return o;
}

}	// namespace wwphys

#endif
