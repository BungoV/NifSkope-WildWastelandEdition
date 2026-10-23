// lane ATMO1: the physical sky's medium and LUT mappings (Hillaire 2020; Bruneton 2017 for the transmittance
// mapping). km units, sun irradiance 1 at the top of the atmosphere. tests/spells/atmo_model.py is the same math.

const float kPi = 3.14159265;
const float kRb = 6360.0;
const float kRt = 6460.0;

uniform vec3 atRay;	// Rayleigh scattering /km (5.802, 13.558, 33.1)e-3
uniform float atMieS;	// Mie scattering /km x the haze scale
uniform float atMieE;	// Mie extinction /km x the haze scale
uniform vec3 atOzone;	// ozone absorption /km (0.650, 1.881, 0.085)e-3; 0 under the red "noozone"
uniform float atG;	// Mie Henyey-Greenstein g

const float kRayH = 8.0;
const float kMieH = 1.2;
const vec3 kAlbedo = vec3( 0.3 );

void atMedium( float h, out vec3 rayS, out vec3 mieS, out vec3 ext )
{
	h = max( h, 0.0 );
	float dr = exp( -h / kRayH );
	float dm = exp( -h / kMieH );
	float dz = max( 0.0, 1.0 - abs( h - 25.0 ) / 15.0 );
	rayS = atRay * dr;
	mieS = vec3( atMieS * dm );
	ext = rayS + vec3( atMieE * dm ) + atOzone * dz;
}

float atDistTop( float r, float mu )
{
	return max( 0.0, -r * mu + sqrt( max( 0.0, r * r * ( mu * mu - 1.0 ) + kRt * kRt ) ) );
}

float atDistGround( float r, float mu )
{
	return max( 0.0, -r * mu - sqrt( max( 0.0, r * r * ( mu * mu - 1.0 ) + kRb * kRb ) ) );
}

bool atHitsGround( float r, float mu )
{
	return mu < 0.0 && r * r * ( mu * mu - 1.0 ) + kRb * kRb >= 0.0;
}

// texel centers sit on the [0,1] ends of the parameter range
float atUnitToSub( float u, float n ) { return 0.5 / n + u * ( 1.0 - 1.0 / n ); }
float atSubToUnit( float u, float n ) { return ( u - 0.5 / n ) / ( 1.0 - 1.0 / n ); }

const vec2 kTransSize = vec2( 256.0, 64.0 );

vec2 atTransUv( float r, float mu )
{
	float H = sqrt( kRt * kRt - kRb * kRb );
	float rho = sqrt( max( 0.0, r * r - kRb * kRb ) );
	float d = atDistTop( r, mu );
	float dmin = kRt - r, dmax = rho + H;
	return vec2( atUnitToSub( ( d - dmin ) / max( dmax - dmin, 1e-6 ), kTransSize.x ),
		atUnitToSub( rho / H, kTransSize.y ) );
}

void atTransRmu( vec2 uv, out float r, out float mu )
{
	float xm = atSubToUnit( uv.x, kTransSize.x ), xr = atSubToUnit( uv.y, kTransSize.y );
	float H = sqrt( kRt * kRt - kRb * kRb );
	float rho = H * xr;
	r = sqrt( rho * rho + kRb * kRb );
	float dmin = kRt - r, dmax = rho + H;
	float d = dmin + xm * ( dmax - dmin );
	mu = d == 0.0 ? 1.0 : clamp( ( H * H - rho * rho - d * d ) / ( 2.0 * r * d ), -1.0, 1.0 );
}

vec3 atTrans( sampler2D T, float r, float mu )
{
	if ( atHitsGround( r, mu ) )
		return vec3( 0.0 );
	return texture( T, atTransUv( r, mu ) ).rgb;
}

// the multiple-scattering LUT: x = cos sun zenith, y = height
vec2 atMsUv( float r, float mus, vec2 n )
{
	return vec2( atUnitToSub( clamp( mus * 0.5 + 0.5, 0.0, 1.0 ), n.x ),
		atUnitToSub( clamp( ( r - kRb ) / ( kRt - kRb ), 0.0, 1.0 ), n.y ) );
}

float atPhaseR( float c ) { return 3.0 / ( 16.0 * kPi ) * ( 1.0 + c * c ); }
float atPhaseHg( float g, float c ) { return ( 1.0 - g * g ) / ( 4.0 * kPi * pow( max( 1e-4, 1.0 + g * g - 2.0 * g * c ), 1.5 ) ); }

// the sky-view LUT (Hillaire 2020 sec. 5.3): latitude squeezed toward the horizon, longitude = angle to the sun
void atSkyUvToDir( vec2 uv, vec2 n, float r, out float cosZ, out float cosAz )
{
	uv = vec2( atSubToUnit( uv.x, n.x ), atSubToUnit( uv.y, n.y ) );
	float vh = sqrt( max( 0.0, r * r - kRb * kRb ) );
	float beta = acos( clamp( vh / r, -1.0, 1.0 ) );
	float zh = kPi - beta;
	float za;
	if ( uv.y < 0.5 ) {
		float c = 1.0 - 2.0 * uv.y;
		za = zh * ( 1.0 - c * c );
	} else {
		float c = uv.y * 2.0 - 1.0;
		za = zh + beta * c * c;
	}
	cosZ = cos( za );
	float c = uv.x * uv.x;
	cosAz = -( c * 2.0 - 1.0 );
}

vec2 atSkyDirToUv( float r, float cosZ, float cosAz, vec2 n )
{
	float vh = sqrt( max( 0.0, r * r - kRb * kRb ) );
	float beta = acos( clamp( vh / r, -1.0, 1.0 ) );
	float zh = kPi - beta;
	float za = acos( clamp( cosZ, -1.0, 1.0 ) );
	vec2 uv;
	if ( za < zh ) {
		float c = 1.0 - za / zh;
		uv.y = ( 1.0 - sqrt( max( 0.0, c ) ) ) * 0.5;
	} else {
		float c = ( za - zh ) / max( beta, 1e-5 );
		uv.y = sqrt( clamp( c, 0.0, 1.0 ) ) * 0.5 + 0.5;
	}
	uv.x = sqrt( clamp( -cosAz * 0.5 + 0.5, 0.0, 1.0 ) );
	return vec2( atUnitToSub( uv.x, n.x ), atUnitToSub( uv.y, n.y ) );
}
