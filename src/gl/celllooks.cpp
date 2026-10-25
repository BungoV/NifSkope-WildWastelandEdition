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
	{ "Placement emittance", "WW/CellPlacementEmittance" },
	{ "Region emittance", "WW/CellRegionEmittance" },
	{ "Emittance on particles", "WW/CellParticleEmittance" },
	{ "Emittance in bounce light", "WW/CellBounceEmittance" },
	{ "Lens flares", "WW/CellLensFlares" },
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
	case WwLook::PlacementEmittance: return "WW_CELL_XEMI";
	case WwLook::RegionEmittance: return "WW_CELL_XEMI_REGN";
	case WwLook::ParticleEmittance: return "WW_CELL_XEMI_PART";
	case WwLook::BounceEmittance: return "WW_CELL_XEMI_GI";
	case WwLook::LensFlares: return "WW_CELL_LENSFLARE";
	default: return "";
	}
}

bool wwLookPinned( WwLook l )
{
	return !pinOf( l ).isEmpty();
}

bool wwLookReopens( WwLook l )
{
	// lane LIGHTS4: the placement, region and particle emittance rows are read at draw time (live)
	return l == WwLook::ActorsOutdoors || l == WwLook::WeaponParts || l == WwLook::BounceEmittance;
}

bool wwLookOn( WwLook l )
{
	const QByteArray p = pinOf( l );
	return p.isEmpty() ? rowVal( l ) != 0 : p != "0";
}

void wwLookSet( WwLook l, bool on )
{
	if ( wwLookPinned( l ) )
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
