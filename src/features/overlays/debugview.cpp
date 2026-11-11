#include "features/overlays/debugview.h"
#include "cellsplat.h"
#include "gl/glnode.h"
#include "gl/glscene.h"
#include "wwswitch.h"

#include <QString>

#include <cstdio>

namespace {
// ids match res/shaders/features/overlays/debug_view.glsl; owners = the passes that write the view
struct DvView {
	const char * name;
	const char * owners;
};
const DvView kViews[] = {
	{ "off", "the shipping frame" },
	{ "albedo", "near objects + near land (fo4_default, pbrm_default), far terrain + far objects (same programs), tree cards (impostor_oct); water: black" },
	{ "normals", "near + far surfaces (world normal), tree cards (the card's normal), water (the wave normal)" },
	{ "gloss", "fo4_default: smoothness / spec mask; pbrm_default: 1 - roughness / metal; tree cards and water: black" },
	{ "ao", "baked (vertex / far) / screen (SSAO) / product, cell_lights.glsl cellAoFactor; tree cards: baked only" },
	{ "sun", "the key light's diffuse share of cellLit (cell_lights.glsl) and of pbrm_default's sun; false colour log10 cd/m2" },
	{ "shadow", "the sun's cascades (wwSunShadowGeo), surfaces under WW_SUNSHADOW; tree cards: 1 (no cascades)" },
	{ "ambient", "cellLit's DALC ambient + air diffuse (exterior) or the interior DALC; pbrm_default: ambient + GI + lights, one sum" },
	{ "gi", "cellLit's GI term (cellGi*; the far tree cards through cellLit, pbrm_default its giDiff); the water: black" },
	{ "emissive", "the applied glow (emissive x glowScale) of fo4_default / pbrm_default; tree cards and water: black" },
	{ "fog", "red = 1 - the fog's transmittance, green = its in-scatter share (wwFog, the aerial perspective) on every surface" },
	{ "air", "black: owed (the volume air is its own pass, cellvolair.cpp, not a surface term)" },
	{ "clouds", "black: owed (the sky and its clouds are not drawn in a probe pass)" },
	{ "cloudshadow", "the clouds' transmittance toward the sun (ussCloudT) on surfaces with the sun's shadow and on the tree cards" },
	{ "water", "the water tiers: cyan near water, violet far water, teal endless sea, x the far fade (fo4_water); surfaces black" },
	{ "waterrefl", "fo4_water: red = Fresnel, green = the far sky colour's weight, blue = the far fade (the near/far swap)" },
	{ "waterheight", "fo4_water: the view ray's angle under the horizon (degrees / 2 on the ramp), where the flat band sits" },
	{ "farowner", "who drew the pixel: grey near object, green near land, orange far terrain, blue far objects, yellow tree card, cyan near water, violet far water, teal endless sea; black = no surface (sky)" },
	{ "exposure", "the lit colour after the fog (the exposure's input), false colour log10 cd/m2, every surface and the water" },
	{ "depth", "the distance from the eye on a log ramp (2 u blue .. 4 M u red), every surface and the water" },
	{ "overdraw", "black: owed (needs an additive pass with the depth test off)" },
	{ "specular", "cellLit's sun specular (fo4_default) / pbrm_default's direct specular" },
	{ "reflection", "the environment / reflection term (cubeK x the cube) of cellLit and pbrm_default" },
	{ "watersky", "fo4_waterlook: the sky each water tier mirrors before the march (x / (1 + x)); tiers by farowner; surfaces black" },
};
constexpr int kCount = int( sizeof( kViews ) / sizeof( kViews[0] ) );

int g_menu = 0;
int g_said = -1;

int parse( const QByteArray & v )
{
	const QByteArray t = v.trimmed().toLower();
	if ( t.isEmpty() )
		return 0;
	bool ok = false;
	const int n = t.toInt( &ok );
	if ( ok )
		return n >= 0 && n < kCount ? n : 0;
	for ( int k = 0; k < kCount; k++ )
		if ( t == kViews[k].name )
			return k;
	return 0;
}
}

bool wwDebugViewPinned()
{
	return !wwEnv( "WW_DEBUG_VIEW" ).trimmed().isEmpty();
}

int wwDebugViewId()
{
	WW_SWITCH_STATIC( int, pin, wwEnvIsSet( "WW_DEBUG_VIEW" ) ? parse( wwEnv( "WW_DEBUG_VIEW" ) ) : -1 );
	return pin >= 0 && wwDebugViewPinned() ? pin : g_menu;
}

bool wwDebugViewShaderDefine()
{
	return wwDebugViewId() != 0;
}

void wwDebugViewSet( int id )
{
	g_menu = id >= 0 && id < kCount ? id : 0;
}

QStringList wwDebugViewNames()
{
	QStringList n;
	for ( int k = 0; k < kCount; k++ ) {
		QString s = QString::fromLatin1( kViews[k].name );
		s[0] = s[0].toUpper();
		n << s;
	}
	return n;
}

int wwDebugViewOwner( Scene * scene, const Node * mesh )
{
	for ( const Node * p = mesh; p; p = p->parentNode() ) {
		const QString & n = p->getName();
		if ( n.startsWith( QLatin1StringView( "FarLOD terrain" ) ) )
			return 2;
		if ( n.startsWith( QLatin1StringView( "FarLOD water" ) ) || n.startsWith( QLatin1StringView( "FarLOD sea" ) ) )
			return 6;
		if ( n.startsWith( QLatin1StringView( "FarLOD" ) ) )
			return 3;
	}
	return scene && mesh && wwCellLandIs( scene->nifModel, mesh->id() ) ? 1 : 0;
}

void wwDebugViewFrame()
{
	const int id = wwDebugViewId();
	if ( id == g_said )
		return;
	if ( g_said < 0 && id == 0 ) {	// never on: no line, the shipping log
		g_said = 0;
		return;
	}
	g_said = id;
	std::fprintf( stderr, "debug view: %s (%s)%s\n", kViews[id].name, kViews[id].owners,
		wwDebugViewPinned() ? " [WW_DEBUG_VIEW]" : " [Show > Debug view]" );
	std::fflush( stderr );
}
