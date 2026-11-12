#ifndef WW_FEATURES_SHARED_CAMUNIRED_H
#define WW_FEATURES_SHARED_CAMUNIRED_H

/* lane EMPTY240 R12-1: the frame start's "camera and uniforms" step (build 91: 0.186 ms a frame) kept its per-call
 * string and driver work only where something moved. Red WW_CAMUNI_RED=<name>[,<name>...] (live; 1 or all: every
 * one) puts that item back as before. Names: envkeys (the light, sun-path, sky and physical-units keys read and
 * formatted per call), farplanes (the far plane line formatted per frame), jittervp (the TAA jitter asks the driver
 * for the viewport each frame), ubo (the global uniforms uploaded though they are the bytes already uploaded). */

#include "wwswitch.h"

#include <QByteArray>
#include <QList>

inline bool wwCamUniRed( const char * name )
{
	WW_SWITCH_STATIC( QByteArray, v, wwEnv( "WW_CAMUNI_RED" ).trimmed().toLower() );
	if ( v.isEmpty() )
		return false;
	if ( v == "1" || v == "all" )
		return true;
	return v.split( ',' ).contains( QByteArray( name ) );
}

#endif
