#pragma once
/* lane VKOBJ5 item 2: which Vulkan twin draws a GL program, and which of its variants (vkCellDraw, vkPfxDraw in cellbindless.cpp).
 * The twin comes from the program's name, the variant from its pixel stage's loader key (Program::wwDefs): the all-on route's
 * RT GI key (WW_CELL_RTDDGI + RTDDGI_CASCADES + RESTIR) is the rt twin, a key one of those short of it stays GL's. No GL or Qt
 * here: tests/vkframe/twinpick checks every twin's base and RT keys and the near misses offline. */
#include <string_view>

//! the route's switches the pick reads (the app fills them from its own state)
struct CellTwinRoute
{
	bool fogLeak = true;     //!< wwFogLeakShaderDefine()
	bool frameUbo = true;    //!< Program::wwShadowFrame
	bool giMarch = false;    //!< wwCellGiVis() == 1
	bool prt = false;        //!< wwCellPrtOn()
	bool rtddgi = false;     //!< rtddgiCellShaderDefine()
	bool restir = false;     //!< rtddgiRestirShaderDefine()
	bool restirGi = false;   //!< rtddgiRestirGiShaderDefine()
	bool airZone = true;     //!< wwAirZoneShaderDefine() (particles_cell only)
	bool colourMask = true;  //!< every channel of GL's colour write mask on (particles_cell only)
};

struct CellTwinPick
{
	int twin = 0;             //!< 2-8 and 10 objCellShade's twins, 9 particles_cell (objPfxShade), 0 none
	bool rt = false;          //!< the RT GI variant
	const char * no = nullptr;  //!< why GL draws it (twin 0 too: not a twin's program)
};

inline constexpr const char * kCellTwinBaseKey = "WW_CELL_FOGLEAK+WW_FRAME_UBO";
inline constexpr const char * kCellTwinBaseKeyAir = "WW_CELL_AIRZONE+WW_CELL_FOGLEAK+WW_FRAME_UBO";
inline constexpr const char * kCellTwinRtKey ="WW_CELL_FOGLEAK+WW_CELL_RESTIR+WW_CELL_RTDDGI+WW_CELL_RTDDGI_CASCADES+WW_FRAME_UBO";
inline constexpr const char * kCellTwinRtKeyAir = "WW_CELL_AIRZONE+WW_CELL_FOGLEAK+WW_CELL_RESTIR+WW_CELL_RTDDGI+WW_CELL_RTDDGI_CASCADES+WW_FRAME_UBO";

//! the twin id of GL's program `prog` (Program::name), 0 for a program no twin draws
inline int cellTwinOf( std::string_view prog )
{
	return prog == "fo4_cellcsm.prog" ? 3 : prog == "fo4_cell.prog" ? 2 : prog == "pbrm_default.prog" ? 4 : prog == "pbrm_cell.prog" ? 5
		: prog == "pbrm_cellcsm.prog" ? 6 : prog == "fo4_effectcell.prog" ? 7 : prog == "fo4_effectdecalcsm.prog" ? 8
		: prog == "particles_cell.prog" ? 9 : prog == "fo4_effectdecal.prog" ? 10 : 0;
}

inline CellTwinPick cellTwinPick( std::string_view prog, std::string_view defs, const CellTwinRoute & r )
{
	CellTwinPick p;
	p.twin = cellTwinOf( prog );
	if ( !p.twin ) {
		p.no = "not a twin's program";
		return p;
	}
	const int t = p.twin;
	if ( t == 9 ) {
		// particles_cell.frag@WW_CELL_AIRZONE+WW_CELL_FOGLEAK+WW_FRAME_UBO (vkframe_spirv.sh fxp), its RT GI variant the air key
		p.rt = defs == kCellTwinRtKeyAir;
		const bool ownKey = p.rt || defs == kCellTwinBaseKeyAir;
		p.no = !r.fogLeak ? "WW_CELL_FOGLEAK=0: GL's program is not the Vulkan twin"
			: !r.airZone ? "WW_CELL_AIRZONE off: GL's program is not the Vulkan twin"
			: !r.frameUbo ? "WW_FRAME_UBO off: GL's program is not the Vulkan twin"
			: r.giMarch ? "WW_CELL_GI_MARCH in GL's program: no Vulkan twin"
			: r.prt ? "WW_CELL_PRT in GL's program: no Vulkan twin"
			: !p.rt && r.rtddgi ? "WW_CELL_RTDDGI in GL's program without WW_CELL_RTDDGI_CASCADES + WW_CELL_RESTIR: no Vulkan twin"
			: !p.rt && r.restir ? "WW_CELL_RESTIR in GL's program without WW_CELL_RTDDGI: no Vulkan twin"
			: r.restirGi ? "WW_CELL_RESTIRGI in GL's program: no Vulkan twin"
			: !ownKey ? "GL's program is another variant (its pixel stage's loader defines): no Vulkan twin"
			: !r.colourMask ? "a colour write mask (the imagespace's measure pass)" : nullptr;
		return p;
	}
	/* every twin takes its own keys only (lane VKOBJ5 round 7: twins 2-6 too): the base key its module compiles or the RT GI
	 * key; any other loader define (a debug view, a red, a probe build) makes GL's program another one, which stays GL's.
	 * fo4_effectcell's keys carry the air zones (they ship on), fo4_effectdecalcsm's with or without them (the zones' read is
	 * not in its code; lane VKOBJ8: fo4_effectdecal's too, twin 10, tests/vkframe/twinpick proves the define inert in both); lane VKWATER3 G4: the RT GI key is the same on every twin (fo4_effectcell's with the air zones).
	 * CELLTWINPICK_RED_ANYBASE (tests/vkframe/twinpick only): the old rule, twins 2-6 take any key */
	p.rt = t == 7 ? defs == kCellTwinRtKeyAir : defs == kCellTwinRtKey || ( ( t == 8 || t == 10 ) && defs == kCellTwinRtKeyAir );
	const bool baseKey = t == 7 ? defs == kCellTwinBaseKeyAir
		: defs == kCellTwinBaseKey || ( ( t == 8 || t == 10 ) && defs == kCellTwinBaseKeyAir );
#ifdef CELLTWINPICK_RED_ANYBASE
	const bool ownKey = t < 7 || p.rt || baseKey;
#else
	const bool ownKey = p.rt || baseKey;
#endif
	p.no = !r.fogLeak ? "WW_CELL_FOGLEAK=0: GL's program is not the Vulkan twin"
		: !r.frameUbo ? "WW_FRAME_UBO off: GL's program is not the Vulkan twin"
		: r.giMarch ? "WW_CELL_GI_MARCH in GL's program: no Vulkan twin"
		: r.prt ? "WW_CELL_PRT in GL's program: no Vulkan twin"
		: !p.rt && r.rtddgi ? "WW_CELL_RTDDGI in GL's program without WW_CELL_RTDDGI_CASCADES + WW_CELL_RESTIR: no Vulkan twin"
		: !p.rt && r.restir ? "WW_CELL_RESTIR in GL's program without WW_CELL_RTDDGI: no Vulkan twin"
		: r.restirGi ? "WW_CELL_RESTIRGI in GL's program: no Vulkan twin"
		: !ownKey ? "GL's program is another variant (its pixel stage's loader defines): no Vulkan twin" : nullptr;
	return p;
}
