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
	/* the present device as vkpresent.cpp creates it: no textureCompressionBC, imageCubeArray, samplerAnisotropy or
	 * tessellationShader enabled, no VkBufferView route for GL texel buffers, no shaded mesh pipeline */
	c.sharedFmt = &waterSharedFmt;
	c.intFormats = waterSharedFmt( 0x8236 ) && waterSharedFmt( 0x823C ) && waterSharedFmt( 0x8232 );  // R32UI RG32UI R8UI
	return c;
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
