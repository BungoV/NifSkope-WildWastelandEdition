/* Lane TERRLIVE1 (2026-09-29): the offscreen preview of the three LOD terrain
 * options. See terrainpreview.h for what it draws. Everything here is a
 * MEASURING tool: it reads the bake's files through their own readers and
 * prints numbers; nothing it does feeds back into a bake.
 *
 * THE HOOK FOR THE ANTI-REPEAT LANE (TILING6): the live splat's texture fetch
 * is ONE GLSL function, `ltexFetch( layer, uv, dx, dy )`, in kTerrainFs. An
 * anti-repeat scheme replaces that function's body (and may add uniforms);
 * the splat loop around it, the weights and the cross-fade stay as they are. */

#include "terrainpreview.h"

#include "esmdata.h"
#include "lodgen.h"
#include "lodtfile.h"
#include "io/loddecal.h"
#include "io/lodvfile.h"

#include "ddstxt16.hpp"

#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QFont>
#include <QGuiApplication>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMatrix4x4>
#include <QOffscreenSurface>
#include <QOpenGLContext>
#include <QOpenGLFunctions_4_3_Core>
#include <QOpenGLVersionFunctionsFactory>
#include <QPainter>
#include <QRegularExpression>
#include <QTextStream>
#include <QVector3D>
#include <QVector4D>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

namespace {

using GL = QOpenGLFunctions_4_3_Core;

QTextStream & sout()
{
	static QTextStream s( stdout );
	return s;
}

void say( const QString & line )
{
	sout() << "preview: " << line << Qt::endl;
	sout().flush();
}

enum Option { OptFull = 0, OptHybrid = 1, OptDynamic = 2, OptLive = 3, OptBaked = 4 };

bool parseOption( const QString & s, int * o )
{
	static const char * const N[] = { "full", "hybrid", "dynamic", "live", "baked" };
	for ( int i = 0; i < 5; i++ )
		if ( s == QLatin1String( N[i] ) ) {
			*o = i;
			return true;
		}
	return false;
}

QString optionName( int o )
{
	static const char * const N[] = { "full", "hybrid", "dynamic", "live", "baked" };
	return QString::fromLatin1( N[qBound( 0, o, 4 )] );
}

/* ---- shaders -------------------------------------------------------------- */

const char * const kTerrainVs = R"(#version 430 core
layout(location = 0) in vec2 aIJ;
uniform sampler2D uHeight;
uniform mat4 uViewProj;
uniform vec2 uOrigin;
uniform float uSpacing;
out vec3 vWorld;
void main()
{
	float z = texelFetch( uHeight, ivec2( aIJ ), 0 ).r;
	vWorld = vec3( uOrigin + aIJ * uSpacing, z );
	gl_Position = uViewProj * vec4( vWorld, 1.0 );
}
)";

const char * const kTerrainFs = R"(#version 430 core
in vec3 vWorld;
layout(location = 0) out vec4 oAlbedo;
layout(location = 1) out vec4 oNormal;
uniform sampler2D uHeight;
uniform usampler2D uIdx;
uniform sampler2D uW;
uniform sampler2D uVclr;
uniform sampler2DArray uLtex;
uniform sampler2D uFull;
uniform sampler2D uFar;
uniform vec2 uOrigin;
uniform float uSpacing;
uniform ivec2 uGrid;
uniform float uTile;
uniform vec4 uFullMap;
uniform vec4 uFarMap;
uniform int uOption;
uniform vec2 uFade;
uniform vec3 uEye;
// law 2 (lane TERRLIVE1): vanilla's dim-4 diffuse and our weight against it
uniform sampler2D uVan;
uniform sampler2D uBlendW;
uniform vec4 uVanMap;
uniform vec4 uBlendMap;
uniform int uLaw2;
// the rule paint outside (lane TERRLIVE1, `--outside-paint rule`): the .lodr's ids and weights
uniform usampler2D uRIdx;
uniform sampler2D uRW;
uniform sampler2D uRG;
uniform vec2 uROrigin;
uniform float uRSpacing;
uniform ivec2 uRGrid;
uniform int uRule;
// the AO map (lane TERRLIVE2, `<ws>.loda`): 32 u sky visibility over our painted ground, 1 elsewhere
uniform sampler2D uLoda;
uniform vec4 uLodaMap;
uniform int uLodaOn;

float lodaAo()
{
	if ( uLodaOn == 0 )
		return 1.0;
	return textureLod( uLoda, vec2( ( vWorld.x - uLodaMap.x ) * uLodaMap.z, ( uLodaMap.y - vWorld.y ) * uLodaMap.w ), 0.0 ).r;
}

// THE ANTI-REPEAT HOOK (lane TILING6 replaces this body): one LTEX layer at a
// world-tiled uv, with the caller's gradients.
vec3 ltexFetch( float layer, vec2 uv, vec2 dx, vec2 dy )
{
	return textureGrad( uLtex, vec3( uv, layer ), dx, dy ).rgb;
}

float liveWeight()
{
	if ( uOption == 2 || uOption == 3 )
		return 1.0;
	if ( uOption == 0 || uOption == 4 )
		return 0.0;
	float d = distance( vWorld, uEye );
	return 1.0 - smoothstep( uFade.x, uFade.y, d );
}

// one splat over an id/weight grid (the .lodl's, or the rule map's); `cover` = the
// bilinear weight of the samples that hold any texture
vec3 splatGrid( usampler2D tIdx, sampler2D tW, vec2 origin, float spacing, ivec2 grid, vec2 wdx, vec2 wdy,
	out vec2 g, out float cover )
{
	g = ( vWorld.xy - origin ) / spacing;
	vec2 g0 = floor( g );
	vec2 f = g - g0;
	vec2 uv = vWorld.xy / uTile;
	vec2 dx = wdx / uTile, dy = wdy / uTile;
	vec3 acc = vec3( 0.0 );
	cover = 0.0;
	for ( int c = 0; c < 4; c++ ) {
		ivec2 o = ivec2( c & 1, c >> 1 );
		ivec2 p = clamp( ivec2( g0 ) + o, ivec2( 0 ), grid - 1 );
		float bw = ( o.x == 1 ? f.x : 1.0 - f.x ) * ( o.y == 1 ? f.y : 1.0 - f.y );
		if ( bw <= 0.0 )
			continue;
		uvec4 id = texelFetch( tIdx, p, 0 );
		vec4 w = texelFetch( tW, p, 0 );
		if ( id.x != 65535u )
			cover += bw;
		for ( int k = 0; k < 4; k++ ) {
			if ( w[k] <= 0.0 )
				continue;
			vec3 t = id[k] == 65535u ? vec3( 0.5 ) : ltexFetch( float( id[k] ), uv, dx, dy );
			acc += bw * w[k] * t;
		}
	}
	return acc;
}

vec3 splat( vec2 wdx, vec2 wdy )
{
	vec2 g;
	float cover;
	vec3 acc = splatGrid( uIdx, uW, uOrigin, uSpacing, uGrid, wdx, wdy, g, cover );
	vec3 vc = textureLod( uVclr, ( g + 0.5 ) / vec2( uGrid ), 0.0 ).rgb;
	return acc * vc;
}

// the rule map's brightness gain (TERRLIVE2, .lodr v2 plane G, x128), bilinear over the samples
// that exist -- LodgenRuleMap::mixAt's weighting
float ruleGain( vec2 g )
{
	vec2 g0 = floor( g ), f = g - g0;
	float s = 0.0, t = 0.0;
	for ( int c = 0; c < 4; c++ ) {
		ivec2 o = ivec2( c & 1, c >> 1 );
		ivec2 p = clamp( ivec2( g0 ) + o, ivec2( 0 ), uRGrid - 1 );
		float bw = ( o.x == 1 ? f.x : 1.0 - f.x ) * ( o.y == 1 ? f.y : 1.0 - f.y );
		if ( bw <= 0.0 || texelFetch( uRIdx, p, 0 ).x == 65535u )
			continue;
		s += bw * texelFetch( uRG, p, 0 ).r;
		t += bw;
	}
	return t > 0.0 ? s / t * ( 255.0 / 128.0 ) : 1.0;
}

// the rule paint: the map's textures through the same hook, no vertex colour (the bake's rule);
// false where no rule sample reaches
bool ruleSplat( vec2 wdx, vec2 wdy, out vec3 R )
{
	vec2 g;
	float cover;
	R = splatGrid( uRIdx, uRW, uROrigin, uRSpacing, uRGrid, wdx, wdy, g, cover );
	if ( cover <= 0.0 )
		return false;
	R /= cover;
	R *= ruleGain( g );
	return true;
}

vec4 baked4( sampler2D s, vec4 m, vec2 wdx, vec2 wdy )
{
	vec2 uv = vec2( ( vWorld.x - m.x ) * m.z, ( m.y - vWorld.y ) * m.w );
	return textureGrad( s, uv, vec2( wdx.x * m.z, -wdx.y * m.w ), vec2( wdy.x * m.z, -wdy.y * m.w ) );
}

vec3 baked( sampler2D s, vec4 m, vec2 wdx, vec2 wdy )
{
	return baked4( s, m, wdx, wdy ).rgb;
}

// our weight against vanilla (0 = vanilla untouched, 1 = ours); a texel with no vanilla sheet stays ours
float oursWeight( vec2 wdx, vec2 wdy, out vec3 V )
{
	V = vec3( 0.0 );
	if ( uLaw2 == 0 )
		return 1.0;
	vec4 v = baked4( uVan, uVanMap, wdx, wdy );
	V = v.rgb;
	float w = textureLod( uBlendW, vec2( ( vWorld.x - uBlendMap.x ) * uBlendMap.z, ( uBlendMap.y - vWorld.y ) * uBlendMap.w ), 0.0 ).r;
	// the rule paint takes vanilla's place in the same band, where it has a sample
	if ( uRule == 1 && w < 1.0 ) {
		vec3 R;
		if ( ruleSplat( wdx, wdy, R ) ) {
			V = R;
			return w;
		}
	}
	return 1.0 - ( 1.0 - w ) * v.a;
}

vec3 terrainNormal()
{
	vec2 g = ( vWorld.xy - uOrigin ) / uSpacing;
	vec2 e = 1.0 / vec2( uGrid );
	vec2 uvh = ( g + 0.5 ) * e;
	float hl = textureLod( uHeight, uvh - vec2( e.x, 0.0 ), 0.0 ).r;
	float hr = textureLod( uHeight, uvh + vec2( e.x, 0.0 ), 0.0 ).r;
	float hd = textureLod( uHeight, uvh - vec2( 0.0, e.y ), 0.0 ).r;
	float hu = textureLod( uHeight, uvh + vec2( 0.0, e.y ), 0.0 ).r;
	return normalize( vec3( ( hl - hr ) / ( 2.0 * uSpacing ), ( hd - hu ) / ( 2.0 * uSpacing ), 1.0 ) );
}

void main()
{
	vec2 wdx = dFdx( vWorld.xy ), wdy = dFdy( vWorld.xy );
	float wl = liveWeight();
	vec3 col;
	if ( uOption == 0 )
		col = baked( uFull, uFullMap, wdx, wdy );
	else if ( uOption == 4 )
		col = baked( uFar, uFarMap, wdx, wdy );
	else {
		vec3 L = vec3( 0.0 ), B = vec3( 0.0 );
		if ( wl > 0.0 ) {
			vec3 V;
			float wo = oursWeight( wdx, wdy, V );
			L = wo > 0.0 ? splat( wdx, wdy ) * lodaAo() : vec3( 0.0 );
			L = mix( V, L, wo );
		}
		if ( wl < 1.0 )
			B = baked( uFar, uFarMap, wdx, wdy );
		col = mix( B, L, wl );
	}
	oAlbedo = vec4( col, 1.0 );
	oNormal = vec4( terrainNormal() * 0.5 + 0.5, 1.0 );
}
)";

// shared by the decal pass and the box-count pass
const char * const kDecalCommon = R"(#version 430 core
struct Rec { vec4 posScale; vec4 quat; vec4 box0; vec4 box1; vec4 atlas; };
layout(std430, binding = 0) readonly buffer Recs { Rec recs[]; };
vec3 qrot( vec4 q, vec3 v ) { return v + 2.0 * cross( q.xyz, cross( q.xyz, v ) + q.w * v ); }
)";

const char * const kDecalVs = R"(
uniform mat4 uViewProj;
flat out int vRec;
const int kIdx[36] = int[36]( 0,2,1, 1,2,3, 4,5,6, 5,7,6, 0,1,4, 1,5,4, 2,6,3, 3,6,7, 0,4,2, 2,4,6, 1,3,5, 3,7,5 );
void main()
{
	Rec r = recs[gl_InstanceID];
	int c = kIdx[gl_VertexID];
	vec3 t = vec3( float( c & 1 ), float( ( c >> 1 ) & 1 ), float( ( c >> 2 ) & 1 ) );
	vec3 l = vec3( mix( r.box0.x, r.box0.z, t.x ), mix( r.box0.y, r.box0.w, t.y ), mix( r.box1.x, r.box1.y, t.z ) );
	vec3 w = r.posScale.xyz + qrot( r.quat, l * r.posScale.w );
	vRec = gl_InstanceID;
	gl_Position = uViewProj * vec4( w, 1.0 );
}
)";

const char * const kDecalFsHead = R"(
flat in int vRec;
uniform sampler2D uDepth;
uniform mat4 uInvViewProj;
uniform vec2 uViewport;
uniform vec3 uEye;
uniform vec2 uFade;
uniform int uOption;
uniform int uOrtho;
uniform float uPixelK;
bool unproject( out vec3 w, out vec3 l, out Rec r )
{
	r = recs[vRec];
	float d = texelFetch( uDepth, ivec2( gl_FragCoord.xy ), 0 ).r;
	if ( d >= 1.0 )
		return false;
	vec4 ndc = vec4( gl_FragCoord.xy / uViewport * 2.0 - 1.0, d * 2.0 - 1.0, 1.0 );
	vec4 wp = uInvViewProj * ndc;
	w = wp.xyz / wp.w;
	vec4 qi = vec4( -r.quat.xyz, r.quat.w );
	l = qrot( qi, w - r.posScale.xyz ) / r.posScale.w;
	return l.x >= r.box0.x && l.x <= r.box0.z && l.y >= r.box0.y && l.y <= r.box0.w
		&& l.z >= r.box1.x && l.z <= r.box1.y;
}
)";

const char * const kDecalFs = R"(
uniform sampler2D uAtlasC;
uniform sampler2D uAtlasN;
uniform float uMaxLod;
uniform int uDecalNormals;
layout(location = 0) out vec4 oAlbedo;
layout(location = 1) out vec4 oNormal;
void main()
{
	vec3 w, l;
	Rec r;
	if ( !unproject( w, l, r ) )
		discard;
	vec2 uv = vec2( ( l.x - r.box0.x ) / ( r.box0.z - r.box0.x ), ( r.box0.w - l.y ) / ( r.box0.w - r.box0.y ) );
	vec2 a = mix( r.atlas.xy, r.atlas.zw, uv );
	float pix = uOrtho != 0 ? uPixelK : uPixelK * distance( w, uEye );
	float lod = clamp( log2( max( 1.0, pix / ( r.box1.z * r.posScale.w ) ) ), 0.0, uMaxLod );
	vec4 c = textureLod( uAtlasC, a, lod );
	vec4 n = textureLod( uAtlasN, a, lod );
	float wgt = 1.0;
	if ( uOption == 1 )
		wgt = 1.0 - smoothstep( uFade.x, uFade.y, distance( w, uEye ) );
	oAlbedo = vec4( c.rgb, c.a * wgt );
	vec3 nw = normalize( qrot( r.quat, n.rgb * 2.0 - 1.0 ) );
	oNormal = vec4( nw * 0.5 + 0.5, uDecalNormals != 0 ? n.a * c.a * wgt : 0.0 );
}
)";

const char * const kCountFs = R"(
layout(location = 0) out vec4 oCount;
void main()
{
	vec3 w, l;
	Rec r;
	bool inside = unproject( w, l, r );
	oCount = vec4( 1.0, inside ? 1.0 : 0.0, 0.0, 1.0 );
}
)";

const char * const kLightVs = R"(#version 430 core
out vec2 vUv;
void main()
{
	vec2 p = vec2( float( ( gl_VertexID << 1 ) & 2 ), float( gl_VertexID & 2 ) );
	vUv = p;
	gl_Position = vec4( p * 2.0 - 1.0, 0.0, 1.0 );
}
)";

const char * const kLightFs = R"(#version 430 core
in vec2 vUv;
layout(location = 0) out vec4 oColour;
uniform sampler2D uAlbedo;
uniform sampler2D uNormal;
uniform sampler2D uDepth;
uniform sampler2D uAo;
uniform mat4 uInvViewProj;
uniform vec4 uAoMap;
uniform float uAoLod;
uniform int uAoMode;
void main()
{
	ivec2 p = ivec2( gl_FragCoord.xy );
	float d = texelFetch( uDepth, p, 0 ).r;
	if ( d >= 1.0 ) {
		oColour = vec4( 0.55, 0.64, 0.78, 1.0 );
		return;
	}
	vec3 a = texelFetch( uAlbedo, p, 0 ).rgb;
	vec3 n = normalize( texelFetch( uNormal, p, 0 ).rgb * 2.0 - 1.0 );
	vec3 L = normalize( vec3( 0.35, 0.45, 0.82 ) );
	float ao = 1.0;
	if ( uAoMode != 0 ) {
		vec4 ndc = vec4( vUv * 2.0 - 1.0, d * 2.0 - 1.0, 1.0 );
		vec4 wp = uInvViewProj * ndc;
		vec3 w = wp.xyz / wp.w;
		vec2 uv = vec2( ( w.x - uAoMap.x ) * uAoMap.z, ( uAoMap.y - w.y ) * uAoMap.w );
		ao = textureLod( uAo, uv, uAoLod ).r;
	}
	oColour = vec4( a * ( 0.45 + 0.75 * max( 0.0, dot( n, L ) ) ) * ao, 1.0 );
}
)";

/* ---- GL helpers ----------------------------------------------------------- */

GLuint compile( GL * gl, GLenum type, const QByteArray & src, QString * why )
{
	const GLuint s = gl->glCreateShader( type );
	const char * p = src.constData();
	gl->glShaderSource( s, 1, &p, nullptr );
	gl->glCompileShader( s );
	GLint ok = 0;
	gl->glGetShaderiv( s, GL_COMPILE_STATUS, &ok );
	if ( !ok ) {
		char log[4096] = {};
		gl->glGetShaderInfoLog( s, sizeof( log ) - 1, nullptr, log );
		*why = QString( "shader compile: %1" ).arg( QString::fromLatin1( log ).simplified() );
		gl->glDeleteShader( s );
		return 0;
	}
	return s;
}

GLuint program( GL * gl, const QByteArray & vs, const QByteArray & fs, QString * why )
{
	const GLuint v = compile( gl, GL_VERTEX_SHADER, vs, why );
	if ( !v )
		return 0;
	const GLuint f = compile( gl, GL_FRAGMENT_SHADER, fs, why );
	if ( !f )
		return 0;
	const GLuint p = gl->glCreateProgram();
	gl->glAttachShader( p, v );
	gl->glAttachShader( p, f );
	gl->glLinkProgram( p );
	gl->glDeleteShader( v );
	gl->glDeleteShader( f );
	GLint ok = 0;
	gl->glGetProgramiv( p, GL_LINK_STATUS, &ok );
	if ( !ok ) {
		char log[4096] = {};
		gl->glGetProgramInfoLog( p, sizeof( log ) - 1, nullptr, log );
		*why = QString( "program link: %1" ).arg( QString::fromLatin1( log ).simplified() );
		return 0;
	}
	return p;
}

GLint U( GL * gl, GLuint p, const char * n )
{
	return gl->glGetUniformLocation( p, n );
}

//! One decoded sheet of one .lodt level, and its world mapping.
struct Sheet
{
	std::vector<quint32> img;   //!< 0xAARRGGBB, row 0 north
	int w = 0, h = 0;
	float map[4] = { 0, 0, 1, 1 }; //!< x west, y north edge, 1/spanX, 1/spanY (world units)
	float texelUnits = 0.0f;
	int tiles = 0;
	qint64 fileBytes = 0;
};

/* Decode sheet `role` of a .lodt level into one image: each present tile's
 * first stored mip goes through the DRIVER's block decoder (BC1/BC3/BC7 alike)
 * and its content square is copied into place. */
bool decodeSheet( GL * gl, const QString & path, int role, Sheet & out, QString * why )
{
	LodvHeaderFields h;
	std::vector<LodvTileEntry> table;
	if ( !lodvValidate( path, &h, &table, false, why ) )
		return false;
	if ( h.compression != 0 ) {
		*why = QStringLiteral( "the container is zlib-compressed; the preview reads uncompressed ones" );
		return false;
	}
	int sh = -1;
	for ( int s = 0; s < h.sheetCount; s++ )
		if ( h.sheets[s].role == role ) {
			sh = s;
			break;
		}
	if ( sh < 0 ) {
		*why = QString( "%1 has no sheet of role %2" ).arg( QFileInfo( path ).fileName() ).arg( role );
		return false;
	}
	const int skip = h.sheets[sh].mipSkip;
	const int C = h.contentTexels >> skip, B = h.borderTexels >> skip, S = lodvSheetSide( h, sh, 0 );
	out.w = h.tilesX * C;
	out.h = h.tilesY * C;
	GLint maxTex = 0;
	gl->glGetIntegerv( GL_MAX_TEXTURE_SIZE, &maxTex );
	if ( out.w > maxTex || out.h > maxTex ) {
		*why = QString( "%1 x %2 texels is over the driver's %3" ).arg( out.w ).arg( out.h ).arg( maxTex );
		return false;
	}
	out.img.assign( size_t( out.w ) * size_t( out.h ), 0xFF808080u );
	QFile f( path );
	if ( !f.open( QIODevice::ReadOnly ) ) {
		*why = QStringLiteral( "cannot open " ) + path;
		return false;
	}
	GLuint t = 0;
	gl->glGenTextures( 1, &t );
	gl->glBindTexture( GL_TEXTURE_2D, t );
	gl->glPixelStorei( GL_PACK_ALIGNMENT, 4 );
	gl->glPixelStorei( GL_UNPACK_ALIGNMENT, 4 );
	std::vector<quint32> tile( size_t( S ) * size_t( S ) );
	out.tiles = 0;
	for ( int ty = 0; ty < h.tilesY; ty++ )
		for ( int tx = 0; tx < h.tilesX; tx++ ) {
			const LodvTileEntry & e = table[size_t( ty ) * h.tilesX + tx];
			if ( !e.offset )
				continue;
			const bool cover = ( e.flags & LODV_TILE_COVER ) != 0;
			QByteArray bytes;
			if ( !lodvReadSheetMip( f, h, e, sh, 0, &bytes, why ) ) {
				gl->glDeleteTextures( 1, &t );
				return false;
			}
			const int dx = cover ? h.sheets[sh].dxgiFormatCover : h.sheets[sh].dxgiFormat;
			GLenum fmt = 0;
			switch ( dx ) {
			case 71: case 72: fmt = 0x83F0; break;          // BC1 as RGB
			case 77: case 78: fmt = 0x83F3; break;          // BC3
			case 80: fmt = 0x8DBB; break;                   // BC4
			case 83: fmt = 0x8DBD; break;                   // BC5
			case 98: case 99: fmt = 0x8E8C; break;          // BC7 (raw bytes, no sRGB decode)
			default: break;
			}
			if ( fmt )
				gl->glCompressedTexImage2D( GL_TEXTURE_2D, 0, fmt, S, S, 0, GLsizei( bytes.size() ), bytes.constData() );
			else if ( dx == 28 || dx == 29 )
				gl->glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA8, S, S, 0, GL_RGBA, GL_UNSIGNED_BYTE, bytes.constData() );
			else {
				*why = QString( "sheet format DXGI %1 is not one the preview decodes" ).arg( dx );
				gl->glDeleteTextures( 1, &t );
				return false;
			}
			gl->glGetTexImage( GL_TEXTURE_2D, 0, GL_BGRA, GL_UNSIGNED_BYTE, tile.data() );
			for ( int y = 0; y < C; y++ )
				std::memcpy( &out.img[size_t( ty * C + y ) * out.w + size_t( tx ) * C],
					&tile[size_t( B + y ) * S + B], size_t( C ) * 4 );
			out.tiles++;
		}
	gl->glDeleteTextures( 1, &t );
	const float span = float( h.levelDim ) * 4096.0f;
	out.map[0] = float( h.west ) * 4096.0f;
	out.map[1] = float( h.north + 1 ) * 4096.0f;
	out.map[2] = 1.0f / ( float( h.tilesX ) * span );
	out.map[3] = 1.0f / ( float( h.tilesY ) * span );
	out.texelUnits = span / float( C );
	out.fileBytes = QFileInfo( path ).size();
	return true;
}

GLuint uploadRgba( GL * gl, const std::vector<quint32> & img, int w, int h, bool mips )
{
	GLuint t = 0;
	gl->glGenTextures( 1, &t );
	gl->glBindTexture( GL_TEXTURE_2D, t );
	gl->glPixelStorei( GL_UNPACK_ALIGNMENT, 4 );
	gl->glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_BGRA, GL_UNSIGNED_BYTE, img.data() );
	if ( mips )
		gl->glGenerateMipmap( GL_TEXTURE_2D );
	gl->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, mips ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR );
	gl->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
	gl->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
	gl->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
	if ( mips )
		gl->glTexParameterf( GL_TEXTURE_2D, 0x84FE /* GL_TEXTURE_MAX_ANISOTROPY */, 8.0f );
	return t;
}

//! Print and clear any pending GL error; returns how many there were.
int drainErrors( GL * gl, const QString & where )
{
	int n = 0;
	for ( GLenum e = gl->glGetError(); e != GL_NO_ERROR && n < 16; e = gl->glGetError(), n++ )
		say( QString( "GL error 0x%1 after %2" ).arg( e, 0, 16 ).arg( where ) );
	return n;
}

struct DecalSet
{
	bool ok = false;
	LoddFile lodd;
	LodgFile lodg;
	GLuint atlasC = 0, atlasN = 0;
	int atlasW = 0, atlasH = 0, drop = 0;
	std::vector<float> atlasRect;   //!< per piece u0 v0 u1 v1
	std::vector<float> texel;       //!< per piece: local units a texel at the atlas level
	float maxLod = 3.0f;
};

bool loadDecals( GL * gl, const QString & loddPath, DecalSet & ds, QString * why )
{
	QString lodgPath = loddPath;
	lodgPath.replace( QRegularExpression( QStringLiteral( "\\.lodd$" ), QRegularExpression::CaseInsensitiveOption ),
		QStringLiteral( ".lodg" ) );
	if ( !ds.lodd.open( loddPath, why ) || !ds.lodg.open( lodgPath, why ) )
		return false;
	if ( ds.lodg.pieceCount() != ds.lodd.pieceCount() || ds.lodg.loddTableCrc() != ds.lodd.tableCrc() ) {
		*why = QStringLiteral( "the .lodg does not pair with the .lodd (piece count or table CRC)" );
		return false;
	}
	GLint maxTex = 0;
	gl->glGetIntegerv( GL_MAX_TEXTURE_SIZE, &maxTex );
	const int AW = qMin( 4096, int( maxTex ) ), PAD = 4, n = ds.lodd.pieceCount();
	const size_t nn = size_t( n );
	std::vector<int> order( nn ), px( nn ), py( nn ), pw( nn ), ph( nn );
	for ( int i = 0; i < n; i++ )
		order[size_t( i )] = i;
	int drop = 0, AH = 0;
	for ( ;; drop++ ) {
		for ( int i = 0; i < n; i++ ) {
			const LoddPiece & p = ds.lodd.piece( i );
			const int m = qMin( drop, qMax( 0, p.mips - 1 ) );
			pw[size_t( i )] = qMax( 1, p.width >> m );
			ph[size_t( i )] = qMax( 1, p.height >> m );
		}
		std::stable_sort( order.begin(), order.end(), [&]( int a, int b ) { return ph[size_t( a )] > ph[size_t( b )]; } );
		int x = 0, y = 0, shelf = 0;
		for ( int i : order ) {
			if ( x + pw[size_t( i )] + PAD > AW ) {
				x = 0;
				y += shelf + PAD;
				shelf = 0;
			}
			px[size_t( i )] = x;
			py[size_t( i )] = y;
			x += pw[size_t( i )] + PAD;
			shelf = qMax( shelf, ph[size_t( i )] );
		}
		AH = y + shelf;
		if ( AH <= maxTex || drop >= 4 )
			break;
	}
	if ( AH > maxTex ) {
		*why = QString( "the decal atlas needs %1 rows even at mip %2" ).arg( AH ).arg( drop );
		return false;
	}
	AH = qMax( 4, ( AH + 3 ) & ~3 );
	std::vector<quint32> C( size_t( AW ) * AH, 0u ), N( size_t( AW ) * AH, 0x00808080u );
	ds.atlasRect.assign( size_t( n ) * 4, 0.0f );
	ds.texel.assign( size_t( n ), 4.0f );
	std::vector<quint32> pic;
	for ( int i = 0; i < n; i++ ) {
		const LoddPiece & p = ds.lodd.piece( i );
		const int m = qMin( drop, qMax( 0, p.mips - 1 ) );
		for ( int sheet = 0; sheet < 2; sheet++ ) {
			int w = 0, h = 0;
			if ( !ds.lodd.picture( i, sheet == 1, m, pic, &w, &h, why ) )
				return false;
			std::vector<quint32> & dst = sheet ? N : C;
			for ( int y = 0; y < h && py[size_t( i )] + y < AH; y++ )
				std::memcpy( &dst[size_t( py[size_t( i )] + y ) * AW + px[size_t( i )]], &pic[size_t( y ) * w],
					size_t( qMin( w, AW - px[size_t( i )] ) ) * 4 );
		}
		// half-texel inset so the clamp never reads the neighbour
		ds.atlasRect[size_t( i ) * 4 + 0] = ( float( px[size_t( i )] ) + 0.5f ) / float( AW );
		ds.atlasRect[size_t( i ) * 4 + 1] = ( float( py[size_t( i )] ) + 0.5f ) / float( AH );
		ds.atlasRect[size_t( i ) * 4 + 2] = ( float( px[size_t( i )] + pw[size_t( i )] ) - 0.5f ) / float( AW );
		ds.atlasRect[size_t( i ) * 4 + 3] = ( float( py[size_t( i )] + ph[size_t( i )] ) - 0.5f ) / float( AH );
		ds.texel[size_t( i )] = ( p.x1 - p.x0 ) / float( pw[size_t( i )] );
	}
	ds.atlasC = uploadRgba( gl, C, AW, AH, true );
	ds.atlasN = uploadRgba( gl, N, AW, AH, true );
	ds.atlasW = AW;
	ds.atlasH = AH;
	ds.drop = drop;
	ds.ok = true;
	return true;
}

//! The .lodl grid a view draws: heights, splat top-4, vertex colour.
struct Grid
{
	int nx = 0, ny = 0, stride = 1;
	float origin[2] = { 0, 0 }, spacing = 128.0f;
	GLuint height = 0, idx = 0, wts = 0, vclr = 0, vao = 0, vbo = 0, ibo = 0;
	GLsizei indexCount = 0;
	float zMin = 0, zMax = 0;
	std::vector<float> z;
	float heightAt( float wx, float wy ) const
	{
		const int i = qBound( 0, int( std::lround( ( wx - origin[0] ) / spacing ) ), nx - 1 );
		const int j = qBound( 0, int( std::lround( ( wy - origin[1] ) / spacing ) ), ny - 1 );
		return z[size_t( j ) * nx + i];
	}
};

void freeGrid( GL * gl, Grid & g )
{
	GLuint t[4] = { g.height, g.idx, g.wts, g.vclr };
	gl->glDeleteTextures( 4, t );
	gl->glDeleteBuffers( 1, &g.vbo );
	gl->glDeleteBuffers( 1, &g.ibo );
	gl->glDeleteVertexArrays( 1, &g.vao );
	g = Grid();
}

bool buildGrid( GL * gl, const LodtFile & L, int cx0, int cy0, int cx1, int cy1, int stride, Grid & g, QString * why )
{
	const int spc = L.samplesPerCell();
	cx0 = qMax( cx0, L.cellMinX() );
	cy0 = qMax( cy0, L.cellMinY() );
	cx1 = qMin( cx1, L.cellMaxX() );
	cy1 = qMin( cy1, L.cellMaxY() );
	if ( cx1 < cx0 || cy1 < cy0 ) {
		*why = QStringLiteral( "the view's cells miss the .lodl" );
		return false;
	}
	g.stride = qMax( 1, stride );
	g.nx = ( cx1 - cx0 + 1 ) * spc / g.stride + 1;
	g.ny = ( cy1 - cy0 + 1 ) * spc / g.stride + 1;
	g.spacing = 128.0f * float( g.stride );
	g.origin[0] = float( cx0 ) * 4096.0f;
	g.origin[1] = float( cy0 ) * 4096.0f;
	const int baseX = ( cx0 - L.cellMinX() ) * spc, baseY = ( cy0 - L.cellMinY() ) * spc;
	const int lastX = L.cellsX() * spc - 1, lastY = L.cellsY() * spc - 1;
	const size_t N = size_t( g.nx ) * size_t( g.ny );
	g.z.assign( N, 0.0f );
	std::vector<quint16> idx( N * 4, 0xFFFFu );
	std::vector<quint8> wts( N * 4, 0 );
	std::vector<quint32> vclr( N, 0xFFFFFFFFu );
	g.zMin = 1e30f;
	g.zMax = -1e30f;
	const int nl = L.ltexCount();
	for ( int j = 0; j < g.ny; j++ ) {
		const int gy = qMin( baseY + j * g.stride, lastY );
		for ( int i = 0; i < g.nx; i++ ) {
			const int gx = qMin( baseX + i * g.stride, lastX );
			const size_t s = size_t( j ) * g.nx + i;
			const float zz = L.height( gx, gy );
			g.z[s] = zz;
			g.zMin = qMin( g.zMin, zz );
			g.zMax = qMax( g.zMax, zz );
			quint16 id[24];
			float w[24];
			int k = 0;
			// the file's compositing of the quadrant that owns sample (sx_, sy_), added into id/w at weight f
			auto addQuad = [&]( int sx_, int sy_, float f ) {
				const int qcx = L.cellMinX() + sx_ / spc, qcy = L.cellMinY() + sy_ / spc;
				const int quad = ( ( sy_ % spc ) >= spc / 2 ? 2 : 0 ) | ( ( sx_ % spc ) >= spc / 2 ? 1 : 0 );
				quint16 q[6];
				L.quadrantSlots( qcx, qcy, quad, q );
				// the base, then slots 4..0 over it (slot 0 on top)
				quint16 qi[6];
				float qw[6];
				int qk = 0;
				// an empty base (no BTXT) is the engine's default land set, layer `nl` of the array
				qi[qk] = ( q[5] < nl ) ? q[5] : quint16( nl );
				qw[qk++] = 1.0f;
				const quint16 aw = L.alphaWord( sx_, sy_ );
				for ( int layer = 4; layer >= 0; layer-- ) {
					const int a = ( aw >> ( 3 * layer ) ) & 7;
					if ( !a || q[layer] >= nl )
						continue;
					const float t = float( a ) / 7.0f;
					for ( int m = 0; m < qk; m++ )
						qw[m] *= 1.0f - t;
					int m = 0;
					while ( m < qk && qi[m] != q[layer] )
						m++;
					if ( m == qk ) {
						qi[qk] = q[layer];
						qw[qk++] = 0.0f;
					}
					qw[m] += t;
				}
				for ( int u = 0; u < qk; u++ ) {
					int m = 0;
					while ( m < k && id[m] != qi[u] )
						m++;
					if ( m == k ) {
						id[k] = qi[u];
						w[k++] = 0.0f;
					}
					w[m] += f * qw[u];
				}
			};
			/* THE QUADRANT CROSS-FADE (TERRLIVE2), the live twin of the baker's
			 * (lodgenBakeVtTile, same margin, same halved quintic): within
			 * `margin` of a 2,048-unit quadrant line the neighbour quadrant's
			 * composite, read at its edge sample, is mixed in -- 0.5 at the line.
			 * Without it every quadrant's base texture ends in a hard square. */
			const int hq = spc / 2, lxs = gx % hq, lys = gy % hq;
			const float margin = lodgenBlendMargin();
			auto ease = []( float t ) -> float {
				const float u2 = qBound( 0.0f, t, 1.0f );
				return 0.5f * u2 * u2 * u2 * ( u2 * ( u2 * 6.0f - 15.0f ) + 10.0f );
			};
			int nx_ = gx, ny_ = gy;
			float wxN = 0.0f, wyN = 0.0f;
			if ( lodgenBlendEdges() == 1 ) {
				const float lxq = float( lxs ) * 128.0f, lyq = float( lys ) * 128.0f;
				if ( lxq < margin && gx - lxs - 1 >= 0 ) {
					nx_ = gx - lxs - 1;
					wxN = ease( 1.0f - lxq / margin );
				} else if ( 2048.0f - lxq < margin && gx + ( hq - lxs ) <= lastX ) {
					nx_ = gx + ( hq - lxs );
					wxN = ease( 1.0f - ( 2048.0f - lxq ) / margin );
				}
				if ( lyq < margin && gy - lys - 1 >= 0 ) {
					ny_ = gy - lys - 1;
					wyN = ease( 1.0f - lyq / margin );
				} else if ( 2048.0f - lyq < margin && gy + ( hq - lys ) <= lastY ) {
					ny_ = gy + ( hq - lys );
					wyN = ease( 1.0f - ( 2048.0f - lyq ) / margin );
				}
			}
			addQuad( gx, gy, ( 1.0f - wxN ) * ( 1.0f - wyN ) );
			if ( wxN > 0.0f )
				addQuad( nx_, gy, wxN * ( 1.0f - wyN ) );
			if ( wyN > 0.0f )
				addQuad( gx, ny_, ( 1.0f - wxN ) * wyN );
			if ( wxN > 0.0f && wyN > 0.0f )
				addQuad( nx_, ny_, wxN * wyN );
			// the four heaviest, renormalised
			for ( int a = 0; a < k; a++ )
				for ( int b = a + 1; b < k; b++ )
					if ( w[b] > w[a] ) {
						std::swap( w[a], w[b] );
						std::swap( id[a], id[b] );
					}
			float sum = 0.0f;
			for ( int a = 0; a < qMin( k, 4 ); a++ )
				sum += w[a];
			for ( int a = 0; a < qMin( k, 4 ); a++ ) {
				idx[s * 4 + a] = id[a];
				wts[s * 4 + a] = quint8( qBound( 0, int( w[a] / qMax( sum, 1e-6f ) * 255.0f + 0.5f ), 255 ) );
			}
			const quint16 cw = L.colourWord( gx, gy );
			if ( cw != 0xFFFFu ) {
				const quint32 r = ( ( cw >> 11 ) & 31 ) * 255 / 31, gg = ( ( cw >> 6 ) & 31 ) * 255 / 31, b = ( cw & 31 ) * 255 / 31;
				vclr[s] = 0xFF000000u | ( r << 16 ) | ( gg << 8 ) | b;
			}
		}
	}
	auto tex = [&]( GLenum ifmt, GLenum fmt, GLenum type, const void * data, bool linear ) {
		GLuint t = 0;
		gl->glGenTextures( 1, &t );
		gl->glBindTexture( GL_TEXTURE_2D, t );
		gl->glPixelStorei( GL_UNPACK_ALIGNMENT, 1 );
		gl->glTexImage2D( GL_TEXTURE_2D, 0, ifmt, g.nx, g.ny, 0, fmt, type, data );
		gl->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, linear ? GL_LINEAR : GL_NEAREST );
		gl->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, linear ? GL_LINEAR : GL_NEAREST );
		gl->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
		gl->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
		return t;
	};
	g.height = tex( GL_R32F, GL_RED, GL_FLOAT, g.z.data(), true );
	g.idx = tex( GL_RGBA16UI, GL_RGBA_INTEGER, GL_UNSIGNED_SHORT, idx.data(), false );
	g.wts = tex( GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, wts.data(), false );
	g.vclr = tex( GL_RGBA8, GL_BGRA, GL_UNSIGNED_BYTE, vclr.data(), true );
	std::vector<float> v( N * 2 );
	for ( int j = 0; j < g.ny; j++ )
		for ( int i = 0; i < g.nx; i++ ) {
			v[( size_t( j ) * g.nx + i ) * 2] = float( i );
			v[( size_t( j ) * g.nx + i ) * 2 + 1] = float( j );
		}
	std::vector<quint32> ix;
	ix.reserve( size_t( g.nx - 1 ) * size_t( g.ny - 1 ) * 6 );
	for ( int j = 0; j + 1 < g.ny; j++ )
		for ( int i = 0; i + 1 < g.nx; i++ ) {
			const quint32 a = quint32( j * g.nx + i ), b = a + 1, c = a + quint32( g.nx ), d = c + 1;
			ix.insert( ix.end(), { a, b, d, a, d, c } );
		}
	g.indexCount = GLsizei( ix.size() );
	gl->glGenVertexArrays( 1, &g.vao );
	gl->glBindVertexArray( g.vao );
	gl->glGenBuffers( 1, &g.vbo );
	gl->glBindBuffer( GL_ARRAY_BUFFER, g.vbo );
	gl->glBufferData( GL_ARRAY_BUFFER, GLsizeiptr( v.size() * 4 ), v.data(), GL_STATIC_DRAW );
	gl->glEnableVertexAttribArray( 0 );
	gl->glVertexAttribPointer( 0, 2, GL_FLOAT, GL_FALSE, 0, nullptr );
	gl->glGenBuffers( 1, &g.ibo );
	gl->glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, g.ibo );
	gl->glBufferData( GL_ELEMENT_ARRAY_BUFFER, GLsizeiptr( ix.size() * 4 ), ix.data(), GL_STATIC_DRAW );
	gl->glBindVertexArray( 0 );
	return true;
}

/* ---- the blend to vanilla (law 2, lane TERRLIVE1 2026-09-29) --------------
 *
 * The same law as the bake's vanilla-colour fill (lodgen.cpp,
 * lodgenVtFillTile): painted per QUADRANT, our weight
 * w = smoothstep( 0, LODGEN_VT_FILL_BAND, distance inside the painted ground to
 * the nearest unpainted quadrant square ), colour = mix( vanilla, ours, w ).
 * Here the painted test reads the .lodl's quadrant slots (a real base or any
 * real layer) and vanilla's dim-4 diffuse is read live from the game's sheets,
 * so the live ground and the baked far levels follow one rule. Lane TERRLIVE2:
 * the distance is the ROUNDED field (LodgenOutlineField), the bake's own. */
struct Blend
{
	GLuint wTex = 0, vTex = 0;
	float wMap[4] = { 0, 0, 1, 1 }, vMap[4] = { 0, 0, 1, 1 };
	int wn = 0, hn = 0;
	float res = 128.0f, x0 = 0.0f, yN = 0.0f;
	std::vector<float> w;
	qint64 paintedQuads = 0, vanSheets = 0, vanMissing = 0, vanBytes = 0;
	int vanMip = 0;
	float weightAt( float wx, float wy ) const
	{
		if ( w.empty() )
			return 1.0f;
		const int i = qBound( 0, int( std::floor( ( wx - x0 ) / res ) ), wn - 1 );
		const int j = qBound( 0, int( std::floor( ( yN - wy ) / res ) ), hn - 1 );
		return w[size_t( j ) * wn + i];
	}
};

int floorDiv( int a, int b )
{
	return ( a >= 0 ) ? a / b : -( ( -a + b - 1 ) / b );
}

bool buildBlend( GL * gl, const LodtFile & L, int cx0, int cy0, int cx1, int cy1, const QString & ws, Blend & B, QString * why )
{
	cx0 = qMax( cx0, L.cellMinX() );
	cy0 = qMax( cy0, L.cellMinY() );
	cx1 = qMin( cx1, L.cellMaxX() );
	cy1 = qMin( cy1, L.cellMaxY() );
	const int nl = L.ltexCount();
	const float band = LODGEN_VT_FILL_BAND;
	// the rounded outline's window (lane TERRLIVE2): far enough that the field matches the bake's
	const int R = lodgenOutlineWindowQuads( band );
	// the painted quadrants over the view plus the field's reach
	const int qx0 = 2 * cx0 - R, qy0 = 2 * cy0 - R, qx1 = 2 * cx1 + 1 + R, qy1 = 2 * cy1 + 1 + R;
	const int qw = qx1 - qx0 + 1, qh = qy1 - qy0 + 1;
	std::vector<quint8> pq( size_t( qw ) * qh, 0 );
	for ( int qy = qy0; qy <= qy1; qy++ )
		for ( int qx = qx0; qx <= qx1; qx++ ) {
			const int cx = floorDiv( qx, 2 ), cy = floorDiv( qy, 2 );
			if ( cx < L.cellMinX() || cy < L.cellMinY() || cx > L.cellMaxX() || cy > L.cellMaxY() )
				continue;
			quint16 q[6];
			L.quadrantSlots( cx, cy, ( ( qy - 2 * cy ) << 1 ) | ( qx - 2 * cx ), q );
			bool p = false;
			for ( int k = 0; k < 6; k++ )
				p = p || q[k] < nl;
			pq[size_t( qy - qy0 ) * qw + ( qx - qx0 )] = p ? 1 : 0;
			if ( p && cx >= cx0 && cx <= cx1 && cy >= cy0 && cy <= cy1 )
				B.paintedQuads++;
		}
	auto painted = [&]( int qx, int qy ) {
		if ( qx < qx0 || qy < qy0 || qx > qx1 || qy > qy1 )
			return false;
		return pq[size_t( qy - qy0 ) * qw + ( qx - qx0 )] != 0;
	};
	// the weight map
	const float extent = float( qMax( cx1 - cx0 + 1, cy1 - cy0 + 1 ) ) * 4096.0f;
	B.res = 128.0f;
	while ( extent / B.res > 4096.0f )
		B.res *= 2.0f;
	B.x0 = float( cx0 ) * 4096.0f;
	B.yN = float( cy1 + 1 ) * 4096.0f;
	B.wn = int( std::ceil( float( cx1 - cx0 + 1 ) * 4096.0f / B.res ) );
	B.hn = int( std::ceil( float( cy1 - cy0 + 1 ) * 4096.0f / B.res ) );
	B.w.assign( size_t( B.wn ) * B.hn, 1.0f );
	// the same rounded field the bake reads (LodgenOutlineField), built over this window
	LodgenOutlineField field;
	lodgenOutlineBuild( painted, qx0, qy0, qx1, qy1, band, field );
	for ( int j = 0; j < B.hn; j++ ) {
		const float wy = B.yN - ( float( j ) + 0.5f ) * B.res;
		const int qy = int( std::floor( wy / 2048.0f ) );
		for ( int i = 0; i < B.wn; i++ ) {
			const float wx = B.x0 + ( float( i ) + 0.5f ) * B.res;
			const int qx = int( std::floor( wx / 2048.0f ) );
			B.w[size_t( j ) * B.wn + i] = painted( qx, qy ) ? field.weight( wx, wy, band ) : 0.0f;
		}
	}
	/* WW_BLEND_DUMP=<file> (lane TERRLIVE2's outline gate): "BLDW" v1, then i32 wn hn,
	 * f32 res x0 yN, i32 qx0 qy0 qw qh, then wn*hn f32 weights (row 0 = north), then
	 * qw*qh u8 painted quadrants (row 0 = south). */
	const QByteArray dumpPath = qgetenv( "WW_BLEND_DUMP" );
	if ( !dumpPath.isEmpty() ) {
		QFile df( QString::fromLocal8Bit( dumpPath ) );
		if ( df.open( QIODevice::WriteOnly ) ) {
			const qint32 hi[2] = { B.wn, B.hn };
			const float hf[3] = { B.res, B.x0, B.yN };
			const qint32 hq[4] = { qx0, qy0, qw, qh };
			df.write( "BLDW", 4 );
			const qint32 ver = 1;
			df.write( reinterpret_cast<const char *>( &ver ), 4 );
			df.write( reinterpret_cast<const char *>( hi ), 8 );
			df.write( reinterpret_cast<const char *>( hf ), 12 );
			df.write( reinterpret_cast<const char *>( hq ), 16 );
			df.write( reinterpret_cast<const char *>( B.w.data() ), qint64( B.w.size() * 4 ) );
			df.write( reinterpret_cast<const char *>( pq.data() ), qint64( pq.size() ) );
		}
	}
	gl->glGenTextures( 1, &B.wTex );
	gl->glBindTexture( GL_TEXTURE_2D, B.wTex );
	gl->glPixelStorei( GL_UNPACK_ALIGNMENT, 4 );
	gl->glTexImage2D( GL_TEXTURE_2D, 0, GL_R32F, B.wn, B.hn, 0, GL_RED, GL_FLOAT, B.w.data() );
	gl->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
	gl->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
	gl->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
	gl->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
	B.wMap[0] = B.x0;
	B.wMap[1] = B.yN;
	B.wMap[2] = 1.0f / ( float( B.wn ) * B.res );
	B.wMap[3] = 1.0f / ( float( B.hn ) * B.res );

	// vanilla's dim-4 diffuse, on the worldspace's own grid (the LODSettings file, as the bake reads it)
	int gridX = 0, gridY = 0;
	{
		QFile lf( QDir( lodgenVanillaLodRoot() ).filePath( QStringLiteral( "LODSettings/%1.LOD" ).arg( ws ) ) );
		if ( !lodgenVanillaLodRoot().isEmpty() && lf.open( QIODevice::ReadOnly ) ) {
			const QByteArray b = lf.read( 4 );
			if ( b.size() >= 4 ) {
				gridX = ( ( qint16( quint8( b[0] ) | ( quint8( b[1] ) << 8 ) ) % 4 ) + 4 ) % 4;
				gridY = ( ( qint16( quint8( b[2] ) | ( quint8( b[3] ) << 8 ) ) % 4 ) + 4 ) % 4;
			}
		}
	}
	const int k0 = floorDiv( cx0 - gridX, 4 ), k1 = floorDiv( cx1 - gridX, 4 );
	const int m0 = floorDiv( cy0 - gridY, 4 ), m1 = floorDiv( cy1 - gridY, 4 );
	const int ncx = k1 - k0 + 1, ncy = m1 - m0 + 1;
	B.vanMip = 0;
	while ( ( 512 >> B.vanMip ) * qMax( ncx, ncy ) > 8192 )
		B.vanMip++;
	const int P = 512 >> B.vanMip;
	std::vector<quint32> img( size_t( ncx * P ) * size_t( ncy * P ), 0x00808080u );
	for ( int m = m0; m <= m1; m++ )
		for ( int k = k0; k <= k1; k++ ) {
			const int chunkX = gridX + 4 * k, chunkY = gridY + 4 * m;
			QByteArray bytes;
			if ( !lodgenReadVanillaSheet( ws, 4, chunkX, chunkY, QString(), bytes ) ) {
				B.vanMissing++;
				continue;
			}
			try {
				DDSTexture16 t( reinterpret_cast<const unsigned char *>( bytes.constData() ), size_t( bytes.size() ) );
				int top = 0;
				while ( ( t.getWidth() >> top ) > 512 )
					top++;
				const int mip = top + B.vanMip;
				if ( ( t.getWidth() >> top ) != 512 || mip > t.getMaxMipLevel() ) {
					B.vanMissing++;
					continue;
				}
				const int ox = ( k - k0 ) * P, oy = ( m1 - m ) * P;   // row 0 = north
				for ( int y = 0; y < P; y++ )
					for ( int x = 0; x < P; x++ ) {
						const FloatVector4 c = FloatVector4::convertFloat16( t.getPixelC( x, y, mip ) );
						const quint32 r = quint32( qBound( 0, int( c[0] * 255.0f + 0.5f ), 255 ) );
						const quint32 g = quint32( qBound( 0, int( c[1] * 255.0f + 0.5f ), 255 ) );
						const quint32 b = quint32( qBound( 0, int( c[2] * 255.0f + 0.5f ), 255 ) );
						img[size_t( oy + y ) * size_t( ncx * P ) + size_t( ox + x )] = 0xFF000000u | ( r << 16 ) | ( g << 8 ) | b;
					}
				B.vanSheets++;
				B.vanBytes += bytes.size();
			} catch ( ... ) {
				B.vanMissing++;
			}
		}
	B.vTex = uploadRgba( gl, img, ncx * P, ncy * P, true );
	B.vMap[0] = float( gridX + 4 * k0 ) * 4096.0f;
	B.vMap[1] = float( gridY + 4 * m1 + 4 ) * 4096.0f;
	B.vMap[2] = 1.0f / ( float( ncx ) * 16384.0f );
	B.vMap[3] = 1.0f / ( float( ncy ) * 16384.0f );
	if ( !B.vanSheets ) {
		*why = QStringLiteral( "no vanilla dim-4 sheet under the vanilla LOD root " ) + lodgenVanillaLodRoot();
		return false;
	}
	return true;
}

struct Camera
{
	bool ortho = true;
	QMatrix4x4 vp, inv;
	QVector3D eye;          //!< the distance origin (the focus point for an ortho view)
	float pixelK = 1.0f;    //!< world units a pixel: ortho = constant, perspective = per unit of distance
};

double median( std::vector<double> v )
{
	if ( v.empty() )
		return 0.0;
	std::sort( v.begin(), v.end() );
	return v[v.size() / 2];
}

QImage titled( const QList<QImage> & panels, const QStringList & titles )
{
	const int bar = 60;
	int W = 0, H = 0;
	for ( const QImage & p : panels ) {
		W += p.width();
		H = qMax( H, p.height() );
	}
	QImage out( W, H + bar, QImage::Format_RGB888 );
	out.fill( QColor( 24, 24, 24 ) );
	QPainter pt( &out );
	QFont font( QStringLiteral( "Segoe UI" ) );
	font.setPixelSize( 24 );
	font.setBold( true );
	pt.setFont( font );
	pt.setPen( QColor( 235, 235, 235 ) );
	int x = 0;
	for ( int i = 0; i < panels.size(); i++ ) {
		pt.drawImage( x, bar, panels[i] );
		pt.drawText( QRect( x + 12, 0, panels[i].width() - 24, bar ), Qt::AlignVCenter | Qt::AlignLeft,
			titles.value( i ) );
		if ( i )
			pt.fillRect( x - 1, 0, 2, H + bar, QColor( 24, 24, 24 ) );
		x += panels[i].width();
	}
	pt.end();
	return out;
}

} // namespace

int terrainPreviewRun( const EsmWorld & world, const QString & dataRoot, const QString & specPath )
{
	auto fail = []( const QString & m ) {
		say( QStringLiteral( "REFUSED: " ) + m );
		return 1;
	};
	if ( !qobject_cast<QGuiApplication *>( QCoreApplication::instance() ) )
		return fail( QStringLiteral( "no GUI platform in this process (platforms/qwindows.dll beside the exe, and the GPU setting on)" ) );
	QFile sf( specPath );
	if ( !sf.open( QIODevice::ReadOnly ) )
		return fail( QStringLiteral( "cannot read the spec " ) + specPath );
	QJsonParseError pe;
	const QJsonObject spec = QJsonDocument::fromJson( sf.readAll(), &pe ).object();
	if ( pe.error != QJsonParseError::NoError )
		return fail( QStringLiteral( "spec JSON: " ) + pe.errorString() );
	// every relative path in the spec is relative to the spec file, never to the caller's folder
	QDir::setCurrent( QFileInfo( specPath ).absolutePath() );

	LodtFile L;
	QString why;
	if ( !L.open( spec.value( "lodl" ).toString(), &why ) )
		return fail( QStringLiteral( ".lodl: " ) + why );
	L.setBlockCacheSize( 512 );

	QSurfaceFormat fmt;
	fmt.setVersion( 4, 3 );
	fmt.setProfile( QSurfaceFormat::CoreProfile );
	QOffscreenSurface surface;
	surface.setFormat( fmt );
	surface.create();
	QOpenGLContext ctx;
	ctx.setFormat( fmt );
	if ( !surface.isValid() || !ctx.create() || !ctx.makeCurrent( &surface ) )
		return fail( QStringLiteral( "no OpenGL 4.3 context" ) );
	GL * gl = QOpenGLVersionFunctionsFactory::get<QOpenGLFunctions_4_3_Core>( &ctx );
	if ( !gl || !gl->initializeOpenGLFunctions() )
		return fail( QStringLiteral( "no OpenGL 4.3 core functions" ) );
	say( QString( "GL %1 on %2" ).arg( QString::fromLatin1( reinterpret_cast<const char *>( gl->glGetString( GL_VERSION ) ) ) )
		.arg( QString::fromLatin1( reinterpret_cast<const char *>( gl->glGetString( GL_RENDERER ) ) ) ) );

	const QByteArray common( kDecalCommon ), head( kDecalFsHead );
	const GLuint pTerrain = program( gl, kTerrainVs, kTerrainFs, &why );
	const GLuint pDecal = pTerrain ? program( gl, common + kDecalVs, common + head + kDecalFs, &why ) : 0;
	const GLuint pCount = pDecal ? program( gl, common + kDecalVs, common + head + kCountFs, &why ) : 0;
	const GLuint pLight = pCount ? program( gl, kLightVs, kLightFs, &why ) : 0;
	if ( !pLight )
		return fail( why );

	/* ---- the rule paint map (lane TERRLIVE1): the spec's "rule", a .lodr ---- */
	LodgenRuleMap rule;
	bool haveRule = false;
	QVector<quint16> ruleLayer;         // palette index -> LTEX array layer
	QVector<quint32> ltexExtra;         // rule textures the .lodl does not name, as extra layers
	if ( !spec.value( "rule" ).toString().isEmpty() ) {
		if ( !lodgenRuleRead( spec.value( "rule" ).toString(), rule, &why ) )
			return fail( QStringLiteral( ".lodr: " ) + why );
		haveRule = true;
		for ( quint32 form : rule.palette ) {
			int layer = -1;
			if ( form == ESM_LTEX_ENGINE_DEFAULT )
				layer = L.ltexCount();
			for ( int i = 0; i < L.ltexCount() && layer < 0; i++ )
				if ( L.ltexForm( i ) == form )
					layer = i;
			if ( layer < 0 ) {
				int e = ltexExtra.indexOf( form );
				if ( e < 0 ) {
					e = ltexExtra.size();
					ltexExtra.append( form );
				}
				layer = L.ltexCount() + 1 + e;
			}
			ruleLayer.append( quint16( layer ) );
		}
		say( QString( "rule: %1 x %2 samples at %3 u, palette %4 (%5 not in the .lodl, added as layers)" )
			.arg( rule.nx() ).arg( rule.ny() ).arg( double( rule.spacing() ) ).arg( rule.palette.size() )
			.arg( ltexExtra.size() ) );
	}

	/* ---- the LTEX array: every record the .lodl names, one layer each ---- */
	QElapsedTimer tm;
	tm.start();
	const int side = qBound( 64, spec.value( "ltex_side" ).toInt( 512 ), 2048 );
	// one layer per .lodl LTEX, plus the engine default land set (esmdata.h), plus the rule's extras
	const int nl = L.ltexCount() + 1 + ltexExtra.size();
	GLuint ltexArr = 0;
	gl->glGenTextures( 1, &ltexArr );
	gl->glBindTexture( GL_TEXTURE_2D_ARRAY, ltexArr );
	const int levels = int( std::log2( double( side ) ) ) + 1;
	gl->glTexStorage3D( GL_TEXTURE_2D_ARRAY, levels, GL_RGBA8, side, side, nl );
	int ltexMissing = 0;
	{
		LodgenBakeCaches * caches = lodgenCreateBakeCaches();
		std::vector<quint32> pic;
		for ( int i = 0; i < nl; i++ ) {
			QString path;
			const quint32 form = i < L.ltexCount() ? L.ltexForm( i )
				: ( i == L.ltexCount() ? ESM_LTEX_ENGINE_DEFAULT : ltexExtra.at( i - L.ltexCount() - 1 ) );
			if ( !lodgenLtexPicture( world, dataRoot, caches, form, side, pic, &path ) ) {
				pic.assign( size_t( side ) * side, 0xFF808080u );
				ltexMissing++;
				say( QString( "ltex: no texture for %1 (%2) -- drawn grey" )
					.arg( form, 8, 16, QChar( '0' ) ).arg( path.isEmpty() ? QStringLiteral( "no path" ) : path ) );
			}
			gl->glTexSubImage3D( GL_TEXTURE_2D_ARRAY, 0, 0, 0, i, side, side, 1, GL_BGRA, GL_UNSIGNED_BYTE, pic.data() );
		}
		lodgenDestroyBakeCaches( caches );
	}
	gl->glGenerateMipmap( GL_TEXTURE_2D_ARRAY );
	gl->glTexParameteri( GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR );
	gl->glTexParameteri( GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
	gl->glTexParameteri( GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_REPEAT );
	gl->glTexParameteri( GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_REPEAT );
	gl->glTexParameterf( GL_TEXTURE_2D_ARRAY, 0x84FE /* GL_TEXTURE_MAX_ANISOTROPY */, 8.0f );
	say( QString( "ltex: %1 record(s) + the engine default at %2 texels, %3 without a texture (drawn grey), tiling %4 u, %5 ms" )
		.arg( L.ltexCount() ).arg( side ).arg( ltexMissing ).arg( double( lodgenLandTiling() ), 0, 'f', 3 )
		.arg( tm.elapsed() ) );

	drainErrors( gl, QStringLiteral( "the LTEX array" ) );
	GLuint ruleIdx = 0, ruleW = 0, ruleG = 0;
	if ( haveRule ) {
		const int rnx = rule.nx(), rny = rule.ny();
		std::vector<quint16> ri( size_t( rnx ) * rny * 4, 0xFFFFu );
		std::vector<quint8> rw( size_t( rnx ) * rny * 4, 0 );
		for ( size_t s = 0; s < size_t( rnx ) * rny; s++ ) {
			if ( rule.a[s] == 0xFF )
				continue;
			ri[s * 4 + 0] = ruleLayer.at( rule.a[s] );
			ri[s * 4 + 1] = ruleLayer.at( rule.b[s] );
			rw[s * 4 + 0] = rule.w[s];
			rw[s * 4 + 1] = quint8( 255 - rule.w[s] );
		}
		auto mk = [&]( GLenum ifmt, GLenum fmt, GLenum type, const void * data ) {
			GLuint t = 0;
			gl->glGenTextures( 1, &t );
			gl->glBindTexture( GL_TEXTURE_2D, t );
			gl->glPixelStorei( GL_UNPACK_ALIGNMENT, 1 );
			gl->glTexImage2D( GL_TEXTURE_2D, 0, ifmt, rnx, rny, 0, fmt, type, data );
			gl->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
			gl->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
			gl->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
			gl->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
			return t;
		};
		ruleIdx = mk( GL_RGBA16UI, GL_RGBA_INTEGER, GL_UNSIGNED_SHORT, ri.data() );
		ruleW = mk( GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, rw.data() );
		ruleG = mk( GL_R8, GL_RED, GL_UNSIGNED_BYTE, rule.g.data() );
		drainErrors( gl, QStringLiteral( "the rule map" ) );
	}
	/* ---- the AO map (lane TERRLIVE2): the spec's "loda", else the .loda beside the "rule" ---- */
	LodgenAoMap loda;
	bool haveLoda = false;
	{
		QString lp = spec.value( "loda" ).toString();
		if ( lp.isEmpty() && haveRule ) {
			const QDir rd = QFileInfo( spec.value( "rule" ).toString() ).absoluteDir();
			const QStringList f = rd.entryList( { QStringLiteral( "*.loda" ) }, QDir::Files, QDir::Name );
			if ( !f.isEmpty() )
				lp = rd.filePath( f.first() );
		}
		if ( !lp.isEmpty() ) {
			if ( !lodgenAoRead( lp, loda, &why ) )
				return fail( QStringLiteral( ".loda: " ) + why );
			haveLoda = true;
			say( QString( "AO map: %1: %2 quadrant(s) at %3 u, %4 bytes" ).arg( lp ).arg( loda.quads.size() )
				.arg( double( LODGEN_AO_UNITS ) ).arg( QFileInfo( lp ).size() ) );
		}
	}
	const QJsonArray fadeA = spec.value( "fade" ).toArray();
	const float fade0 = float( fadeA.at( 0 ).toDouble( 8192.0 ) ), fade1 = float( fadeA.at( 1 ).toDouble( 12288.0 ) );
	const int frames = qBound( 1, spec.value( "frames" ).toInt( 20 ), 500 );
	const bool decalNormals = spec.value( "decal_normals" ).toBool( true );

	GLuint queries[3] = { 0, 0, 0 };
	gl->glGenQueries( 3, queries );
	GLuint emptyVao = 0;
	gl->glGenVertexArrays( 1, &emptyVao );

	int failures = 0;
	const QJsonArray views = spec.value( "views" ).toArray();
	for ( const QJsonValue & vv : views ) {
		const QJsonObject V = vv.toObject();
		const QString vname = V.value( "name" ).toString();
		const QJsonArray cells = V.value( "cells" ).toArray();
		tm.restart();
		Grid grid;
		if ( !buildGrid( gl, L, cells.at( 0 ).toInt(), cells.at( 1 ).toInt(), cells.at( 2 ).toInt(), cells.at( 3 ).toInt(),
				V.value( "stride" ).toInt( 1 ), grid, &why ) ) {
			say( QString( "view %1: REFUSED: %2" ).arg( vname, why ) );
			failures++;
			continue;
		}
		say( QString( "view %1: grid %2 x %3 samples at %4 u, heights %5..%6, built in %7 ms" )
			.arg( vname ).arg( grid.nx ).arg( grid.ny ).arg( double( grid.spacing ) )
			.arg( double( grid.zMin ), 0, 'f', 0 ).arg( double( grid.zMax ), 0, 'f', 0 ).arg( tm.elapsed() ) );

		// the baked levels: FULL's finest and the hybrid's
		Sheet full, farS, aoS;
		GLuint texFull = 0, texFar = 0, texAo = 0;
		const QString fullPath = V.value( "lodt_full" ).toString(), farPath = V.value( "lodt_far" ).toString();
		tm.restart();
		if ( !fullPath.isEmpty() ) {
			if ( decodeSheet( gl, fullPath, LODV_ROLE_COLOR, full, &why ) ) {
				texFull = uploadRgba( gl, full.img, full.w, full.h, true );
				say( QString( "view %1: FULL level %2: %3 x %4 texels at %5 u, %6 tile(s), %7 bytes" )
					.arg( vname, QFileInfo( fullPath ).fileName() ).arg( full.w ).arg( full.h )
					.arg( double( full.texelUnits ) ).arg( full.tiles ).arg( full.fileBytes ) );
				std::vector<quint32>().swap( full.img );
			} else
				say( QString( "view %1: FULL level: %2" ).arg( vname, why ) );
			if ( decodeSheet( gl, fullPath, LODV_ROLE_MASK, aoS, &why ) ) {
				// the mask sheet's B = sky AO; kept as grey in RGBA8 so one upload path serves
				for ( quint32 & p : aoS.img ) {
					const quint32 b = p & 0xFF;
					p = 0xFF000000u | ( b << 16 ) | ( b << 8 ) | b;
				}
				texAo = uploadRgba( gl, aoS.img, aoS.w, aoS.h, true );
				std::vector<quint32>().swap( aoS.img );
			} else
				say( QString( "view %1: AO (mask sheet B): %2" ).arg( vname, why ) );
		}
		if ( !farPath.isEmpty() ) {
			if ( decodeSheet( gl, farPath, LODV_ROLE_COLOR, farS, &why ) ) {
				texFar = uploadRgba( gl, farS.img, farS.w, farS.h, true );
				say( QString( "view %1: hybrid far level %2: %3 x %4 texels at %5 u, %6 tile(s), %7 bytes" )
					.arg( vname, QFileInfo( farPath ).fileName() ).arg( farS.w ).arg( farS.h )
					.arg( double( farS.texelUnits ) ).arg( farS.tiles ).arg( farS.fileBytes ) );
				std::vector<quint32>().swap( farS.img );
			} else
				say( QString( "view %1: hybrid far level: %2" ).arg( vname, why ) );
		}
		say( QString( "view %1: sheets decoded in %2 ms" ).arg( vname ).arg( tm.elapsed() ) );

		// law 2: the blend to vanilla's diffuse, unless the spec turns it off (a "before" picture)
		Blend blend;
		bool haveBlend = false;
		if ( V.value( "blend" ).toBool( true ) && !lodgenVanillaLodRoot().isEmpty() ) {
			tm.restart();
			haveBlend = buildBlend( gl, L, cells.at( 0 ).toInt(), cells.at( 1 ).toInt(), cells.at( 2 ).toInt(),
				cells.at( 3 ).toInt(), spec.value( "ws" ).toString( QStringLiteral( "Commonwealth" ) ), blend, &why );
			if ( haveBlend )
				say( QString( "view %1: blend to vanilla: band %2 u, weight map %3 x %4 at %5 u, painted quadrants %6; "
						"vanilla dim-4 diffuse read live: %7 sheet(s), %8 bytes (game files, not shipped), %9 missing, "
						"mip %10; built in %11 ms" )
					.arg( vname ).arg( double( LODGEN_VT_FILL_BAND ) ).arg( blend.wn ).arg( blend.hn ).arg( double( blend.res ) )
					.arg( blend.paintedQuads ).arg( blend.vanSheets ).arg( blend.vanBytes ).arg( blend.vanMissing )
					.arg( blend.vanMip ).arg( tm.elapsed() ) );
			else
				say( QString( "view %1: blend to vanilla: REFUSED: %2" ).arg( vname, why ) );
		} else
			say( QString( "view %1: blend to vanilla: off (%2)" ).arg( vname,
				lodgenVanillaLodRoot().isEmpty() ? QStringLiteral( "no --vanilla-lod-root" ) : QStringLiteral( "the spec's \"blend\": false" ) ) );

		// the AO map over the view's cells, north row first, 255 (no shading) outside painted ground
		GLuint lodaTex = 0;
		float lodaMap[4] = { 0, 0, 1, 1 };
		if ( haveLoda ) {
			const int qx0 = 2 * cells.at( 0 ).toInt(), qy0 = 2 * cells.at( 1 ).toInt();
			const int qx1 = 2 * cells.at( 2 ).toInt() + 1, qy1 = 2 * cells.at( 3 ).toInt() + 1;
			const int Q = LODGEN_AO_QUAD;
			const int nx = ( qx1 - qx0 + 1 ) * Q, ny = ( qy1 - qy0 + 1 ) * Q;
			int step = 1;       // a whole-map view is box-filtered down to at most 8192 texels a side
			while ( qMax( nx, ny ) / step > 8192 )
				step *= 2;
			const int tw = nx / step, th = ny / step;
			std::vector<quint8> t( size_t( tw ) * th, 255 );
			qint64 hit = 0;
			for ( int qy = qy0; qy <= qy1; qy++ )
				for ( int qx = qx0; qx <= qx1; qx++ ) {
					const quint8 * b = loda.block( qx, qy );
					if ( !b )
						continue;
					hit++;
					for ( int v = 0; v < Q; v += step )
						for ( int u = 0; u < Q; u += step ) {
							int s = 0;
							for ( int dv = 0; dv < step; dv++ )
								for ( int du = 0; du < step; du++ )
									s += b[( v + dv ) * Q + u + du];
							const int r = ( ( qy1 - qy ) * Q + ( Q - 1 - v - ( step - 1 ) ) ) / step;
							const int col = ( ( qx - qx0 ) * Q + u ) / step;
							t[size_t( r ) * tw + col] = quint8( ( s + step * step / 2 ) / ( step * step ) );
						}
				}
			gl->glGenTextures( 1, &lodaTex );
			gl->glBindTexture( GL_TEXTURE_2D, lodaTex );
			gl->glPixelStorei( GL_UNPACK_ALIGNMENT, 1 );
			gl->glTexImage2D( GL_TEXTURE_2D, 0, GL_R8, tw, th, 0, GL_RED, GL_UNSIGNED_BYTE, t.data() );
			gl->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
			gl->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
			gl->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
			gl->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
			lodaMap[0] = float( qx0 ) * 2048.0f;
			lodaMap[1] = float( qy1 + 1 ) * 2048.0f;
			lodaMap[2] = 1.0f / ( float( nx ) * LODGEN_AO_UNITS );
			lodaMap[3] = 1.0f / ( float( ny ) * LODGEN_AO_UNITS );
			drainErrors( gl, QStringLiteral( "the AO map" ) );
			say( QString( "view %1: AO map %2 x %3 texels at %4 u, %5 painted quadrant(s) in view" )
				.arg( vname ).arg( tw ).arg( th ).arg( double( LODGEN_AO_UNITS ) * step ).arg( hit ) );
		}
		int lodaOn = 0;     // per render: the spec's "loda" (default on when the map is loaded)

		DecalSet ds;
		const QString decalPath = V.value( "decals" ).toString();
		if ( !decalPath.isEmpty() ) {
			tm.restart();
			if ( loadDecals( gl, decalPath, ds, &why ) )
				say( QString( "view %1: decals %2 piece(s), %3 placement(s), atlas %4 x %5 at mip %6, %7 ms" )
					.arg( vname ).arg( ds.lodd.pieceCount() ).arg( ds.lodg.recordCount() )
					.arg( ds.atlasW ).arg( ds.atlasH ).arg( ds.drop ).arg( tm.elapsed() ) );
			else
				say( QString( "view %1: decals: %2" ).arg( vname, why ) );
		}

		drainErrors( gl, QStringLiteral( "loading view " ) + vname );
		// camera
		const int W = qBound( 64, V.value( "width" ).toInt( 1600 ), 8192 );
		const int H = qBound( 64, V.value( "height" ).toInt( 1000 ), 8192 );
		Camera cam;
		const QJsonArray ortho = V.value( "ortho" ).toArray();
		if ( !ortho.isEmpty() ) {
			// ortho: [centre x, centre y, half width in world units], looking straight down
			const float cx = float( ortho.at( 0 ).toDouble() ), cy = float( ortho.at( 1 ).toDouble() );
			const float hw = float( ortho.at( 2 ).toDouble() ), hh = hw * float( H ) / float( W );
			QMatrix4x4 proj, view;
			proj.ortho( -hw, hw, -hh, hh, 1.0f, grid.zMax - grid.zMin + 20000.0f );
			view.lookAt( QVector3D( cx, cy, grid.zMax + 10000.0f ), QVector3D( cx, cy, grid.zMin ), QVector3D( 0, 1, 0 ) );
			cam.vp = proj * view;
			cam.ortho = true;
			cam.pixelK = 2.0f * hw / float( W );
			const QJsonArray focus = V.value( "focus" ).toArray();
			const float fx = focus.isEmpty() ? cx : float( focus.at( 0 ).toDouble() );
			const float fy = focus.isEmpty() ? cy : float( focus.at( 1 ).toDouble() );
			cam.eye = QVector3D( fx, fy, grid.heightAt( fx, fy ) + float( V.value( "eye_agl" ).toDouble( 150.0 ) ) );
		} else {
			const QJsonArray e = V.value( "eye" ).toArray(), a = V.value( "at" ).toArray();
			const float ex = float( e.at( 0 ).toDouble() ), ey = float( e.at( 1 ).toDouble() );
			const float ax = float( a.at( 0 ).toDouble() ), ay = float( a.at( 1 ).toDouble() );
			// z given as height above the ground under the point
			const QVector3D eye( ex, ey, grid.heightAt( ex, ey ) + float( e.at( 2 ).toDouble( 150.0 ) ) );
			const QVector3D at( ax, ay, grid.heightAt( ax, ay ) + float( a.at( 2 ).toDouble( 0.0 ) ) );
			const float fov = float( V.value( "fov" ).toDouble( 60.0 ) );
			QMatrix4x4 proj, view;
			proj.perspective( fov, float( W ) / float( H ), 32.0f, 400000.0f );
			view.lookAt( eye, at, QVector3D( 0, 0, 1 ) );
			cam.vp = proj * view;
			cam.ortho = false;
			cam.eye = eye;
			cam.pixelK = 2.0f * std::tan( fov * 0.5f * 3.14159265f / 180.0f ) / float( H );
		}
		cam.inv = cam.vp.inverted();

		// targets
		auto target = [&]( GLenum ifmt ) {
			GLuint t = 0;
			gl->glGenTextures( 1, &t );
			gl->glBindTexture( GL_TEXTURE_2D, t );
			gl->glTexStorage2D( GL_TEXTURE_2D, 1, ifmt, W, H );
			gl->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
			gl->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
			return t;
		};
		const GLuint tAlb = target( GL_RGBA8 ), tNrm = target( GL_RGBA8 ), tDepth = target( GL_DEPTH_COMPONENT32F );
		const GLuint tOut = target( GL_RGBA8 ), tCount = target( GL_RG32F );
		GLuint fbo[4];
		gl->glGenFramebuffers( 4, fbo );
		const GLenum both[2] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
		gl->glBindFramebuffer( GL_FRAMEBUFFER, fbo[0] );
		gl->glFramebufferTexture( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, tAlb, 0 );
		gl->glFramebufferTexture( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, tNrm, 0 );
		gl->glFramebufferTexture( GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, tDepth, 0 );
		gl->glDrawBuffers( 2, both );
		gl->glBindFramebuffer( GL_FRAMEBUFFER, fbo[1] );
		gl->glFramebufferTexture( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, tAlb, 0 );
		gl->glFramebufferTexture( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, tNrm, 0 );
		gl->glDrawBuffers( 2, both );
		gl->glBindFramebuffer( GL_FRAMEBUFFER, fbo[2] );
		gl->glFramebufferTexture( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, tOut, 0 );
		gl->glDrawBuffers( 1, both );
		gl->glBindFramebuffer( GL_FRAMEBUFFER, fbo[3] );
		gl->glFramebufferTexture( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, tCount, 0 );
		gl->glDrawBuffers( 1, both );
		gl->glViewport( 0, 0, W, H );

		GLuint ssbo = 0;
		gl->glGenBuffers( 1, &ssbo );

		// one frame of one option; returns false on a GL error
		auto uploadRecords = [&]( int option ) -> int {
			std::vector<float> buf;
			if ( !ds.ok || option == OptFull || option == OptLive || option == OptBaked )
				return 0;
			const float pad = ds.lodg.boxPad();
			int n = 0;
			for ( int r = 0; r < ds.lodg.recordCount(); r++ ) {
				const LodgRecord & R = ds.lodg.record( r );
				const LoddPiece & P = ds.lodd.piece( R.piece );
				if ( option == OptHybrid ) {
					// the cull the game would do: past the fade's far edge a decal weighs 0
					const float rad = R.scale * std::sqrt( qMax( P.x0 * P.x0, P.x1 * P.x1 ) + qMax( P.y0 * P.y0, P.y1 * P.y1 ) );
					if ( ( QVector3D( R.pos[0], R.pos[1], R.pos[2] ) - cam.eye ).length() - rad > fade1 )
						continue;
				}
				const float * a = &ds.atlasRect[size_t( R.piece ) * 4];
				const float rec[20] = {
					R.pos[0], R.pos[1], R.pos[2], R.scale,
					R.quat[0], R.quat[1], R.quat[2], R.quat[3],
					P.x0, P.y0, P.x1, P.y1,
					P.z0 - pad, P.z1 + pad, ds.texel[size_t( R.piece )], 0.0f,
					a[0], a[1], a[2], a[3] };
				buf.insert( buf.end(), rec, rec + 20 );
				n++;
			}
			gl->glBindBuffer( GL_SHADER_STORAGE_BUFFER, ssbo );
			gl->glBufferData( GL_SHADER_STORAGE_BUFFER, GLsizeiptr( qMax<size_t>( 80, buf.size() * 4 ) ),
				buf.empty() ? nullptr : buf.data(), GL_STATIC_DRAW );
			gl->glBindBufferBase( GL_SHADER_STORAGE_BUFFER, 0, ssbo );
			return n;
		};

		auto bindTex = [&]( GLuint p, const char * name, int unit, GLenum target, GLuint t ) {
			gl->glActiveTexture( GL_TEXTURE0 + unit );
			gl->glBindTexture( target, t );
			gl->glUniform1i( U( gl, p, name ), unit );
		};

		auto decalUniforms = [&]( GLuint p, int option ) {
			gl->glUseProgram( p );
			gl->glUniformMatrix4fv( U( gl, p, "uViewProj" ), 1, GL_FALSE, cam.vp.constData() );
			gl->glUniformMatrix4fv( U( gl, p, "uInvViewProj" ), 1, GL_FALSE, cam.inv.constData() );
			gl->glUniform2f( U( gl, p, "uViewport" ), float( W ), float( H ) );
			gl->glUniform3f( U( gl, p, "uEye" ), cam.eye.x(), cam.eye.y(), cam.eye.z() );
			gl->glUniform2f( U( gl, p, "uFade" ), fade0, fade1 );
			gl->glUniform1i( U( gl, p, "uOption" ), option );
			gl->glUniform1i( U( gl, p, "uOrtho" ), cam.ortho ? 1 : 0 );
			gl->glUniform1f( U( gl, p, "uPixelK" ), cam.pixelK );
			bindTex( p, "uDepth", 0, GL_TEXTURE_2D, tDepth );
		};

		auto frame = [&]( int option, int nDecals, int aoMode, float aoLod, bool timed ) {
			// 1. terrain
			if ( timed )
				gl->glBeginQuery( GL_TIME_ELAPSED, queries[0] );
			gl->glBindFramebuffer( GL_FRAMEBUFFER, fbo[0] );
			gl->glDisable( GL_BLEND );
			gl->glEnable( GL_DEPTH_TEST );
			gl->glDepthFunc( GL_LESS );
			gl->glDepthMask( GL_TRUE );
			gl->glEnable( GL_CULL_FACE );
			gl->glCullFace( GL_BACK );
			gl->glFrontFace( GL_CCW );
			gl->glClearColor( 0, 0, 0, 0 );
			gl->glClearDepth( 1.0 );
			gl->glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
			gl->glUseProgram( pTerrain );
			gl->glUniformMatrix4fv( U( gl, pTerrain, "uViewProj" ), 1, GL_FALSE, cam.vp.constData() );
			gl->glUniform2f( U( gl, pTerrain, "uOrigin" ), grid.origin[0], grid.origin[1] );
			gl->glUniform1f( U( gl, pTerrain, "uSpacing" ), grid.spacing );
			gl->glUniform2i( U( gl, pTerrain, "uGrid" ), grid.nx, grid.ny );
			gl->glUniform1f( U( gl, pTerrain, "uTile" ), lodgenLandTiling() );
			gl->glUniform4fv( U( gl, pTerrain, "uFullMap" ), 1, full.map );
			gl->glUniform4fv( U( gl, pTerrain, "uFarMap" ), 1, farS.map );
			gl->glUniform1i( U( gl, pTerrain, "uOption" ), option );
			gl->glUniform2f( U( gl, pTerrain, "uFade" ), fade0, fade1 );
			gl->glUniform3f( U( gl, pTerrain, "uEye" ), cam.eye.x(), cam.eye.y(), cam.eye.z() );
			bindTex( pTerrain, "uHeight", 0, GL_TEXTURE_2D, grid.height );
			bindTex( pTerrain, "uIdx", 1, GL_TEXTURE_2D, grid.idx );
			bindTex( pTerrain, "uW", 2, GL_TEXTURE_2D, grid.wts );
			bindTex( pTerrain, "uVclr", 3, GL_TEXTURE_2D, grid.vclr );
			bindTex( pTerrain, "uLtex", 4, GL_TEXTURE_2D_ARRAY, ltexArr );
			bindTex( pTerrain, "uFull", 5, GL_TEXTURE_2D, texFull );
			bindTex( pTerrain, "uFar", 6, GL_TEXTURE_2D, texFar );
			gl->glUniform1i( U( gl, pTerrain, "uLaw2" ), haveBlend ? 1 : 0 );
			gl->glUniform4fv( U( gl, pTerrain, "uVanMap" ), 1, blend.vMap );
			gl->glUniform4fv( U( gl, pTerrain, "uBlendMap" ), 1, blend.wMap );
			bindTex( pTerrain, "uVan", 7, GL_TEXTURE_2D, blend.vTex );
			bindTex( pTerrain, "uBlendW", 8, GL_TEXTURE_2D, blend.wTex );
			// the rule paint: on when the spec has a map and the view does not say "rule": false
			const bool ruleView = haveRule && haveBlend && V.value( "rule" ).toBool( true );
			gl->glUniform1i( U( gl, pTerrain, "uRule" ), ruleView ? 1 : 0 );
			gl->glUniform2f( U( gl, pTerrain, "uROrigin" ), rule.originX(), rule.originY() );
			gl->glUniform1f( U( gl, pTerrain, "uRSpacing" ), rule.spacing() );
			gl->glUniform2i( U( gl, pTerrain, "uRGrid" ), qMax( 1, rule.nx() ), qMax( 1, rule.ny() ) );
			bindTex( pTerrain, "uRIdx", 9, GL_TEXTURE_2D, ruleIdx );
			bindTex( pTerrain, "uRW", 10, GL_TEXTURE_2D, ruleW );
			bindTex( pTerrain, "uRG", 12, GL_TEXTURE_2D, ruleG );
			gl->glUniform1i( U( gl, pTerrain, "uLodaOn" ), lodaTex ? lodaOn : 0 );
			gl->glUniform4fv( U( gl, pTerrain, "uLodaMap" ), 1, lodaMap );
			bindTex( pTerrain, "uLoda", 11, GL_TEXTURE_2D, lodaTex );
			gl->glBindVertexArray( grid.vao );
			gl->glDrawElements( GL_TRIANGLES, grid.indexCount, GL_UNSIGNED_INT, nullptr );
			if ( timed )
				gl->glEndQuery( GL_TIME_ELAPSED );
			// 2. decals
			if ( timed )
				gl->glBeginQuery( GL_TIME_ELAPSED, queries[1] );
			if ( nDecals > 0 ) {
				gl->glBindFramebuffer( GL_FRAMEBUFFER, fbo[1] );
				gl->glDisable( GL_DEPTH_TEST );
				gl->glDepthMask( GL_FALSE );
				gl->glEnable( GL_CULL_FACE );
				gl->glCullFace( GL_FRONT );   // back faces: drawn whether or not the eye is inside the box
				gl->glEnable( GL_BLEND );
				gl->glBlendFuncSeparate( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ZERO, GL_ONE );
				decalUniforms( pDecal, option );
				bindTex( pDecal, "uAtlasC", 1, GL_TEXTURE_2D, ds.atlasC );
				bindTex( pDecal, "uAtlasN", 2, GL_TEXTURE_2D, ds.atlasN );
				gl->glUniform1f( U( gl, pDecal, "uMaxLod" ), ds.maxLod );
				gl->glUniform1i( U( gl, pDecal, "uDecalNormals" ), decalNormals ? 1 : 0 );
				gl->glBindVertexArray( emptyVao );
				gl->glDrawArraysInstanced( GL_TRIANGLES, 0, 36, nDecals );
			}
			if ( timed )
				gl->glEndQuery( GL_TIME_ELAPSED );
			// 3. lighting
			if ( timed )
				gl->glBeginQuery( GL_TIME_ELAPSED, queries[2] );
			gl->glBindFramebuffer( GL_FRAMEBUFFER, fbo[2] );
			gl->glDisable( GL_BLEND );
			gl->glDisable( GL_DEPTH_TEST );
			gl->glDisable( GL_CULL_FACE );
			gl->glUseProgram( pLight );
			bindTex( pLight, "uAlbedo", 0, GL_TEXTURE_2D, tAlb );
			bindTex( pLight, "uNormal", 1, GL_TEXTURE_2D, tNrm );
			bindTex( pLight, "uDepth", 2, GL_TEXTURE_2D, tDepth );
			bindTex( pLight, "uAo", 3, GL_TEXTURE_2D, texAo );
			gl->glUniformMatrix4fv( U( gl, pLight, "uInvViewProj" ), 1, GL_FALSE, cam.inv.constData() );
			gl->glUniform4fv( U( gl, pLight, "uAoMap" ), 1, aoS.map );
			gl->glUniform1f( U( gl, pLight, "uAoLod" ), aoLod );
			gl->glUniform1i( U( gl, pLight, "uAoMode" ), texAo ? aoMode : 0 );
			gl->glBindVertexArray( emptyVao );
			gl->glDrawArrays( GL_TRIANGLES, 0, 3 );
			if ( timed )
				gl->glEndQuery( GL_TIME_ELAPSED );
		};

		auto readOut = [&]() {
			QImage img( W, H, QImage::Format_RGBA8888 );
			gl->glBindFramebuffer( GL_FRAMEBUFFER, fbo[2] );
			gl->glPixelStorei( GL_PACK_ALIGNMENT, 4 );
			gl->glReadPixels( 0, 0, W, H, GL_RGBA, GL_UNSIGNED_BYTE, img.bits() );
			return img.mirrored( false, true ).convertToFormat( QImage::Format_RGB888 );
		};

		// per-pixel world distance from the eye, from the depth buffer (-1 = sky)
		auto distances = [&]( bool oursOnly ) {
			std::vector<float> d( size_t( W ) * H );
			gl->glBindFramebuffer( GL_FRAMEBUFFER, fbo[0] );
			gl->glReadPixels( 0, 0, W, H, GL_DEPTH_COMPONENT, GL_FLOAT, d.data() );
			std::vector<float> out( size_t( W ) * H, -1.0f );
			for ( int y = 0; y < H; y++ )
				for ( int x = 0; x < W; x++ ) {
					const float z = d[size_t( y ) * W + x];
					if ( z >= 1.0f )
						continue;
					const QVector4D ndc( ( float( x ) + 0.5f ) / float( W ) * 2.0f - 1.0f,
						( float( y ) + 0.5f ) / float( H ) * 2.0f - 1.0f, z * 2.0f - 1.0f, 1.0f );
					const QVector4D w = cam.inv * ndc;
					const QVector3D wp = w.toVector3D() / w.w();
					if ( oursOnly && haveBlend && blend.weightAt( wp.x(), wp.y() ) < 0.999f )
						continue;
					// image rows run top-down after the mirror
					out[size_t( H - 1 - y ) * W + x] = ( wp - cam.eye ).length();
				}
			return out;
		};

		QHash<QString, QImage> shots;   // option|ao -> picture, for the crossover and the side-by-sides
		auto render = [&]( int option, int aoMode, float aoLod, double * msT, double * msD, double * msL, double * msAll,
				int * boxes ) {
			const int n = uploadRecords( option );
			*boxes = n;
			frame( option, n, aoMode, aoLod, false );   // warm-up
			gl->glFinish();
			std::vector<double> t0, t1, t2, tt;
			for ( int f = 0; f < frames; f++ ) {
				frame( option, n, aoMode, aoLod, true );
				gl->glFinish();
				GLuint64 r[3] = { 0, 0, 0 };
				for ( int q = 0; q < 3; q++ )
					gl->glGetQueryObjectui64v( queries[q], GL_QUERY_RESULT, &r[q] );
				t0.push_back( double( r[0] ) / 1e6 );
				t1.push_back( double( r[1] ) / 1e6 );
				t2.push_back( double( r[2] ) / 1e6 );
				tt.push_back( double( r[0] + r[1] + r[2] ) / 1e6 );
			}
			*msT = median( t0 );
			*msD = median( t1 );
			*msL = median( t2 );
			*msAll = median( tt );
			return readOut();
		};

		const QJsonArray renders = V.value( "renders" ).toArray();
		for ( const QJsonValue & rv : renders ) {
			const QJsonObject R = rv.toObject();
			QStringList opts;
			for ( const QJsonValue & o : R.value( "options" ).toArray() )
				opts << o.toString();
			if ( opts.isEmpty() )
				opts << R.value( "option" ).toString();
			const int aoMode = R.value( "ao" ).toInt( 0 );
			const float aoLod = float( R.value( "ao_lod" ).toDouble( 0.0 ) );
			lodaOn = R.value( "loda" ).toBool( true ) ? 1 : 0;
			QList<QImage> panels;
			QStringList titles;
			for ( const QString & os : opts ) {
				int option = 0;
				if ( !parseOption( os, &option ) ) {
					say( QString( "view %1: REFUSED: unknown option '%2'" ).arg( vname, os ) );
					failures++;
					continue;
				}
				double a, b, c, t;
				int boxes = 0;
				const QImage img = render( option, aoMode, aoLod, &a, &b, &c, &t, &boxes );
				const GLenum err = gl->glGetError();
				say( QString( "view %1 option %2%3: %4 x %5 px, GPU ms terrain %6, decals %7, lighting %8, total %9 "
						"(median of %10 frames); %11 decal box(es) drawn%12" )
					.arg( vname, optionName( option ), ( aoMode ? QString( " ao %1 lod %2" ).arg( aoMode ).arg( double( aoLod ) ) : QString() )
						+ ( lodaTex ? ( lodaOn ? QStringLiteral( " loda on" ) : QStringLiteral( " loda off" ) ) : QString() ) )
					.arg( W ).arg( H ).arg( a, 0, 'f', 3 ).arg( b, 0, 'f', 3 ).arg( c, 0, 'f', 3 ).arg( t, 0, 'f', 3 )
					.arg( frames ).arg( boxes ).arg( err ? QString( "; GL ERROR 0x%1" ).arg( err, 0, 16 ) : QString() ) );
				if ( err )
					failures++;
				shots.insert( QString( "%1|%2|%3|%4" ).arg( option ).arg( aoMode ).arg( double( aoLod ) ).arg( lodaOn ), img );
				panels << img;
				const QStringList tl = R.value( "titles" ).toVariant().toStringList();
				titles << ( tl.value( int( panels.size() ) - 1 ).isEmpty()
					? QString( "%1 -- %2" ).arg( vname, optionName( option ).toUpper() )
					: tl.value( int( panels.size() ) - 1 ) );
			}
			const QString outPath = R.value( "out" ).toString();
			if ( !outPath.isEmpty() && !panels.isEmpty() ) {
				const QImage pic = titled( panels, titles );
				if ( pic.save( outPath ) )
					say( QString( "view %1: wrote %2 (%3 x %4)" ).arg( vname, outPath ).arg( pic.width() ).arg( pic.height() ) );
				else {
					say( QString( "view %1: FAILED to write %2" ).arg( vname, outPath ) );
					failures++;
				}
			}
		}

		// box layers: fragments the decal pass rasterises, and those inside a box, per ground pixel
		if ( V.value( "box_count" ).toBool( false ) && ds.ok ) {
			for ( int option : { int( OptHybrid ), int( OptDynamic ) } ) {
				const int n = uploadRecords( option );
				frame( option, 0, 0, 0.0f, false );
				gl->glBindFramebuffer( GL_FRAMEBUFFER, fbo[3] );
				gl->glClearColor( 0, 0, 0, 0 );
				gl->glClear( GL_COLOR_BUFFER_BIT );
				gl->glDisable( GL_DEPTH_TEST );
				gl->glEnable( GL_CULL_FACE );
				gl->glCullFace( GL_FRONT );
				gl->glEnable( GL_BLEND );
				gl->glBlendFunc( GL_ONE, GL_ONE );
				decalUniforms( pCount, option );
				gl->glBindVertexArray( emptyVao );
				if ( n )
					gl->glDrawArraysInstanced( GL_TRIANGLES, 0, 36, n );
				gl->glFinish();
				std::vector<float> rg( size_t( W ) * H * 2 );
				gl->glReadPixels( 0, 0, W, H, GL_RG, GL_FLOAT, rg.data() );
				const std::vector<float> dist = distances( false );
				double frag = 0, inside = 0, ground = 0, maxLayers = 0;
				for ( size_t i = 0; i < dist.size(); i++ ) {
					// rg is bottom-up, dist top-down
					const size_t y = i / size_t( W ), x = i % size_t( W );
					const size_t j = ( size_t( H ) - 1 - y ) * size_t( W ) + x;
					if ( dist[j] < 0.0f )
						continue;
					ground++;
					frag += rg[i * 2];
					inside += rg[i * 2 + 1];
					maxLayers = qMax( maxLayers, double( rg[i * 2] ) );
				}
				say( QString( "view %1 option %2: box layers per ground pixel: %3 rasterised, %4 inside a box, max %5, "
						"over %6 ground pixel(s), %7 box(es)" )
					.arg( vname, optionName( option ) ).arg( frag / qMax( 1.0, ground ), 0, 'f', 3 )
					.arg( inside / qMax( 1.0, ground ), 0, 'f', 3 ).arg( maxLayers ).arg( ground ).arg( n ) );
			}
		}

		// the crossover: past which distance does the hybrid's baked level match FULL within the threshold?
		const QJsonObject X = V.value( "crossover" ).toObject();
		if ( !X.isEmpty() ) {
			const double thr = X.value( "threshold" ).toDouble( 4.0 );
			const double bin = X.value( "bin" ).toDouble( 1024.0 );
			double a, b, c, t;
			int boxes;
			const QImage imFull = render( OptFull, 0, 0.0f, &a, &b, &c, &t, &boxes );
			const QImage imBaked = render( OptBaked, 0, 0.0f, &a, &b, &c, &t, &boxes );
			const QImage imDyn = render( OptDynamic, 0, 0.0f, &a, &b, &c, &t, &boxes );
			const QImage imLive = render( OptLive, 0, 0.0f, &a, &b, &c, &t, &boxes );
			const std::vector<float> dist = distances( true );   // ground that is wholly ours
			float dmax = 0.0f;
			for ( float d : dist )
				dmax = qMax( dmax, d );
			const int nb = qMax( 1, int( std::ceil( double( dmax ) / bin ) ) );
			std::vector<double> sB( size_t( nb ), 0 ), sD( size_t( nb ), 0 ), sL( size_t( nb ), 0 ), cnt( size_t( nb ), 0 );
			for ( int y = 0; y < H; y++ ) {
				const uchar * f = imFull.constScanLine( y );
				const uchar * bb = imBaked.constScanLine( y );
				const uchar * dd = imDyn.constScanLine( y );
				const uchar * ll = imLive.constScanLine( y );
				for ( int x = 0; x < W; x++ ) {
					const float d = dist[size_t( y ) * W + x];
					if ( d < 0.0f )
						continue;
					const int k = qMin( nb - 1, int( double( d ) / bin ) );
					double eB = 0, eD = 0, eL = 0;
					for ( int ch = 0; ch < 3; ch++ ) {
						eB += std::abs( int( bb[x * 3 + ch] ) - int( f[x * 3 + ch] ) );
						eD += std::abs( int( dd[x * 3 + ch] ) - int( f[x * 3 + ch] ) );
						eL += std::abs( int( ll[x * 3 + ch] ) - int( f[x * 3 + ch] ) );
					}
					sB[size_t( k )] += eB / 3.0;
					sD[size_t( k )] += eD / 3.0;
					sL[size_t( k )] += eL / 3.0;
					cnt[size_t( k )] += 1;
				}
			}
			const QString csv = X.value( "csv" ).toString();
			QFile cf( csv );
			QTextStream cs( &cf );
			const bool haveCsv = !csv.isEmpty() && cf.open( QIODevice::WriteOnly | QIODevice::Truncate );
			if ( !csv.isEmpty() && !haveCsv ) {
				say( QStringLiteral( "FAILED: cannot write " ) + csv );
				failures++;
			}
			if ( haveCsv )
				cs << "bin_start_u,pixels,baked_vs_full,dynamic_vs_full,live_vs_full\n";
			int crossBin = -1;
			for ( int k = nb - 1; k >= 0; k-- ) {
				if ( cnt[size_t( k )] < X.value( "min_pixels" ).toDouble( 200.0 ) )
					continue;
				if ( sB[size_t( k )] / cnt[size_t( k )] > thr )
					break;
				crossBin = k;
			}
			for ( int k = 0; k < nb; k++ ) {
				if ( !cnt[size_t( k )] )
					continue;
				const double n = cnt[size_t( k )];
				if ( haveCsv )
					cs << k * bin << ',' << n << ',' << sB[size_t( k )] / n << ',' << sD[size_t( k )] / n << ','
					   << sL[size_t( k )] / n << '\n';
			}
			if ( haveCsv )
				cf.close();
			QStringList row;
			for ( int k = 0; k < nb; k++ )
				if ( cnt[size_t( k )] >= 200 )
					row << QString( "%1:%2/%3" ).arg( k * bin / 1024.0, 0, 'f', 0 )
						.arg( sB[size_t( k )] / cnt[size_t( k )], 0, 'f', 1 ).arg( sD[size_t( k )] / cnt[size_t( k )], 0, 'f', 1 );
			say( QString( "view %1: crossover on ground wholly ours (mean |baked hybrid level - the 16 u reference| per pixel, 0..255, bins of %2 u): "
					"%3; threshold %4 -> %5" )
				.arg( vname ).arg( bin ).arg( crossBin >= 0 ? QString( "met from %1 u on" ).arg( crossBin * bin ) : QString( "never met" ) )
				.arg( thr ).arg( csv.isEmpty() ? QString( "no csv" ) : csv ) );
			say( QString( "view %1: per bin (k-units: baked/dynamic vs the 16 u reference): %2" ).arg( vname, row.join( ' ' ) ) );
		}

		// cleanup
		gl->glDeleteBuffers( 1, &ssbo );
		gl->glDeleteFramebuffers( 4, fbo );
		GLuint tt[5] = { tAlb, tNrm, tDepth, tOut, tCount };
		gl->glDeleteTextures( 5, tt );
		GLuint bt[8] = { texFull, texFar, texAo, ds.atlasC, ds.atlasN, blend.vTex, blend.wTex, lodaTex };
		gl->glDeleteTextures( 8, bt );
		freeGrid( gl, grid );
	}
	if ( haveRule ) {
		GLuint rt[3] = { ruleIdx, ruleW, ruleG };
		gl->glDeleteTextures( 3, rt );
	}
	ctx.doneCurrent();
	say( failures ? QString( "%1 failure(s)" ).arg( failures ) : QStringLiteral( "done, no failures" ) );
	return failures ? 1 : 0;
}
