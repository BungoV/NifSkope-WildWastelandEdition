#include "celllooks.h"

#include "wwswitch.h"

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
	case WwLook::ExposureWide: {
		WW_SWITCH_STATIC( QByteArray, p, wwEnv( "WW_CELL_EXPOSURE_WIDE" ).trimmed() );
		return p;
	}
	case WwLook::ProbeShare: {
		WW_SWITCH_STATIC( QByteArray, p, wwEnv( "WW_CELL_GI_SHARE" ).trimmed() );
	case WwLook::DecalFade: {
		WW_SWITCH_STATIC( QByteArray, p, wwEnv( "WW_CELL_DECAL_FADE" ).trimmed() );
		return p;
	}
	case WwLook::DecalParallax: {
		WW_SWITCH_STATIC( QByteArray, p, wwEnv( "WW_CELL_DECAL_POM" ).trimmed() );
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
	case WwLook::PlacementEmittance: {
		WW_SWITCH_STATIC( QByteArray, p, wwEnv( "WW_CELL_XEMI" ).trimmed() );
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
	return l == WwLook::ActorsOutdoors || l == WwLook::WeaponParts || l == WwLook::AnimObjects || l == WwLook::BtoSway
		|| l == WwLook::PlacementEmittance;
}

bool wwLookHasRow( WwLook l )
{
	return l != WwLook::AirClusters;
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
