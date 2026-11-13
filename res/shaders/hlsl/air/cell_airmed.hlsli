// HLSL source of features/air/cell_airmed.glsl (include-only GLSL), lane HLSLSRC2 (notes/hlslsrc2/tools/glsl2hlsl.py): the GLSL's comments,
// names and order kept. Its loose uniforms sit at the top under WW_LOOSE_UNIFORMS: the entry that includes this
// file includes it once more inside its LooseUniforms cbuffer with that defined (HLSL has one cbuffer for them).
#ifdef WW_HLSL_SRC
#if defined(WW_LOOSE_TYPES)	// the types its loose uniforms use

#elif defined(WW_LOOSE_UNIFORMS)
	float4 airMed;			// sigma0 (per game unit), zb, 1 / H (0: uniform), the top (the fog's height frame)
	float4 airLayer;			// the near band's share w, zn, 1 / Hn, the ground layer's strength s (cellvolair.cpp VaLayer)
	float4 airGround;			// the ground layer's anchor zg, 1 / its thickness, G2: 1 / the top's falloff height (0: a hard top)
	int airTerrOn;			// FOGTERRAIN1: the ground layer follows the low-pass terrain (red absolute: 0)
	float4 airTerrXf;			// u = x X.x + X.y, v = y X.z + X.w (world)
	float4 airTerrFineXf;		// FOGTERRAIN3: the fine level's (uv -1 everywhere: none)
	float airTerrRef;		// the world z the absolute anchor stands on (the fog frame's 0)
	int volRed;				// 2 gioff, 4 flat, 8 noshadow, 16 nolights, 256 physcol, 262144 lightmid, 524288 bluescatter, 4194304 roomstep (1 off, 32 wrongmed, 128 farfit: the CPU's)
	float4 volPhase[3];		// rgb: the weight of the air (Rayleigh), forward and back terms; w: the HG g (0 for air)
	float4 volGi;				// rgb: the phase's tint (the three weights summed), w: its L1 g
	/* lane FIX1101A (atmo, the fog-sun fault): the SUN's transmittance to height z. airSunLayer (shipped; set by
	 * cellvolair.cpp, false under the red WW_CELL_AIR_RED=suncolumn) = only the derived ground layer dims it: the bands are
	 * the records' fog and the weather's Sunlight colour is already the sun it gives the ground. The whole column to the scene
	 * top (airTup) took Foggy noon at c3 to 0.026: no shafts. lookdev_fog.glsl wwVolAirSunT (flag 128) is the surfaces' twin. */
	bool airSunLayer;
	/* SKY2 FOGSLICE1 (bungo 10-23, fog slices when flying up and down): the grid is valid only within the probes' reach of
	 * surfaces; the air above it takes the flat ambient. The valid share fell 1 -> 0 over one voxel (70 u at Concord) while the
	 * probe GI and the ambient differ (0.0099 vs 0.0182 at the Concord street), so the air held world-anchored layers at the
	 * shell's edges. The share is now read over the probes' reach (8 taps at +-volShell voxels, a 4-voxel ramp) and the GI
	 * value comes in by that share, so the air blends GI -> ambient continuously. volShell 0 (red shellstep) = the old law. */
	float volShell;
#if defined(WW_AIR_GIVOL)
	bool cellGiAirOn;
	float3 cellGiAirOrigin;
	float cellGiAirVoxel;
	float3 cellGiAirDims;
#endif
	/* lane FIX1101A (bungo 11-01, fog not shadowed by buildings): the air's ENV share (sky, GI, ambient: unshadowed by
	 * design) takes the records' fog colour only, without the fog-sun glare; the glare hue stays on the sun's share,
	 * which carries the shadow (airSun x vis). Before, every sample looking toward the sun took the glare hue whether or
	 * not the sun reached it: shadowed air in front of a facade glowed orange. airEnvFogHue is set by cellvolair.cpp
	 * (shipped on); red WW_CELL_AIR_RED=envhue (or physcol) brings the old hue on the env share. */
	bool airEnvFogHue;
#else
#ifndef WW_GLSL_SIZE_Texture3D_float4_
#define WW_GLSL_SIZE_Texture3D_float4_
int3 textureSize(Texture3D<float4> t, int lod) { uint w, h, d, l; t.GetDimensions(uint(lod), w, h, d, l); return int3(w, h, d); }
#endif
/* lane VOLPART1: the air's medium, its phase, its six-axis cube and its records' colour, shared by the froxel passes
 * (cell_volair.frag) and the volumetric particles and effect cards (cell_volpart.glsl), so a steam puff is lit by the
 * very law the air is. Needs lookdev_fog.glsl (wwAirRhoLaw, wwAirColLaw, wwAirRecordColour) and cell_lights.glsl (the
 * GI grid and its rooms) included first. Moved out of cell_volair.frag unchanged (the landed VOLAIR1). */


Texture2DArray<float4> airTerr : register(t17, space0); SamplerState _airTerr_sampler : register(s17, space0);	// the low-pass terrain height, world z (R32F, clamped at its edge); FOGTERRAIN3: layer 1 the fine level
static float airGz = 0.0;				// the local ground's rise over the frame's ground at the current sample (0: the absolute law)

// FOGTERRAIN3: the ground at world xy, the fine level blended over the coarse one, faded out in its window's last 16th
float airTerrH( float2 Pw )
{
	float hc = airTerr.Sample( _airTerr_sampler, float3( Pw.x * airTerrXf.x + airTerrXf.y, Pw.y * airTerrXf.z + airTerrXf.w, 0.0 ) ).r;
	float2 uf = float2( Pw.x * airTerrFineXf.x + airTerrFineXf.y, Pw.y * airTerrFineXf.z + airTerrFineXf.w );
	float wf = clamp( min( min( uf.x, 1.0 - uf.x ), min( uf.y, 1.0 - uf.y ) ) * 16.0, 0.0, 1.0 );
	return wf > 0.0 ? lerp( hc, airTerr.Sample( _airTerr_sampler, float3( uf, 1.0 ) ).r, wf ) : hc;
}

// FOGTERRAIN1: set the ground layer's anchor for world point Pw (the froxel pass; FOGTERRAIN2: the particles too)
void airGroundAt( float3 Pw )
{
	airGz = airTerrOn != 0 ? airTerrH( Pw.xy ) - airTerrRef : 0.0;
}

static const float PI = 3.14159265;

float volHg( float c, float g )
{
	float d = 1.0 + g * g - 2.0 * g * c;
	return ( 1.0 - g * g ) / ( 4.0 * PI * d * sqrt( d ) );
}

// pi x the phase (display units): c = the cosine between the light's travel and the scattered ray's (to the eye)
float3 volPhasePi( float c )
{
	return PI * ( volPhase[0].rgb * ( 3.0 / ( 16.0 * PI ) ) * ( 1.0 + c * c )
		+ volPhase[1].rgb * volHg( c, volPhase[1].w ) + volPhase[2].rgb * volHg( c, volPhase[2].w ) );
}

// the column of air from zb up to z, per unit sigma0: F(z) = z - zb below zb, H (1 - exp(-(z - zb) / H)) above,
// constant above the top
float airF( float z )
{
	z = min( z, airMed.w );
	float x = z - airMed.y;
	if ( or( x <= 0.0, airMed.z <= 0.0 ) )
		return x;
	float u = x * airMed.z;
	float e = u < 1e-3 ? u * ( 1.0 - u * ( 0.5 - u / 6.0 ) ) : 1.0 - exp( -u );
	return e / airMed.z;
}

// the density per unit sigma0 (lookdev_fog.glsl wwAirRhoLaw: the far band, the near band's share, the ground layer)
float airRho( float z )
{
	return wwAirRhoLaw( z, airMed, airLayer, float4( airGround.x + airGz, airGround.yzw ) );
}

// (1 - exp(-u)) / u, precise for small u
float airE1( float u )
{
	return u < 1e-3 ? 1.0 - u * ( 0.5 - u / 6.0 ) : ( 1.0 - exp( -u ) ) / u;
}

// the column of air between heights lo < hi, per unit sigma0, without the cancellation of F(hi) - F(lo): F is a
// large number (the column from zb, up to H ~ 1e6 units), its difference over a few units lost to float precision
// near the horizon (the gate's R found it at Sanctuary's foggy band)
float airCol( float lo, float hi )
{
	return wwAirColLaw( lo, hi, airMed, airLayer, float4( airGround.x + airGz, airGround.yzw ) );
}

// the optical depth of a straight segment of length len (game units) from height za to height zb
float airOd( float za, float zb, float len )
{
	float dz = zb - za;
	if ( abs( dz ) < 0.5 )
		return airMed.x * len * airRho( 0.5 * ( za + zb ) );
	return airMed.x * len * airCol( min( za, zb ), max( za, zb ) ) / abs( dz );
}

// 1 - exp(-tau), precise for small tau
float air1mE( float tau )
{
	return tau < 1e-3 ? tau * ( 1.0 - tau * ( 0.5 - tau / 6.0 ) ) : 1.0 - exp( -tau );
}

// the transmittance from height z toward a direction whose height grows mu per unit, to the top
float airTup( float z, float mu )
{
	if ( z >= wwAirEnd( airMed, airGround ) )	// MODULES1 G2: the column runs through the top's falloff (lookdev_fog.glsl wwAirColLaw)
		return 1.0;
	mu = max( mu, 0.02 );
	return exp( -airMed.x * airCol( z, wwAirEnd( airMed, airGround ) ) / mu );
}
float airTupSun( float z, float mu )
{
	if ( !airSunLayer )
		return airTup( z, mu );
	if ( or( z >= airMed.w, airLayer.w <= 0.0 ) )
		return 1.0;
	return exp( -airMed.x * airLayer.w * wwAirColOne( z, airMed.w, airGround.x + airGz, airGround.y ) / max( mu, 0.02 ) );
}

/* the six-axis cube at P, axis a = +X -X +Y -Y +Z -Z, in the surfaces' GI unit (lane VOLFOG1b): a froxel is a point in
 * the air, not a surface; the voxels of P's room only, an axis with no weight takes the grid's plain trilinear.
 * share[a] = the grid's valid share there (the ambient's replace rule).
 * FOGFIX1 (bungo 10-16, the squares in the Concord roof fog): one room label read at P switched at every fine room cell
 * and at the room grid's box, and a room with one voxel left held that voxel flat -- hard-edged boxes in the air. Now the
 * 8 room cells around P are blended trilinearly, each label's answer fading to the plain trilinear below a quarter of
 * the voxel weight; solid cells drop out, cells outside the grid read plain. Continuous, and still no voxel of another
 * room. Red WW_CELL_AIR_RED=roomstep: the one label at P, as before. */
float4 volRoomAxis( int3 c0, float3 f, int2 L, int a, float4 plain, int3 dm )
{
	float4 s = (float4)( 0.0 );
	float ws = 0.0;
	for ( int k = 0; k < 8; k++ ) {
		int3 o = int3( k & 1, ( k >> 1 ) & 1, ( k >> 2 ) & 1 );
		int3 c = clamp( c0 + o, (int3)( 0 ), dm - 1 );
		float3 t = lerp( 1.0 - f, f, float3( o ) );
		float w = t.x * t.y * t.z;
		int2 S = cellRoomUnpack( cellGiSlots.Load( int4(c, 0) ).r );
		int base = ( S.x >= 0 && ( or( S.x == L.x, S.x == L.y ) ) ) ? 0 : ( S.y >= 0 && ( or( S.y == L.x, S.y == L.y ) ) ) ? 6 * dm.z : -1;
		if ( or( base < 0, w <= 0.0 ) )
			continue;
		float4 v = cellGi.Load( int4(int3( c.xy, c.z + base + a * dm.z ), 0) );
		if ( v.a > 0.0 ) {
			s += w * v;
			ws += w;
		}
	}
	if ( ( volRed & 4194304 ) != 0 )
		return ws > 0.0 ? s / ws : plain;
	const float wMin = 0.25;
	return ws >= wMin ? s / ws : ( s + ( wMin - ws ) * plain ) / wMin;
}
#ifdef WW_AIR_GIVOL
/* SKY2 FOGSLICE1, the fix at the cause (Division deck D2 s55, D9 s61): the air samples its own filled volume of the
 * probes' mean irradiance (WwCellGi::air, 3/4 of the probe spacing, every voxel filled, the edge clamped), so open air
 * reads the probes' sky and no flat ambient takes over above the grid's shell. Reds: noairvol (the shell law above),
 * shellstep (the old law). */
Texture3D<float4> cellGiAir : register(t18, space0); SamplerState _cellGiAir_sampler : register(s18, space0);
#endif

void volCubeAll( float3 P, out float3 E[6], out float share[6] )
{
#ifdef WW_AIR_GIVOL
	if ( cellGiAirOn ) {
		float3 ga = ( P - cellGiAirOrigin ) / cellGiAirVoxel;
		float za = clamp( ga.z, 0.5, cellGiAirDims.z - 0.5 );
		for ( int a = 0; a < 6; a++ ) {
			E[a] = max( cellGiAir.Sample( _cellGiAir_sampler, float3( ga.xy / cellGiAirDims.xy, ( za + float( a ) * cellGiAirDims.z ) / ( 6.0 * cellGiAirDims.z ) ) ).rgb, (float3)( 0.0 ) );
			share[a] = 1.0;
		}
		return;
	}
#endif
	int3 dm = int3( cellGiDims );
	float3 g = ( P - cellGiOrigin ) / cellGiVoxel;
	float2 xy = g.xy / cellGiDims.xy;
	float z = clamp( g.z, 0.5, cellGiDims.z - 0.5 );
	float depth = float( textureSize( cellGi, 0 ).z );	// lanes EMISSIVEGRID1 + SPECOCC1: the fine atlas / sky slabs may follow
	float4 plain[6];
	float4 r[6];
	for ( int a = 0; a < 6; a++ ) {
		plain[a] = cellGi.Sample( _cellGi_sampler, float3( xy, ( z + float( a ) * cellGiDims.z ) / depth ) );
		r[a] = plain[a];
	}
	if ( cellGiRooms && ( cellPassRed & 8 ) == 0 ) {
		float3 gv = g - 0.5;
		int3 c0 = int3( floor( gv ) );
		float3 f = gv - float3( c0 );
		if ( ( volRed & 4194304 ) != 0 ) {
			int2 L = cellRoomAt( P );
			if ( L.x >= 0 )
				for ( int a = 0; a < 6; a++ )
					r[a] = volRoomAxis( c0, f, L, a, plain[a], dm );
		} else {
			// the 8 room cells around P: labels, trilinear weights; solid cells drop out, outside the grid = plain
			float3 gr = ( P - cellRoomsOrigin ) / cellRoomsCell - 0.5;
			int3 r0 = int3( floor( gr ) );
			float3 fr = gr - float3( r0 );
			int2 Lk[8];
			float wk[8];
			float wAir = 0.0;
			bool same = true;
			for ( int k = 0; k < 8; k++ ) {
				int3 o = int3( k & 1, ( k >> 1 ) & 1, ( k >> 2 ) & 1 );
				int3 c = r0 + o;
				float3 t = lerp( 1.0 - fr, fr, float3( o ) );
				wk[k] = t.x * t.y * t.z;
				if ( or( any( (c < (int3)( 0 )) ), any( (c >= int3( cellRoomsDims )) ) ) )
					Lk[k] = (int2)( -2 );	// outside the room grid: the plain read
				else
					Lk[k] = cellRoomUnpack( cellRooms.Load( int4(c, 0) ).r );
				if ( Lk[k].x == -1 )
					wk[k] = 0.0;	// solid
				wAir += wk[k];
				if ( k > 0 && any(Lk[k] != Lk[0]) )
					same = false;
			}
			if ( wAir > 0.0 ) {
				if ( same ) {
					if ( Lk[0].x >= 0 )
						for ( int a = 0; a < 6; a++ )
							r[a] = volRoomAxis( c0, f, Lk[0], a, plain[a], dm );
				} else {
					for ( int a = 0; a < 6; a++ )
						r[a] = (float4)( 0.0 );
					for ( int k = 0; k < 8; k++ ) {
						if ( wk[k] <= 0.0 )
							continue;
						bool seen = false;
						for ( int j = 0; j < k; j++ )
							seen = or( seen, ( wk[j] > 0.0 && all(Lk[j] == Lk[k]) ) );
						if ( seen )
							continue;
						float wl = 0.0;
						for ( int j = k; j < 8; j++ )
							wl += all(Lk[j] == Lk[k]) ? wk[j] : 0.0;
						wl /= wAir;
						for ( int a = 0; a < 6; a++ )
							r[a] += wl * ( Lk[k].x >= 0 ? volRoomAxis( c0, f, Lk[k], a, plain[a], dm ) : plain[a] );
					}
				}
			}
		}
	}
	if ( volShell > 0.0 && cellGiSky ) {
		float4 wide[6];
		for ( int a = 0; a < 6; a++ )
			wide[a] = (float4)( 0.0 );
		for ( int k = 0; k < 8; k++ ) {
			float3 q = g + ( float3( k & 1, ( k >> 1 ) & 1, ( k >> 2 ) & 1 ) * 2.0 - 1.0 ) * volShell;
			float qz = clamp( q.z, 0.5, cellGiDims.z - 0.5 );
			for ( int a = 0; a < 6; a++ )
				wide[a] += 0.125 * cellGi.Sample( _cellGi_sampler, float3( q.xy / cellGiDims.xy, ( qz + float( a ) * cellGiDims.z ) / depth ) );
		}
		for ( int a = 0; a < 6; a++ ) {
			// the GI's own value (P's room answer, else the wide mean), weighted by the wide share; the ambient takes the rest
			float3 v = r[a].a > 0.01 ? r[a].rgb / r[a].a : ( wide[a].a > 1e-4 ? wide[a].rgb / wide[a].a : (float3)( 0.0 ) );
			share[a] = clamp( wide[a].a, 0.0, 1.0 );
			E[a] = max( v * share[a], (float3)( 0.0 ) );
		}
		return;
	}
	for ( int a = 0; a < 6; a++ ) {
		// as cellGiE0: a sky grid keeps the valid share, else the mean over the valid share
		E[a] = cellGiSky ? max( r[a].rgb, (float3)( 0.0 ) ) : ( r[a].a > 0.01 ? max( r[a].rgb / r[a].a, (float3)( 0.0 ) ) : (float3)( 0.0 ) );
		share[a] = clamp( plain[a].a, 0.0, 1.0 );
	}
}

// L0 + L1 of a six-axis cube of irradiances (display units), scattered toward the eye along dW (the view ray's
// direction, world): 1/4 x max(0, (2/3) sum E + 3 g (E+ - E-).d) x the phase's tint
float3 volCubeScatter( float3 E[6], float3 dW )
{
	float3 fl = ( 2.0 / 3.0 ) * ( E[0] + E[1] + E[2] + E[3] + E[4] + E[5] );
	float3 l1 = ( E[0] - E[1] ) * dW.x + ( E[2] - E[3] ) * dW.y + ( E[4] - E[5] ) * dW.z;
	return 0.25 * max( fl + 3.0 * volGi.w * l1, (float3)( 0.0 ) ) * volGi.rgb;
}

/* MODULES1 10b-3: what the flat ambient keeps where the ray-traced volume covers the point (the air's leak fix): fade =
 * the volume's cover, r = its open-field share there (< 0: none, the volume's own read replaces the covered part). ONE
 * law for the air's froxel (cell_volair.frag) and a lit card (cell_volpart.glsl vpAmbKeep): the same sky share. */
float airAmbKeep( float fade, float r )
{
	return r >= 0.0 ? 1.0 - fade + fade * r : 1.0 - fade;
}

/* the haze's colour from the records (owner's ruling 10-14): the game's own fog colour at this distance and height
 * (the WTHR / lighting template near, far and high colours by hour, wwFogEval) mixed toward the fog-sun colour as the
 * game mixes it (fDirectionalFogPower), at unit luminance; the physical light keeps its brightness and direction.
 * Applied to the sky's share (the directional light, the GI, the ambient); the placed lights keep their own records'
 * colours. Red physcol (256): the physical colour (the phase's tint x the light's). */
float3 airEnvColour( float3 J, float t, float z, float3 dirView )
{
	if ( ( volRed & 256 ) != 0 )
		return J;
	bool ok;
	float3 c = wwAirRecordColour( t, z, dirView, ok );	// lookdev_fog.glsl, at unit luminance
	return ok ? dot( J, float3( 0.2126, 0.7152, 0.0722 ) ) * c : J;
}
float3 airEnvColourEnv( float3 J, float t, float z, float3 dirView )
{
	if ( or( !airEnvFogHue, ( volRed & 256 ) != 0 ) )
		return airEnvColour( J, t, z, dirView );
	float hb;
	float3 fc;
	wwFogEval( t, z, hb, fc );
	fc = max( fc, (float3)( 0.0 ) );
	float lc = dot( fc, float3( 0.2126, 0.7152, 0.0722 ) );
	return lc > 1e-8 ? ( dot( J, float3( 0.2126, 0.7152, 0.0722 ) ) / lc ) * fc : J;
}
#endif	// WW_LOOSE_UNIFORMS
#else	// WW_HLSL_SRC: the per-variant files' half (HLSLBULK1, WW_FN_<name> picks functions); it goes with the last of them
// Shared HLSL for res/shaders/features/air/cell_airmed.glsl (include-only GLSL), HLSLBULK1 round 2 (notes/hlslbulk1/tools/factor.py). No bindings: the
// functions below are the converted ones, they read the declarations of the shader that includes them (its cbuffers,
// resources, statics and LooseUniforms: the per-module shim and the entry own those). Define WW_FN_<name> for each
// wanted function before the #include (and #undef after), or WW_FN_ALL for all of them; a _V<n> suffix is a variant
// of the same GLSL function (a different define set at the entry). Fusion Shaders (F4FS) reuses the math from here.
// Included by: cell_precip_gpu_gs.hlsl, cell_precip_look_gs.hlsl, cell_precip_ps.hlsl, cell_volair_ps.hlsl, fo4_effectcell_ps.hlsl, fo4_effectcellfx_ps.hlsl, fo4_effectdecal_ps.hlsl, fo4_effectdecalcsm_ps.hlsl, particles_cell_ps.hlsl
// Not here (declarations, or no converted entry carried them): airF, airE1, air1mE, airEnvColourEnv
#ifdef WW_FN_ALL
static const float PI = 3.14159265;
#endif

#if defined(WW_FN_airTerrH) || defined(WW_FN_ALL)
float airTerrH(float2 Pw)
{
    float hc = airTerr.Sample(_airTerr_sampler, float3((Pw.x * airTerrXf.x) + airTerrXf.y, (Pw.y * airTerrXf.z) + airTerrXf.w, 0.0f)).x;
    float2 uf = float2((Pw.x * airTerrFineXf.x) + airTerrFineXf.y, (Pw.y * airTerrFineXf.z) + airTerrFineXf.w);
    float wf = clamp(min(min(uf.x, 1.0f - uf.x), min(uf.y, 1.0f - uf.y)) * 16.0f, 0.0f, 1.0f);
    float airTerrHRet;
    if (wf > 0.0f)
    {
        airTerrHRet = lerp(hc, airTerr.Sample(_airTerr_sampler, float3(uf, 1.0f)).x, wf);
    }
    else
    {
        airTerrHRet = hc;
    }
    return airTerrHRet;
}
#endif

#if defined(WW_FN_airTerrH_V2) || defined(WW_FN_ALL)
float airTerrH(float2 Pw)
{
    float hc = airTerr.SampleLevel(_airTerr_sampler, float3((Pw.x * airTerrXf.x) + airTerrXf.y, (Pw.y * airTerrXf.z) + airTerrXf.w, 0.0f), 0.0f).x;
    float2 uf = float2((Pw.x * airTerrFineXf.x) + airTerrFineXf.y, (Pw.y * airTerrFineXf.z) + airTerrFineXf.w);
    float wf = clamp(min(min(uf.x, 1.0f - uf.x), min(uf.y, 1.0f - uf.y)) * 16.0f, 0.0f, 1.0f);
    float airTerrHRet;
    if (wf > 0.0f)
    {
        airTerrHRet = lerp(hc, airTerr.SampleLevel(_airTerr_sampler, float3(uf, 1.0f), 0.0f).x, wf);
    }
    else
    {
        airTerrHRet = hc;
    }
    return airTerrHRet;
}
#endif

#if defined(WW_FN_airGroundAt) || defined(WW_FN_ALL)
void airGroundAt(float3 Pw)
{
    float airTerrHRet;
    if (airTerrOn != 0)
    {
        float2 airTerrHPart = Pw.xy;
        airTerrHRet = airTerrH(airTerrHPart) - airTerrRef;
    }
    else
    {
        airTerrHRet = 0.0f;
    }
    airGz = airTerrHRet;
}
#endif

#if defined(WW_FN_volHg) || defined(WW_FN_ALL)
float volHg(float c, float g)
{
    float d = (1.0f + (g * g)) - ((2.0f * g) * c);
    return (1.0f - (g * g)) / ((12.56637096405029296875f * d) * sqrt(d));
}
#endif

#if defined(WW_FN_volPhasePi) || defined(WW_FN_ALL)
float3 volPhasePi(float c)
{
    float airTerrHRet = c;
    float airTerrHPart = volPhase[1].w;
    float volPhasePiRet2 = c;
    float volPhasePiRet = volPhase[2].w;
    return ((((volPhase[0].xyz * 0.0596831031143665313720703125f) * (1.0f + (c * c))) + (volPhase[1].xyz * volHg(airTerrHRet, airTerrHPart))) + (volPhase[2].xyz * volHg(volPhasePiRet2, volPhasePiRet))) * 3.1415927410125732421875f;
}
#endif

#if defined(WW_FN_airRho) || defined(WW_FN_ALL)
float airRho(float z)
{
    float airTerrHRet = z;
    float4 airTerrHPart = airMed;
    float4 volPhasePiRet2 = airLayer;
    float4 volPhasePiRet = float4(airGround.x + airGz, airGround.yzw);
    float airRhoRet = wwAirRhoLaw(airTerrHRet, airTerrHPart, volPhasePiRet2, volPhasePiRet);
    return airRhoRet;
}
#endif

#if defined(WW_FN_airCol) || defined(WW_FN_ALL)
float airCol(float lo, float hi)
{
    float airTerrHRet = lo;
    float airTerrHPart = hi;
    float4 volPhasePiRet2 = airMed;
    float4 volPhasePiRet = airLayer;
    float4 airRhoRet = float4(airGround.x + airGz, airGround.yzw);
    float airColRet = wwAirColLaw(airTerrHRet, airTerrHPart, volPhasePiRet2, volPhasePiRet, airRhoRet);
    return airColRet;
}
#endif

#if defined(WW_FN_airOd) || defined(WW_FN_ALL)
float airOd(float za, float zb, float len)
{
    float dz = zb - za;
    if (abs(dz) < 0.5f)
    {
        float airTerrHRet = 0.5f * (za + zb);
        return (airMed.x * len) * airRho(airTerrHRet);
    }
    float airTerrHPart = min(za, zb);
    float volPhasePiRet2 = max(za, zb);
    return ((airMed.x * len) * airCol(airTerrHPart, volPhasePiRet2)) / abs(dz);
}
#endif

#if defined(WW_FN_airTup) || defined(WW_FN_ALL)
float airTup(float z, inout float mu)
{
    float4 airTerrHRet = airMed;
    float4 airTerrHPart = airGround;
    if (z >= wwAirEnd(airTerrHRet, airTerrHPart))
    {
        return 1.0f;
    }
    mu = max(mu, 0.0199999995529651641845703125f);
    float4 volPhasePiRet2 = airMed;
    float4 volPhasePiRet = airGround;
    float airRhoRet = z;
    float airColRet = wwAirEnd(volPhasePiRet2, volPhasePiRet);
    return exp(((-airMed.x) * airCol(airRhoRet, airColRet)) / mu);
}
#endif

#if defined(WW_FN_airTupSun) || defined(WW_FN_ALL)
float airTupSun(float z, float mu)
{
    if (!airSunLayer)
    {
        float airTerrHRet = z;
        float airTerrHPart = mu;
        float volPhasePiRet2 = airTup(airTerrHRet, airTerrHPart);
        return volPhasePiRet2;
    }
    bool volPhasePiRet = z >= airMed.w;
    bool airRhoRet;
    if (!volPhasePiRet)
    {
        airRhoRet = airLayer.w <= 0.0f;
    }
    else
    {
        airRhoRet = volPhasePiRet;
    }
    if (airRhoRet)
    {
        return 1.0f;
    }
    float airColRet = z;
    float airTupSunRet3 = airMed.w;
    float airTupSunRet2 = airGround.x + airGz;
    float airTupSunRet = airGround.y;
    return exp((((-airMed.x) * airLayer.w) * wwAirColOne(airColRet, airTupSunRet3, airTupSunRet2, airTupSunRet)) / max(mu, 0.0199999995529651641845703125f));
}
#endif

#if defined(WW_FN_volRoomAxis) || defined(WW_FN_ALL)
float4 volRoomAxis(int3 c0, float3 f, int2 L, int a, float4 plain, int3 dm)
{
    float4 s = 0.0f.xxxx;
    float ws = 0.0f;
    int airTerrHRet;
    int airTerrHPart;
    for (int k = 0; k < 8; k++)
    {
        int3 o = int3(k & 1, (k >> 1) & 1, (k >> 2) & 1);
        int3 c = clamp(c0 + o, int3(0, 0, 0), dm - int3(1, 1, 1));
        float3 t = lerp(1.0f.xxx - f, f, float3(o));
        float w = (t.x * t.y) * t.z;
        float volPhasePiRet2 = cellGiSlots.Load(int4(c, 0)).x;
        int2 S = cellRoomUnpack(volPhasePiRet2);
        bool volPhasePiRet = S.x >= 0;
        bool airRhoRet;
        if (volPhasePiRet)
        {
            bool airColRet = S.x == L.x;
            bool airTupSunRet3;
            if (!airColRet)
            {
                airTupSunRet3 = S.x == L.y;
            }
            else
            {
                airTupSunRet3 = airColRet;
            }
            airRhoRet = airTupSunRet3;
        }
        else
        {
            airRhoRet = volPhasePiRet;
        }
        if (airRhoRet)
        {
            airTerrHRet = 0;
        }
        else
        {
            bool airTupSunRet2 = S.y >= 0;
            bool airTupSunRet;
            if (airTupSunRet2)
            {
                bool cond = S.y == L.x;
                bool airTupSunPick;
                if (!cond)
                {
                    airTupSunPick = S.y == L.y;
                }
                else
                {
                    airTupSunPick = cond;
                }
                airTupSunRet = airTupSunPick;
            }
            else
            {
                airTupSunRet = airTupSunRet2;
            }
            if (airTupSunRet)
            {
                airTerrHPart = 6 * dm.z;
            }
            else
            {
                airTerrHPart = -1;
            }
            airTerrHRet = airTerrHPart;
        }
        int base = airTerrHRet;
        bool wwh0 = (base < 0);
        bool wwh1 = (w <= 0.0f);
        if (or(wwh0, wwh1))   // HLSLFIX2 orfix.py: no short-circuit branch (NV took the wrong side)
        {
            continue;
        }
        float4 v = cellGi.Load(int4(int3(c.xy, (c.z + base) + (a * dm.z)), 0));
        if (v.w > 0.0f)
        {
            s += (v * w);
            ws += w;
        }
    }
    if ((volRed & 4194304) != 0)
    {
        float4 volRoomAxisRet2;
        if (ws > 0.0f)
        {
            volRoomAxisRet2 = s / ws.xxxx;
        }
        else
        {
            volRoomAxisRet2 = plain;
        }
        return volRoomAxisRet2;
    }
    float4 volRoomAxisRet;
    if (ws >= 0.25f)
    {
        volRoomAxisRet = s / ws.xxxx;
    }
    else
    {
        volRoomAxisRet = (s + (plain * (0.25f - ws))) / 0.25f.xxxx;
    }
    return volRoomAxisRet;
}
#endif

#if defined(WW_FN_volCubeAll) || defined(WW_FN_ALL)
void volCubeAll(float3 P, inout float3 E[6], inout float share[6])
{
    if (cellGiAirOn)
    {
        float3 ga = (P - cellGiAirOrigin) / cellGiAirVoxel.xxx;
        float za = clamp(ga.z, 0.5f, cellGiAirDims.z - 0.5f);
        for (int a = 0; a < 6; a++)
        {
            E[a] = max(cellGiAir.Sample(_cellGiAir_sampler, float3(ga.xy / cellGiAirDims.xy, (za + (float(a) * cellGiAirDims.z)) / (6.0f * cellGiAirDims.z))).xyz, 0.0f.xxx);
            share[a] = 1.0f;
        }
        return;
    }
    int3 dm = int3(cellGiDims);
    float3 g = (P - cellGiOrigin) / cellGiVoxel.xxx;
    float2 xy = g.xy / cellGiDims.xy;
    float z = clamp(g.z, 0.5f, cellGiDims.z - 0.5f);
    float depth = float(int3(spvTextureSize(cellGi, uint(0), dummy_parameter)).z);
    float4 plain[6];
    float4 r[6];
    for (int a_1 = 0; a_1 < 6; a_1++)
    {
        plain[a_1] = cellGi.Sample(_cellGi_sampler, float3(xy, (z + (float(a_1) * cellGiDims.z)) / depth));
        r[a_1] = plain[a_1];
    }
    bool airTerrHRet;
    if (cellGiRooms)
    {
        airTerrHRet = (cellPassRed & 8) == 0;
    }
    else
    {
        airTerrHRet = cellGiRooms;
    }
    if (airTerrHRet)
    {
        float3 gv = g - 0.5f.xxx;
        int3 c0 = int3(floor(gv));
        float3 f = gv - float3(c0);
        if ((volRed & 4194304) != 0)
        {
            float3 airTerrHPart = P;
            int2 L = cellRoomAt(airTerrHPart);
            if (L.x >= 0)
            {
                for (int a_2 = 0; a_2 < 6; a_2++)
                {
                    int3 volPhasePiRet2 = c0;
                    float3 volPhasePiRet = f;
                    int2 airRhoRet = L;
                    int airColRet = a_2;
                    float4 airTupSunRet3 = plain[a_2];
                    int3 airTupSunRet2 = dm;
                    r[a_2] = volRoomAxis(volPhasePiRet2, volPhasePiRet, airRhoRet, airColRet, airTupSunRet3, airTupSunRet2);
                }
            }
        }
        else
        {
            float3 gr = ((P - cellRoomsOrigin) / cellRoomsCell.xxx) - 0.5f.xxx;
            int3 r0 = int3(floor(gr));
            float3 fr = gr - float3(r0);
            float wAir = 0.0f;
            bool same = true;
            float wk[8];
            int2 Lk[8];
            for (int k = 0; k < 8; k++)
            {
                int3 o = int3(k & 1, (k >> 1) & 1, (k >> 2) & 1);
                int3 c = r0 + o;
                float3 t = lerp(1.0f.xxx - fr, fr, float3(o));
                wk[k] = (t.x * t.y) * t.z;
                bool airTupSunRet = any(bool3(c.x < int3(0, 0, 0).x, c.y < int3(0, 0, 0).y, c.z < int3(0, 0, 0).z));
                bool cond;
                if (!airTupSunRet)
                {
                    int3 airTupSunPick = int3(cellRoomsDims);
                    cond = any(bool3(c.x >= airTupSunPick.x, c.y >= airTupSunPick.y, c.z >= airTupSunPick.z));
                }
                else
                {
                    cond = airTupSunRet;
                }
                if (cond)
                {
                    Lk[k] = int2(-2, -2);
                }
                else
                {
                    float volRoomAxisRet2 = cellRooms.Load(int4(c, 0)).x;
                    Lk[k] = cellRoomUnpack(volRoomAxisRet2);
                }
                if (Lk[k].x == (-1))
                {
                    wk[k] = 0.0f;
                }
                wAir += wk[k];
                bool volRoomAxisRet = k > 0;
                bool cond5;
                if (volRoomAxisRet)
                {
                    cond5 = any(bool2(Lk[k].x != Lk[0].x, Lk[k].y != Lk[0].y));
                }
                else
                {
                    cond5 = volRoomAxisRet;
                }
                if (cond5)
                {
                    same = false;
                }
            }
            if (wAir > 0.0f)
            {
                if (same)
                {
                    if (Lk[0].x >= 0)
                    {
                        for (int a_3 = 0; a_3 < 6; a_3++)
                        {
                            int3 rPart6 = c0;
                            float3 rPart5 = f;
                            int2 rPart4 = Lk[0];
                            int rPart3 = a_3;
                            float4 rPart2 = plain[a_3];
                            int3 rPart = dm;
                            r[a_3] = volRoomAxis(rPart6, rPart5, rPart4, rPart3, rPart2, rPart);
                        }
                    }
                }
                else
                {
                    for (int a_4 = 0; a_4 < 6; a_4++)
                    {
                        r[a_4] = 0.0f.xxxx;
                    }
                    float wlPick;
                    float4 rPick;
                    for (int k_1 = 0; k_1 < 8; k_1++)
                    {
                        if (wk[k_1] <= 0.0f)
                        {
                            continue;
                        }
                        bool seen = false;
                        for (int j = 0; j < k_1; j++)
                        {
                            bool seenPick;
                            if (!seen)
                            {
                                bool cond4 = wk[j] > 0.0f;
                                bool seenPick2;
                                if (cond4)
                                {
                                    seenPick2 = all(bool2(Lk[j].x == Lk[k_1].x, Lk[j].y == Lk[k_1].y));
                                }
                                else
                                {
                                    seenPick2 = cond4;
                                }
                                seenPick = seenPick2;
                            }
                            else
                            {
                                seenPick = seen;
                            }
                            seen = seenPick;
                        }
                        if (seen)
                        {
                            continue;
                        }
                        float wl = 0.0f;
                        for (int j_1 = k_1; j_1 < 8; j_1++)
                        {
                            if (all(bool2(Lk[j_1].x == Lk[k_1].x, Lk[j_1].y == Lk[k_1].y)))
                            {
                                wlPick = wk[j_1];
                            }
                            else
                            {
                                wlPick = 0.0f;
                            }
                            wl += wlPick;
                        }
                        wl /= wAir;
                        for (int a_5 = 0; a_5 < 6; a_5++)
                        {
                            if (Lk[k_1].x >= 0)
                            {
                                int3 rPart12 = c0;
                                float3 rPart11 = f;
                                int2 rPart10 = Lk[k_1];
                                int rPart9 = a_5;
                                float4 rPart8 = plain[a_5];
                                int3 rPart7 = dm;
                                rPick = volRoomAxis(rPart12, rPart11, rPart10, rPart9, rPart8, rPart7);
                            }
                            else
                            {
                                rPick = plain[a_5];
                            }
                            r[a_5] += (rPick * wl);
                        }
                    }
                }
            }
        }
    }
    bool cond3 = volShell > 0.0f;
    bool cond2;
    if (cond3)
    {
        cond2 = cellGiSky && false;
    }
    else
    {
        cond2 = cond3;
    }
    if (cond2)
    {
        float4 wide[6];
        for (int a_6 = 0; a_6 < 6; a_6++)
        {
            wide[a_6] = 0.0f.xxxx;
        }
        for (int k_2 = 0; k_2 < 8; k_2++)
        {
            float3 q = g + (((float3(float(k_2 & 1), float((k_2 >> 1) & 1), float((k_2 >> 2) & 1)) * 2.0f) - 1.0f.xxx) * volShell);
            float qz = clamp(q.z, 0.5f, cellGiDims.z - 0.5f);
            for (int a_7 = 0; a_7 < 6; a_7++)
            {
                wide[a_7] += (cellGi.Sample(_cellGi_sampler, float3(q.xy / cellGiDims.xy, (qz + (float(a_7) * cellGiDims.z)) / depth)) * 0.125f);
            }
        }
        float3 vPick;
        float3 vPick2;
        for (int a_8 = 0; a_8 < 6; a_8++)
        {
            if (r[a_8].w > 0.00999999977648258209228515625f)
            {
                vPick = r[a_8].xyz / r[a_8].w.xxx;
            }
            else
            {
                if (wide[a_8].w > 9.9999997473787516355514526367188e-05f)
                {
                    vPick2 = wide[a_8].xyz / wide[a_8].w.xxx;
                }
                else
                {
                    vPick2 = 0.0f.xxx;
                }
                vPick = vPick2;
            }
            float3 v = vPick;
            share[a_8] = clamp(wide[a_8].w, 0.0f, 1.0f);
            E[a_8] = max(v * share[a_8], 0.0f.xxx);
        }
        return;
    }
    float3 EPick;
    float3 EPick2;
    for (int a_9 = 0; a_9 < 6; a_9++)
    {
        if (cellGiSky && false)
        {
            EPick = max(r[a_9].xyz, 0.0f.xxx);
        }
        else
        {
            if (r[a_9].w > 0.00999999977648258209228515625f)
            {
                EPick2 = max(r[a_9].xyz / r[a_9].w.xxx, 0.0f.xxx);
            }
            else
            {
                EPick2 = 0.0f.xxx;
            }
            EPick = EPick2;
        }
        E[a_9] = EPick;
        share[a_9] = clamp(plain[a_9].w, 0.0f, 1.0f);
    }
}
#endif

#if defined(WW_FN_volCubeAll_V2) || defined(WW_FN_ALL)
void volCubeAll(float3 P, inout float3 E[6], inout float share[6])
{
    if (cellGiAirOn)
    {
        float3 ga = (P - cellGiAirOrigin) / cellGiAirVoxel.xxx;
        float za = clamp(ga.z, 0.5f, cellGiAirDims.z - 0.5f);
        for (int a = 0; a < 6; a++)
        {
            E[a] = max(cellGiAir.SampleLevel(_cellGiAir_sampler, float3(ga.xy / cellGiAirDims.xy, (za + (float(a) * cellGiAirDims.z)) / (6.0f * cellGiAirDims.z)), 0.0f).xyz, 0.0f.xxx);
            share[a] = 1.0f;
        }
        return;
    }
    int3 dm = int3(cellGiDims);
    float3 g = (P - cellGiOrigin) / cellGiVoxel.xxx;
    float2 xy = g.xy / cellGiDims.xy;
    float z = clamp(g.z, 0.5f, cellGiDims.z - 0.5f);
    float depth = float(int3(spvTextureSize(cellGi, uint(0), dummy_parameter)).z);
    float4 plain[6];
    float4 r[6];
    for (int a_1 = 0; a_1 < 6; a_1++)
    {
        plain[a_1] = cellGi.SampleLevel(_cellGi_sampler, float3(xy, (z + (float(a_1) * cellGiDims.z)) / depth), 0.0f);
        r[a_1] = plain[a_1];
    }
    bool airTerrHRet;
    if (cellGiRooms)
    {
        airTerrHRet = (cellPassRed & 8) == 0;
    }
    else
    {
        airTerrHRet = cellGiRooms;
    }
    if (airTerrHRet)
    {
        float3 gv = g - 0.5f.xxx;
        int3 c0 = int3(floor(gv));
        float3 f = gv - float3(c0);
        if ((volRed & 4194304) != 0)
        {
            float3 airTerrHPart = P;
            int2 L = cellRoomAt(airTerrHPart);
            if (L.x >= 0)
            {
                for (int a_2 = 0; a_2 < 6; a_2++)
                {
                    int3 volPhasePiRet2 = c0;
                    float3 volPhasePiRet = f;
                    int2 airRhoRet = L;
                    int airColRet = a_2;
                    float4 airTupSunRet3 = plain[a_2];
                    int3 airTupSunRet2 = dm;
                    r[a_2] = volRoomAxis(volPhasePiRet2, volPhasePiRet, airRhoRet, airColRet, airTupSunRet3, airTupSunRet2);
                }
            }
        }
        else
        {
            float3 gr = ((P - cellRoomsOrigin) / cellRoomsCell.xxx) - 0.5f.xxx;
            int3 r0 = int3(floor(gr));
            float3 fr = gr - float3(r0);
            float wAir = 0.0f;
            bool same = true;
            float wk[8];
            int2 Lk[8];
            for (int k = 0; k < 8; k++)
            {
                int3 o = int3(k & 1, (k >> 1) & 1, (k >> 2) & 1);
                int3 c = r0 + o;
                float3 t = lerp(1.0f.xxx - fr, fr, float3(o));
                wk[k] = (t.x * t.y) * t.z;
                bool airTupSunRet = any(bool3(c.x < int3(0, 0, 0).x, c.y < int3(0, 0, 0).y, c.z < int3(0, 0, 0).z));
                bool cond;
                if (!airTupSunRet)
                {
                    int3 airTupSunPick = int3(cellRoomsDims);
                    cond = any(bool3(c.x >= airTupSunPick.x, c.y >= airTupSunPick.y, c.z >= airTupSunPick.z));
                }
                else
                {
                    cond = airTupSunRet;
                }
                if (cond)
                {
                    Lk[k] = int2(-2, -2);
                }
                else
                {
                    float volRoomAxisRet2 = cellRooms.Load(int4(c, 0)).x;
                    Lk[k] = cellRoomUnpack(volRoomAxisRet2);
                }
                if (Lk[k].x == (-1))
                {
                    wk[k] = 0.0f;
                }
                wAir += wk[k];
                bool volRoomAxisRet = k > 0;
                bool cond5;
                if (volRoomAxisRet)
                {
                    cond5 = any(bool2(Lk[k].x != Lk[0].x, Lk[k].y != Lk[0].y));
                }
                else
                {
                    cond5 = volRoomAxisRet;
                }
                if (cond5)
                {
                    same = false;
                }
            }
            if (wAir > 0.0f)
            {
                if (same)
                {
                    if (Lk[0].x >= 0)
                    {
                        for (int a_3 = 0; a_3 < 6; a_3++)
                        {
                            int3 rPart6 = c0;
                            float3 rPart5 = f;
                            int2 rPart4 = Lk[0];
                            int rPart3 = a_3;
                            float4 rPart2 = plain[a_3];
                            int3 rPart = dm;
                            r[a_3] = volRoomAxis(rPart6, rPart5, rPart4, rPart3, rPart2, rPart);
                        }
                    }
                }
                else
                {
                    for (int a_4 = 0; a_4 < 6; a_4++)
                    {
                        r[a_4] = 0.0f.xxxx;
                    }
                    float wlPick;
                    float4 rPick;
                    for (int k_1 = 0; k_1 < 8; k_1++)
                    {
                        if (wk[k_1] <= 0.0f)
                        {
                            continue;
                        }
                        bool seen = false;
                        for (int j = 0; j < k_1; j++)
                        {
                            bool seenPick;
                            if (!seen)
                            {
                                bool cond4 = wk[j] > 0.0f;
                                bool seenPick2;
                                if (cond4)
                                {
                                    seenPick2 = all(bool2(Lk[j].x == Lk[k_1].x, Lk[j].y == Lk[k_1].y));
                                }
                                else
                                {
                                    seenPick2 = cond4;
                                }
                                seenPick = seenPick2;
                            }
                            else
                            {
                                seenPick = seen;
                            }
                            seen = seenPick;
                        }
                        if (seen)
                        {
                            continue;
                        }
                        float wl = 0.0f;
                        for (int j_1 = k_1; j_1 < 8; j_1++)
                        {
                            if (all(bool2(Lk[j_1].x == Lk[k_1].x, Lk[j_1].y == Lk[k_1].y)))
                            {
                                wlPick = wk[j_1];
                            }
                            else
                            {
                                wlPick = 0.0f;
                            }
                            wl += wlPick;
                        }
                        wl /= wAir;
                        for (int a_5 = 0; a_5 < 6; a_5++)
                        {
                            if (Lk[k_1].x >= 0)
                            {
                                int3 rPart12 = c0;
                                float3 rPart11 = f;
                                int2 rPart10 = Lk[k_1];
                                int rPart9 = a_5;
                                float4 rPart8 = plain[a_5];
                                int3 rPart7 = dm;
                                rPick = volRoomAxis(rPart12, rPart11, rPart10, rPart9, rPart8, rPart7);
                            }
                            else
                            {
                                rPick = plain[a_5];
                            }
                            r[a_5] += (rPick * wl);
                        }
                    }
                }
            }
        }
    }
    bool cond3 = volShell > 0.0f;
    bool cond2;
    if (cond3)
    {
        cond2 = cellGiSky && false;
    }
    else
    {
        cond2 = cond3;
    }
    if (cond2)
    {
        float4 wide[6];
        for (int a_6 = 0; a_6 < 6; a_6++)
        {
            wide[a_6] = 0.0f.xxxx;
        }
        for (int k_2 = 0; k_2 < 8; k_2++)
        {
            float3 q = g + (((float3(float(k_2 & 1), float((k_2 >> 1) & 1), float((k_2 >> 2) & 1)) * 2.0f) - 1.0f.xxx) * volShell);
            float qz = clamp(q.z, 0.5f, cellGiDims.z - 0.5f);
            for (int a_7 = 0; a_7 < 6; a_7++)
            {
                wide[a_7] += (cellGi.SampleLevel(_cellGi_sampler, float3(q.xy / cellGiDims.xy, (qz + (float(a_7) * cellGiDims.z)) / depth), 0.0f) * 0.125f);
            }
        }
        float3 vPick;
        float3 vPick2;
        for (int a_8 = 0; a_8 < 6; a_8++)
        {
            if (r[a_8].w > 0.00999999977648258209228515625f)
            {
                vPick = r[a_8].xyz / r[a_8].w.xxx;
            }
            else
            {
                if (wide[a_8].w > 9.9999997473787516355514526367188e-05f)
                {
                    vPick2 = wide[a_8].xyz / wide[a_8].w.xxx;
                }
                else
                {
                    vPick2 = 0.0f.xxx;
                }
                vPick = vPick2;
            }
            float3 v = vPick;
            share[a_8] = clamp(wide[a_8].w, 0.0f, 1.0f);
            E[a_8] = max(v * share[a_8], 0.0f.xxx);
        }
        return;
    }
    float3 EPick;
    float3 EPick2;
    for (int a_9 = 0; a_9 < 6; a_9++)
    {
        if (cellGiSky && false)
        {
            EPick = max(r[a_9].xyz, 0.0f.xxx);
        }
        else
        {
            if (r[a_9].w > 0.00999999977648258209228515625f)
            {
                EPick2 = max(r[a_9].xyz / r[a_9].w.xxx, 0.0f.xxx);
            }
            else
            {
                EPick2 = 0.0f.xxx;
            }
            EPick = EPick2;
        }
        E[a_9] = EPick;
        share[a_9] = clamp(plain[a_9].w, 0.0f, 1.0f);
    }
}
#endif

#if defined(WW_FN_volCubeScatter) || defined(WW_FN_ALL)
float3 volCubeScatter(float3 E[6], float3 dW)
{
    float3 fl = (((((E[0] + E[1]) + E[2]) + E[3]) + E[4]) + E[5]) * 0.666666686534881591796875f;
    float3 l1 = (((E[0] - E[1]) * dW.x) + ((E[2] - E[3]) * dW.y)) + ((E[4] - E[5]) * dW.z);
    return (max(fl + (l1 * (3.0f * volGi.w)), 0.0f.xxx) * 0.25f) * volGi.xyz;
}
#endif

#if defined(WW_FN_airAmbKeep) || defined(WW_FN_ALL)
float airAmbKeep(float fade, float r)
{
    float airTerrHRet;
    if (r >= 0.0f)
    {
        airTerrHRet = (1.0f - fade) + (fade * r);
    }
    else
    {
        airTerrHRet = 1.0f - fade;
    }
    return airTerrHRet;
}
#endif

#if defined(WW_FN_airEnvColour) || defined(WW_FN_ALL)
float3 airEnvColour(float3 J, float t, float z, float3 dirView)
{
    if ((volRed & 256) != 0)
    {
        return J;
    }
    float airTerrHRet = t;
    float airTerrHPart = z;
    float3 volPhasePiRet2 = dirView;
    bool volPhasePiRet;
    float3 airRhoRet = wwAirRecordColour(airTerrHRet, airTerrHPart, volPhasePiRet2, volPhasePiRet);
    bool ok = volPhasePiRet;
    float3 c = airRhoRet;
    float3 airColRet;
    if (ok)
    {
        airColRet = c * dot(J, float3(0.2125999927520751953125f, 0.715200006961822509765625f, 0.072200000286102294921875f));
    }
    else
    {
        airColRet = J;
    }
    return airColRet;
}
#endif
#endif	// WW_HLSL_SRC
