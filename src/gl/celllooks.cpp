#include "celllooks.h"

#include "wwswitch.h"
#include "gl/lightphys.h"

#include <QSettings>

#include <algorithm>

namespace {

struct LookRow
{
	const char * label;
	const char * key;
};

// the order of WwLook
const LookRow kRows[] = {
	{ "Fog light clusters", "WW/CellAirClusters" },
	{ "Fog multiple scattering", "WW/CellAirMultiScatter" },
	{ "Ground fog burn-off", "WW/CellAirBurnOff" },
	{ "Far contact shadows", "WW/CellContactFar" },
	{ "Lamp contact shadows", "WW/CellContactLocal" },
	{ "Moonlit terrain shadows", "WW/ShadowMoonSky" },
	{ "PBR ambient in terrain shadow", "WW/ShadowPbrDim" },
	{ "Bloom width in fog", "WW/CellBloomPerPixel" },
	{ "Real sun and moon", "WW/SkyEphemeris" },
	{ "Actors outdoors", "WW/CellActorsOutdoors" },
	{ "Weapon mod parts", "WW/CellWeaponParts" },
	{ "World animation", "WW/CellWorldAnim" },
	{ "Animated objects", "WW/CellAnimObjects" },
	{ "Wide exposure range", "WW/CellExposureWide" },
	{ "Probe sharing", "WW/CellGiShare" },
	{ "Decal distance fade", "WW/CellDecalFade" },
	{ "Decal parallax", "WW/CellDecalParallax" },
	{ "Terrain shadow slope from heights", "WW/ShadowSlopeMap" },
	{ "Object LOD shadows on ground only", "WW/ShadowGroundGate" },
	{ "Cable shadows sway", "WW/CableShadowSway" },
	{ "Game LOD trees sway", "WW/FarLodBtoSway" },
	{ "Far tree card shadows", "WW/ShadowFarCards" },
	{ "Placement emittance", "WW/CellPlacementEmittance" },
	{ "Region emittance", "WW/CellRegionEmittance" },
	{ "Emittance on particles", "WW/CellParticleEmittance" },
	{ "Emittance in bounce light", "WW/CellBounceEmittance" },
	{ "Lens flares", "WW/CellLensFlares" },
	{ "Wide probe sharing", "WW/CellGiShareWide" },
	{ "Probe depth visibility", "WW/CellGiDepthVis" },
	{ "Probe light smoothing", "WW/CellGiSmooth" },
	{ "Probes out of walls", "WW/CellProbeClear" },
	{ "Physical light falloff", "WW/CellLightPhys" },
	{ "Advanced light model", "WW/CellLightAdvanced" },
	{ "Area lights", "WW/CellLightArea" },
	{ "Light patterns", "WW/CellLightPatterns" },
	{ "Mesh lights (.nifx)", "WW/CellNifxLights" },
	{ "Water depth colour", "WW/CellWaterDepth" },
	{ "Water caustics", "WW/CellWaterCaustics" },
	{ "Water soft edge", "WW/CellWaterEdge" },
	{ "Shore foam", "WW/CellWaterFoam" },
	{ "Water swell", "WW/CellWaterWaves" },
	{ "Water reflections", "WW/CellWaterRefl" },
	{ "Water sun glare", "WW/CellWaterGlint" },
	{ "Ocean waves", "WW/CellWaterOcean" },
	{ "Underwater fog", "WW/CellUnderwater" },
	{ "Water clear colour", "WW/CellWaterClear" },
};
static_assert( sizeof( kRows ) / sizeof( kRows[0] ) == size_t( WwLook::Count ), "a row per look" );

// the row's value: read once from the settings, then whatever the menu sets
int & rowVal( WwLook l )
{
	static int v[size_t( WwLook::Count )];
	static bool loaded = false;
	if ( !loaded ) {
		loaded = true;
		QSettings s;
		for ( int k = 0; k < int( WwLook::Count ); k++ )
			v[k] = s.value( QLatin1String( kRows[k].key ), false ).toBool() ? 1 : 0;
	}
	return v[size_t( l )];
}

// the pin (literal names, so tools/wwswitch_survey.py sees each; re-read on a live set)
QByteArray pinOf( WwLook l )
{
	switch ( l ) {
	case WwLook::AirClusters: {
		WW_SWITCH_STATIC( QByteArray, p, wwEnv( "WW_CELL_AIR_CLUSTERS" ).trimmed() );
		return p;
	}
	case WwLook::AirMultiScatter: {
		WW_SWITCH_STATIC( QByteArray, p, wwEnv( "WW_CELL_AIR_MS" ).trimmed() );
		return p;
	}
	case WwLook::GroundBurnOff: {
		WW_SWITCH_STATIC( QByteArray, p, wwEnv( "WW_CELL_AIR_BURNOFF" ).trimmed() );
		return p;
	}
	case WwLook::ContactFar: {
		WW_SWITCH_STATIC( QByteArray, p, wwEnv( "WW_CELL_CONTACT_FAR" ).trimmed() );
		return p;
	}
	case WwLook::ContactLocal: {
		WW_SWITCH_STATIC( QByteArray, p, wwEnv( "WW_CELL_CONTACT_LOCAL" ).trimmed() );
		return p;
	}
	case WwLook::TerrMoonSky: {
		WW_SWITCH_STATIC( QByteArray, p, wwEnv( "WW_SHADOW_MOONSKY" ).trimmed() );
		return p;
	}
	case WwLook::TerrPbrDim: {
		WW_SWITCH_STATIC( QByteArray, p, wwEnv( "WW_SHADOW_PBRDIM" ).trimmed() );
		return p;
	}
	case WwLook::BloomMip: {
		WW_SWITCH_STATIC( QByteArray, p, wwEnv( "WW_CELL_BLOOM_PERPIX" ).trimmed() );
		return p;
	}
	case WwLook::Ephemeris: {
		WW_SWITCH_STATIC( QByteArray, p, wwEnv( "WW_SKY_EPHEM" ).trimmed() );
		return p;
	}
	case WwLook::ActorsOutdoors: {
		WW_SWITCH_STATIC( QByteArray, p, wwEnv( "WW_CELL_ACTORS_OUT" ).trimmed() );
		return p;
	}
	case WwLook::WeaponParts: {
		WW_SWITCH_STATIC( QByteArray, p, wwEnv( "WW_CELL_WEAP_PARTS" ).trimmed() );
		return p;
	}
	case WwLook::WorldAnim: {
		WW_SWITCH_STATIC( QByteArray, p, wwEnv( "WW_CELL_WORLDANIM" ).trimmed() );
		return p;
	}
	case WwLook::AnimObjects: {
		WW_SWITCH_STATIC( QByteArray, p, wwEnv( "WW_CELL_ANIMOBJ" ).trimmed() );
		return p;
	}
	case WwLook::ExposureWide: {
		WW_SWITCH_STATIC( QByteArray, p, wwEnv( "WW_CELL_EXPOSURE_WIDE" ).trimmed() );
		return p;
	}
	case WwLook::ProbeShare: {
		WW_SWITCH_STATIC( QByteArray, p, wwEnv( "WW_CELL_GI_SHARE" ).trimmed() );
		return p;
	}
	case WwLook::DecalFade: {
		WW_SWITCH_STATIC( QByteArray, p, wwEnv( "WW_CELL_DECAL_FADE" ).trimmed() );
		return p;
	}
	case WwLook::DecalParallax: {
		WW_SWITCH_STATIC( QByteArray, p, wwEnv( "WW_CELL_DECAL_POM" ).trimmed() );
		return p;
	}
	case WwLook::TerrSlopeMap: {
		WW_SWITCH_STATIC( QByteArray, p, wwEnv( "WW_SHADOW_SLOPEMAP" ).trimmed() );
		return p;
	}
	case WwLook::TerrGroundGate: {
		WW_SWITCH_STATIC( QByteArray, p, wwEnv( "WW_SHADOW_GROUNDGATE" ).trimmed() );
		return p;
	}
	case WwLook::CableShadowSway: {
		WW_SWITCH_STATIC( QByteArray, p, wwEnv( "WW_CELL_CABLES_SHADOWSWAY" ).trimmed() );
		return p;
	}
	case WwLook::BtoSway: {
		WW_SWITCH_STATIC( QByteArray, p, wwEnv( "WW_CELL_FARLOD_BTOSWAY" ).trimmed() );
		return p;
	}
	case WwLook::FarCardShadows: {
		WW_SWITCH_STATIC( QByteArray, p, wwEnv( "WW_SHADOW_FARCARDS" ).trimmed() );
		return p;
	}
	case WwLook::PlacementEmittance: {
		WW_SWITCH_STATIC( QByteArray, p, wwEnv( "WW_CELL_XEMI" ).trimmed() );
		return p;
	}
	case WwLook::RegionEmittance: {
		WW_SWITCH_STATIC( QByteArray, p, wwEnv( "WW_CELL_XEMI_REGN" ).trimmed() );
		return p;
	}
	case WwLook::ParticleEmittance: {
		WW_SWITCH_STATIC( QByteArray, p, wwEnv( "WW_CELL_XEMI_PART" ).trimmed() );
		return p;
	}
	case WwLook::BounceEmittance: {
		WW_SWITCH_STATIC( QByteArray, p, wwEnv( "WW_CELL_XEMI_GI" ).trimmed() );
		return p;
	}
	case WwLook::LensFlares: {
		WW_SWITCH_STATIC( QByteArray, p, wwEnv( "WW_CELL_LENSFLARE" ).trimmed() );
		return p;
	}
	case WwLook::ProbeShareWide: {
		WW_SWITCH_STATIC( QByteArray, p, wwEnv( "WW_CELL_GI_SHARE_WIDE" ).trimmed() );
		return p;
	}
	case WwLook::ProbeDepthVis: {
		WW_SWITCH_STATIC( QByteArray, p, wwEnv( "WW_CELL_GI_DEPTHVIS" ).trimmed() );
		return p;
	}
	case WwLook::ProbeSmooth: {
		WW_SWITCH_STATIC( QByteArray, p, wwEnv( "WW_CELL_GI_SMOOTH" ).trimmed() );
		return p;
	}
	case WwLook::ProbeClear: {
		WW_SWITCH_STATIC( QByteArray, p, wwEnv( "WW_CELL_PROBE_CLEAR" ).trimmed() );
		return p;
	}
	case WwLook::LightPhysFalloff: {
		WW_SWITCH_STATIC( QByteArray, p, wwEnv( "WW_CELL_LIGHT_PHYS" ).trimmed() );
		return p;
	}
	case WwLook::LightAdvanced: {
		WW_SWITCH_STATIC( QByteArray, p, wwEnv( "WW_CELL_LIGHT_ADV" ).trimmed() );
		return p;
	}
	case WwLook::LightArea: {
		WW_SWITCH_STATIC( QByteArray, p, wwEnv( "WW_CELL_LIGHT_AREA" ).trimmed() );
		return p;
	}
	case WwLook::LightPatterns: {
		WW_SWITCH_STATIC( QByteArray, p, wwEnv( "WW_CELL_GOBO" ).trimmed() );
		return p;
	}
	case WwLook::NifxLights: {
		WW_SWITCH_STATIC( QByteArray, p, wwEnv( "WW_CELL_NIFX_LIGHTS" ).trimmed() );
		return p;
	}
	case WwLook::WaterDepth: {
		WW_SWITCH_STATIC( QByteArray, p, wwEnv( "WW_CELL_WATER_DEPTH" ).trimmed() );
		return p;
	}
	case WwLook::WaterCaustics: {
		WW_SWITCH_STATIC( QByteArray, p, wwEnv( "WW_CELL_WATER_CAUSTICS" ).trimmed() );
		return p;
	}
	case WwLook::WaterSoftEdge: {
		WW_SWITCH_STATIC( QByteArray, p, wwEnv( "WW_CELL_WATER_EDGE" ).trimmed() );
		return p;
	}
	case WwLook::ShoreFoam: {
		WW_SWITCH_STATIC( QByteArray, p, wwEnv( "WW_CELL_WATER_FOAM" ).trimmed() );
		return p;
	}
	case WwLook::WaterSwell: {
		WW_SWITCH_STATIC( QByteArray, p, wwEnv( "WW_CELL_WATER_WAVES" ).trimmed() );
		return p;
	}
	case WwLook::WaterRefl: {
		WW_SWITCH_STATIC( QByteArray, p, wwEnv( "WW_CELL_WATER_REFL" ).trimmed() );
		return p;
	}
	case WwLook::WaterGlint: {
		WW_SWITCH_STATIC( QByteArray, p, wwEnv( "WW_CELL_WATER_GLINT" ).trimmed() );
		return p;
	}
	case WwLook::WaterOcean: {
		WW_SWITCH_STATIC( QByteArray, p, wwEnv( "WW_CELL_WATER_OCEAN" ).trimmed() );
		return p;
	}
	case WwLook::Underwater: {
		WW_SWITCH_STATIC( QByteArray, p, wwEnv( "WW_CELL_UNDERWATER" ).trimmed() );
		return p;
	}
	case WwLook::WaterClear: {
		WW_SWITCH_STATIC( QByteArray, p, wwEnv( "WW_CELL_WATER_CLEAR" ).trimmed() );
		return p;
	}
	default:
		return QByteArray();
	}
}

}	// namespace

const char * wwLookLabel( WwLook l )
{
	return kRows[size_t( l )].label;
}

const char * wwLookPinName( WwLook l )
{
	switch ( l ) {
	case WwLook::AirClusters: return "WW_CELL_AIR_CLUSTERS";
	case WwLook::AirMultiScatter: return "WW_CELL_AIR_MS";
	case WwLook::GroundBurnOff: return "WW_CELL_AIR_BURNOFF";
	case WwLook::ContactFar: return "WW_CELL_CONTACT_FAR";
	case WwLook::ContactLocal: return "WW_CELL_CONTACT_LOCAL";
	case WwLook::TerrMoonSky: return "WW_SHADOW_MOONSKY";
	case WwLook::TerrPbrDim: return "WW_SHADOW_PBRDIM";
	case WwLook::BloomMip: return "WW_CELL_BLOOM_PERPIX";
	case WwLook::Ephemeris: return "WW_SKY_EPHEM";
	case WwLook::ActorsOutdoors: return "WW_CELL_ACTORS_OUT";
	case WwLook::WeaponParts: return "WW_CELL_WEAP_PARTS";
	case WwLook::WorldAnim: return "WW_CELL_WORLDANIM";
	case WwLook::AnimObjects: return "WW_CELL_ANIMOBJ";
	case WwLook::ExposureWide: return "WW_CELL_EXPOSURE_WIDE";
	case WwLook::ProbeShare: return "WW_CELL_GI_SHARE";
	case WwLook::DecalFade: return "WW_CELL_DECAL_FADE";
	case WwLook::DecalParallax: return "WW_CELL_DECAL_POM";
	case WwLook::TerrSlopeMap: return "WW_SHADOW_SLOPEMAP";
	case WwLook::TerrGroundGate: return "WW_SHADOW_GROUNDGATE";
	case WwLook::CableShadowSway: return "WW_CELL_CABLES_SHADOWSWAY";
	case WwLook::BtoSway: return "WW_CELL_FARLOD_BTOSWAY";
	case WwLook::FarCardShadows: return "WW_SHADOW_FARCARDS";
	case WwLook::PlacementEmittance: return "WW_CELL_XEMI";
	case WwLook::RegionEmittance: return "WW_CELL_XEMI_REGN";
	case WwLook::ParticleEmittance: return "WW_CELL_XEMI_PART";
	case WwLook::BounceEmittance: return "WW_CELL_XEMI_GI";
	case WwLook::LensFlares: return "WW_CELL_LENSFLARE";
	case WwLook::ProbeShareWide: return "WW_CELL_GI_SHARE_WIDE";
	case WwLook::ProbeDepthVis: return "WW_CELL_GI_DEPTHVIS";
	case WwLook::ProbeSmooth: return "WW_CELL_GI_SMOOTH";
	case WwLook::ProbeClear: return "WW_CELL_PROBE_CLEAR";
	case WwLook::LightPhysFalloff: return "WW_CELL_LIGHT_PHYS";
	case WwLook::LightAdvanced: return "WW_CELL_LIGHT_ADV";
	case WwLook::LightArea: return "WW_CELL_LIGHT_AREA";
	case WwLook::LightPatterns: return "WW_CELL_GOBO";
	case WwLook::NifxLights: return "WW_CELL_NIFX_LIGHTS";
	case WwLook::WaterDepth: return "WW_CELL_WATER_DEPTH";
	case WwLook::WaterCaustics: return "WW_CELL_WATER_CAUSTICS";
	case WwLook::WaterSoftEdge: return "WW_CELL_WATER_EDGE";
	case WwLook::ShoreFoam: return "WW_CELL_WATER_FOAM";
	case WwLook::WaterSwell: return "WW_CELL_WATER_WAVES";
	case WwLook::WaterRefl: return "WW_CELL_WATER_REFL";
	case WwLook::WaterGlint: return "WW_CELL_WATER_GLINT";
	case WwLook::WaterOcean: return "WW_CELL_WATER_OCEAN";
	case WwLook::Underwater: return "WW_CELL_UNDERWATER";
	case WwLook::WaterClear: return "WW_CELL_WATER_CLEAR";
	default: return "";
	}
}

bool wwLookPinned( WwLook l )
{
	return !pinOf( l ).isEmpty();
}

bool wwLookReopens( WwLook l )
{
	// BtoSway rewrites the .bto shapes' vertices while the far field is built
	// ProbeClear moves probes at bake time: the cell's own bake runs again when it reopens
	// lane LIGHTS4: the placement, region and particle emittance rows are read at draw time (live)
	// lane LIGHTS5: the bounce emittance row relights from the bake's kept other state (probesky.cpp)
	return l == WwLook::ActorsOutdoors || l == WwLook::WeaponParts || l == WwLook::AnimObjects || l == WwLook::BtoSway
		|| l == WwLook::ProbeClear;
}

bool wwLookHasRow( WwLook l )
{
	return l != WwLook::AirClusters;
}

bool wwLookBounceEmitRedStale()
{
	WW_SWITCH_STATIC( bool, stale, wwEnv( "WW_CELL_XEMI_GI_RED" ).trimmed() == "stale" );
	return stale;
}

bool wwLookOn( WwLook l )
{
	const QByteArray p = pinOf( l );
	if ( p.isEmpty() )
		return wwLookHasRow( l ) ? rowVal( l ) != 0 : true;
	return p != "0";
}

void wwLookSet( WwLook l, bool on )
{
	if ( wwLookPinned( l ) || !wwLookHasRow( l ) )
		return;
	rowVal( l ) = on ? 1 : 0;
	QSettings().setValue( QLatin1String( kRows[size_t( l )].key ), on );
}

// ---- LIGHTSYS-3
namespace {
int & budgetRow()
{
	static int v = -1;
	if ( v < 0 )
		v = QSettings().value( QStringLiteral( "WW/CellShadowBudget" ), 0 ).toInt();
	return v;
}
int budgetClamp( int n )
{
	return n <= 0 ? 0 : n <= 4 ? 4 : n <= 8 ? 8 : 16;
}
}	// namespace

bool wwCellShadowBudgetPinned()
{
	WW_SWITCH_STATIC( QByteArray, p, wwEnv( "WW_CELL_SHADOWBUDGET" ).trimmed() );
	return !p.isEmpty();
}

int wwCellShadowBudget()
{
	WW_SWITCH_STATIC( QByteArray, p, wwEnv( "WW_CELL_SHADOWBUDGET" ).trimmed() );
	return budgetClamp( p.isEmpty() ? budgetRow() : p.toInt() );
}

void wwCellShadowBudgetSet( int n )
{
	if ( wwCellShadowBudgetPinned() )
		return;
	budgetRow() = budgetClamp( n );
	QSettings().setValue( QStringLiteral( "WW/CellShadowBudget" ), budgetRow() );
}

bool wwCellShadowBudgetRedWorst()
{
	WW_SWITCH_STATIC( QByteArray, r, wwEnv( "WW_CELL_SHADOWBUDGET_RED" ).trimmed() );
	return r == "worst";
}

// ---- LIGHTPHYS1
namespace {
float & curveRow()
{
	static float v = -1.0f;
	if ( v < 0.0f )
		v = std::max( QSettings().value( QStringLiteral( "WW/CellLightCurve" ), 1.0 ).toFloat(), 0.0f );
	return v;
}
}	// namespace

bool wwCellLightCurvePinned()
{
	WW_SWITCH_STATIC( QByteArray, p, wwEnv( "WW_CELL_LIGHT_CURVE" ).trimmed() );
	return !p.isEmpty();
}

float wwCellLightCurve()
{
	WW_SWITCH_STATIC( QByteArray, p, wwEnv( "WW_CELL_LIGHT_CURVE" ).trimmed() );
	const float c = p.isEmpty() ? curveRow() : p.toFloat();
	return c > 0.0f ? c : 1.0f;
}

void wwCellLightCurveSet( float curve )
{
	if ( wwCellLightCurvePinned() )
		return;
	curveRow() = curve > 0.0f ? curve : 1.0f;
	QSettings().setValue( QStringLiteral( "WW/CellLightCurve" ), double( curveRow() ) );
}

// ---- LIGHTPHYS3: the Advanced model's calibration (FO4CS fCalibrationScale, src/Lights/AdvancedLighting.h: 0.05-20, 1)
namespace {
float clampCalib( float v )
{
	return v > 0.0f ? std::min( std::max( v, 0.05f ), 20.0f ) : 1.0f;
}

float & calibRow()
{
	static float v = -1.0f;
	if ( v < 0.0f )
		v = clampCalib( QSettings().value( QStringLiteral( "WW/CellLightCalibration" ), 1.0 ).toFloat() );
	return v;
}
}	// namespace

bool wwCellLightCalibrationPinned()
{
	WW_SWITCH_STATIC( QByteArray, p, wwEnv( "WW_CELL_LIGHT_ADV_CALIB" ).trimmed() );
	return !p.isEmpty();
}

float wwCellLightCalibration()
{
	if ( wwEnvStr( "WW_CELL_LIGHT_ADV_CALIB_RED" ) == QLatin1String( "ignore" ) )
		return 1.0f;	// the red: the row's value never reaches the lights
	WW_SWITCH_STATIC( QByteArray, p, wwEnv( "WW_CELL_LIGHT_ADV_CALIB" ).trimmed() );
	return p.isEmpty() ? calibRow() : clampCalib( p.toFloat() );
}

void wwCellLightCalibrationSet( float scale )
{
	if ( wwCellLightCalibrationPinned() )
		return;
	calibRow() = clampCalib( scale );
	QSettings().setValue( QStringLiteral( "WW/CellLightCalibration" ), double( calibRow() ) );
}

wwlp::Params wwLightPhysParams()
{
	wwlp::Params p;
	p.mode = wwLookOn( WwLook::LightAdvanced ) ? wwlp::Advanced : wwLookOn( WwLook::LightPhysFalloff ) ? wwlp::Physical : wwlp::Vanilla;
	p.advNumerator = wwlp::vanillaFlux() * wwCellLightCalibration();	// lane LIGHTPHYS3: 1 = kAdvCalibration, the same float
	p.curve = wwCellLightCurve();
	p.areaWorld = wwLookOn( WwLook::LightArea ) ? wwlp::kSourceWorld : 0.0f;
	// the reds, read at the use (each moves its own row's picture)
	if ( wwEnvStr( "WW_CELL_LIGHT_PHYS_RED" ) == QLatin1String( "noanchor" ) )
		p.red |= 1;
	if ( wwEnvStr( "WW_CELL_LIGHT_ADV_RED" ) == QLatin1String( "nowindow" ) )
		p.red |= 2;
	if ( wwEnvStr( "WW_CELL_LIGHT_AREA_RED" ) == QLatin1String( "nowiden" ) )
		p.red |= 4;
	if ( wwEnvStr( "WW_CELL_LIGHT_CURVE_RED" ) == QLatin1String( "nopivot" ) )
		p.red |= 8;
	return p;
}
