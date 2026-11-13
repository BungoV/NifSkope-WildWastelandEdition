/* Lane VKWATER1 (2026-11-13, plan row G5): the water's route on the present device (vkwater.h). Included at the end of
 * vkpresent.cpp (its vk() / vkFormatOf / nowMs). No draw moves yet: every refusal is named slot by slot, and GL draws. */
#include "features/vkframe/vkwater.h"

namespace
{
struct WaterRoute
{
	WwVkPresent::WaterStat st;
};
WaterRoute & waterS()
{
	static WaterRoute W;
	return W;
}
bool waterSharedFmt( unsigned f )
{
	bool depth = false;
	return vkFormatOf( GLenum( f ), depth ) != VK_FORMAT_UNDEFINED;
}
}  // namespace

namespace WwVkPresent
{
WaterCaps waterCaps()
{
	WaterCaps c;
	/* the present device as vkpresent.cpp creates it (lane VKWATER2): textureCompressionBC and imageCubeArray (VKFAR1's
	 * farTwin), VkBufferView texel buffers (farTexelBuffer), when offered; no shaded mesh pipeline yet */
	c.sharedFmt = &waterSharedFmt;  // lane VKWATER2: + R32UI / RG32UI / R8UI and D32F_S8 (waterDepth on a float frame)
	c.intFormats = waterSharedFmt( 0x8236 ) && waterSharedFmt( 0x823C ) && waterSharedFmt( 0x8232 );  // R32UI RG32UI R8UI
	if ( vk().ok ) {
		std::string why;
		c.bc = fcFeatures( true, false, why );
		c.cubeArray = fcFeatures( false, true, why );
		c.texelBuffer = farTexelBufferMax() > 0;
		c.aniso = vk().anisotropy;  // VKOBJ2's samplerAnisotropy
	}
	return c;
}

bool waterTwin( std::uintptr_t key, const WaterSlot & s, unsigned & tex, std::string & why )
{
	tex = 0;
	if ( s.kind == 5 ) {
		unsigned buf = 0;
		return farTexelBuffer( key, s.glFmt, std::size_t( std::max( s.w, 1 ) ) * 16u, buf, tex, why );
	}
	if ( s.kind == 4 )
		return farTwin( key, 3, s.w, s.h, s.d, s.glFmt, tex, why, s.levels );
	if ( s.kind < 0 || s.kind > 2 || ( s.kind == 1 && waterBcFormat( s.glFmt ) ) ) {
		why = s.name + ": no twin for this sampler kind / format";
		return false;
	}
	if ( waterBcFormat( s.glFmt ) )
		return farTwin( key, s.kind, s.w, s.h, s.kind == 2 ? s.d : 1, s.glFmt, tex, why, s.levels );
	return sharedTarget( key, s.kind, s.w, s.h, s.d, s.glFmt, tex, why, s.levels );
}

void waterTwinDrop( std::uintptr_t key, const WaterSlot & s )
{
	if ( s.kind == 5 )
		farTexelBufferDrop( key );
	else if ( s.kind == 4 || waterBcFormat( s.glFmt ) )
		farTwinDrop( key );
	else
		sharedTargetDrop( key );
}

void waterOnGl( const char * why )
{
	WaterStat & s = waterS().st;
	s.gl++;
	s.glWhy = why ? why : "";
}

void waterCensusTook( double ms )
{
	WaterStat & s = waterS().st;
	s.census++;
	s.censusMs += ms;
}

WaterStat takeWaterStat()
{
	WaterStat s = waterS().st;
	waterS().st = WaterStat();
	return s;
}
}  // namespace WwVkPresent

#ifndef WW_VKPRESENT_NO_QT
namespace WwVkPresent
{
void waterSay( bool wanted, const char * glWhy )
{
	static QString said;
	static double at = 0.0;
	const QString head = QStringLiteral( "vk water: " );
	if ( !wanted ) {
		const QString line = head + QStringLiteral( "on GL (%1)" ).arg( QString::fromUtf8( glWhy ? glWhy : "" ) );
		if ( line != said ) {
			said = line;
			qInfo().noquote() << line;
		}
		return;
	}
	const double now = nowMs();
	if ( now - at < 5000.0 && said.startsWith( head + QStringLiteral( "wanted" ) ) )
		return;
	at = now;
	const WaterStat st = takeWaterStat();
	QString line = head + QStringLiteral( "wanted on Vulkan, 0 draws run; on GL %1 draws" ).arg( st.gl );
	if ( !st.glWhy.empty() )
		line += QStringLiteral( " (blocked: %1)" ).arg( QString::fromStdString( st.glWhy ) );
	if ( st.census > 0 )
		line += QStringLiteral( "; %1 censuses, %2 ms each" ).arg( st.census ).arg( st.censusMs / st.census, 0, 'f', 3 );
	said = line;
	qInfo().noquote() << line;
}
}  // namespace WwVkPresent
#endif
