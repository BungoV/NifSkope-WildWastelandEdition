#version 430
layout( local_size_x = 64 ) in;
layout( std430, binding = 0 ) buffer Surf { float surf[]; };          // 16 a surfel: n, dirK, alb, sunK, le, -, B1, -
layout( std430, binding = 1 ) readonly buffer PairStart { int pairStart[]; };
layout( std430, binding = 2 ) readonly buffer Pairs { float pairs[]; };   // 8: light, d, nl, dl, doorRec, -, -, -
layout( std430, binding = 3 ) readonly buffer DoorRec { float doorRec[]; }; // 12: door0, door1, -, -, T[8]
layout( std430, binding = 4 ) readonly buffer Lights { float lights[]; }; // 16: pos, radius, color, bias, scale, exp, cone, cosOuter, spot, k, s|x0, kind
layout( std430, binding = 5 ) coherent buffer Ctl { uint ctl[]; };       // 0 max change, 1 max B, 2.. door closed
layout( std430, binding = 6 ) readonly buffer LinkStart { int linkStart[]; };
layout( std430, binding = 7 ) readonly buffer Links { float links[]; };   // 12: surf, omega, tint, cos[6], doorRec
layout( std430, binding = 8 ) readonly buffer ProbeK { float probeK[]; }; // 20: kUnl, -, sky[18]
layout( std430, binding = 9 ) buffer PE { float pe[]; };                 // 18 a probe
layout( std430, binding = 10 ) buffer Bsrc { float bsrc[]; };           // 4 a surfel
layout( std430, binding = 11 ) buffer Bdst { float bdst[]; };
layout( std430, binding = 12 ) readonly buffer FeedStart { int feedStart[]; };
layout( std430, binding = 13 ) readonly buffer Feed { float feed[]; };    // 4: probe, w, doorRec, -
layout( std430, binding = 14 ) readonly buffer Slots { int slots[]; };   // 2: voxel, which
layout( std430, binding = 15 ) buffer Grid { float grid[]; };           // 2 x nVox x 24
uniform int uN;
uniform int uRed;          // 1 doorblanket, 2 glassopaque, 3 dooropen
uniform int uHasDir;
uniform vec3 uDirColor;
uniform int uSunOn;
uniform vec3 uSun;
uniform int uSkyOn;
uniform int uRedGrow;
uniform int uNVox;
uniform int uFirst;

float doorF( int rec, int c, bool vis )
{
	if ( rec < 0 || uRed == 3 )
		return 1.0;
	float f = 1.0;
	for ( int s = 0; s < 2; s++ ) {
		int d = floatBitsToInt( doorRec[rec * 12 + s] );
		if ( d < 0 || ctl[2 + d] == 0u )
			continue;
		float cl = uintBitsToFloat( ctl[2 + d] );   // lane DOORLIVE1: the slot's shut fraction (0u = open)
		int o = rec * 12 + 4 + s * 4;
		float tt;
		if ( uRed == 1 )
			tt = 0.0;
		else if ( uRed == 2 )
			tt = doorRec[o + 3];
		else if ( vis )
			tt = ( doorRec[o] > 0.0 || doorRec[o + 1] > 0.0 || doorRec[o + 2] > 0.0 ) ? 1.0 : 0.0;
		else
			tt = doorRec[o + c];
		if ( cl >= 1.0 ) {
			if ( uRed == 1 )
				return 0.0;
			f *= tt;
		} else {
			f *= 1.0 - cl * ( 1.0 - tt );
		}
	}
	return f;
}
