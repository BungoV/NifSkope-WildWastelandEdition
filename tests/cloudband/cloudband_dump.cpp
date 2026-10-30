// lane CLOUDBAND1: Cloud Accumulation's CPU side for the offline gate (tests/spells/cloudband_check.py builds this with
// g++). Prints one "acc <cycle> <weight>" line per refresh cycle 0..15 (cloudvolmath.h accumAlpha, the weight cloudvol.cpp
// hands cloud_resolve_a.frag as cvAccAlpha).
#include "gl/cloudvolmath.h"

#include <cstdio>

int main()
{
	for ( long long c = 0; c < 16; c++ )
		std::printf( "acc %lld %.9g\n", c, double( WwCloudVolMath::accumAlpha( c ) ) );
	return 0;
}
