// HLSL source of features/lights/cell_lights.glsl (include-only GLSL), lane HLSLSRC2 (notes/hlslsrc2/tools/glsl2hlsl.py): the GLSL's comments,
// names and order kept. Its loose uniforms sit at the top under WW_LOOSE_UNIFORMS: the entry that includes this
// file includes it once more inside its LooseUniforms cbuffer with that defined (HLSL has one cbuffer for them).
#ifdef WW_HLSL_SRC
#if defined(WW_LOOSE_TYPES)	// the types its loose uniforms use

#elif defined(WW_LOOSE_UNIFORMS)
	bool cellOn;
	int cellLightCount;
	float4 cellRow[3];			// world = (dot(row.xyz, posView) + row.w), the view's inverse
	bool cellHasDalc;
	int cellGiAmb;				// WW_CELL_GI_AMB, the interior ambient beside the GI: 0 keep, 1 replace (default), 2 off, 3 max, 4 asgi (debug)
	bool cellGiFill;			// a GI gap takes the weighted mean round it (WW_CELL_GI_FILL=0: off)
	float4 cellDalc[3];			// per channel: (p - n) / 2 per axis, mean of the six (byte / 255)
	bool cellHasDir;
	bool cellExtDalc;			// an exterior's ambient is the weather's DALC, in cellDalc (the same law as indoors)
	float cellSunScale;			// the imagespace Sunlight Scale outdoors (1 indoors; cellDirColor has it in)
	float3 cellDirColor;			// linear
	float3 cellDirTo;				// world, TO the light
	bool cellInterior;
	float3 cellCenter;
	int cellProbe;
	// probe 6 is multiplied by this (WW_CELL_LIT_PROBE_GAIN, debug only; 1 otherwise)
	float cellProbeGain;
	// the Ambient Only volumes (src/gl/celllights.h): world center + 1.22077 x radius, and the
	// per-channel scale of the ambient's affine sum; the first in plugin order that holds a point wins
	int cellAmboCount;
	float4 cellAmbo[16];
	float4 cellAmboK[16];
	float4 cellAmboBox[48];		// where cellAmboK[i].w is 1 the volume is a box, rows i * 3 .. i * 3 + 2
	float4 cellLightPhys;			// x falloff 0 game / 1 physical / 2 advanced, y anchor,
	float4 cellLightPhysB;		// z source fraction, w advanced numerator; B: x advanced min metres, y area size (0 off), z red bits
	int cellRed;				// 1 linear: the radial curve without its 2.2; 8 lambert, 16 normalised, 32 norim,
	// 64 rimflags (the lights' rim / roughness flags ignored); 512 cubeold
	// the bake relit by these lights (src/features/gi/probegi.h), six axis slabs of dims.z each, x fastest;
	// rgb = irradiance x valid, a = valid (so a filtered sample divides by its own valid)
	bool cellGiOn;
	float3 cellGiOrigin;
	float cellGiVoxel;
	float3 cellGiDims;
	float cellGiPad;		// lane GIEDGE1: the grid's x/y edge voxels past the probes' reach (probegi kPad); 0 = the clampedge red
	// rooms (src/features/gi/proberooms.h): the grid holds 12 slabs (slot 0's six, then slot 1's); cellGiSlots
	// per voxel and cellRooms per fine cell hold two rooms packed as (a + 1) x 4096 + (b + 1), -1 none
	bool cellGiRooms;
	float3 cellRoomsOrigin;
	float cellRoomsCell;
	float3 cellRoomsDims;
	// a weather-lit exterior's grid holds the sky the probes see (src/features/gi/probesky.h), so it stands in
	// for the weather's unshadowed ambient by the grid's valid share (0 where the grid does not reach)
	bool cellGiSky;
	// lane EMISSIVEGRID1 (src/features/gi/probegifine.h): the fine boxes round bright glow, their atlas in cellGi's layers after the grid's
	bool cellGiFineOn;
	int cellGiFineN;
	int cellGiFineRed;		// 1 noseen, 2 nosub (WW_CELL_EMISSIVEGRID_RED)
	float4 cellGiFineBox[8];		// origin, voxel
	float4 cellGiFineDim[8];		// nx, ny, nz, first layer
	// the PRTP band's Pass (0 Combined, 1 GI, 2 Sky visibility, 3 Surfel color, 4 Surfel light).
	// In the Sky visibility pass the CPU binds the sky grid on cellGi (same layout, rgb = the probes' sky share).
	int cellPass;
	int cellPassRed;			// the gate's reds: 1 direct (the lights leak into GI), 2 nonormal (sampled facing up); 8192 WW_CELL_GIWALL_RED=off
	// the cell's imagespace, the game's own HDR -> display chain (src/gl/celllights.h)
	bool cellIsOn;
	bool cellIsLinear;			// write linear light, the frame is tone-mapped once (cell_hdr.frag)
	float cellIsExposure;		// clamp( middle gray / (adapted + 0.001), min, max ), the CPU's
	float cellIsE;				// HNAM Tonemap E: the curve's toe numerator
	float cellIsAdapted;		// the frame's mean luminance, the contrast pivot
	float3 cellIsCine;			// CNAM saturation, brightness, contrast
	float4 cellIsTint;			// TNAM amount, r, g, b
	// lane FIX1101A (atmo, LATE ITEM 15c cause 10): a camera white balance on the linear light, gain - 1 per channel; 0
	// (unset, WW_CELL_WB off: the shipped default) multiplies by exactly 1. WW_CELL_WB=day45 balances to the 45-degree sun's
	// daylight through the frame's air (atmosphere.cpp wwAtmoWbGain); a test key for bungo's three-way sheet, not a default.
	float3 cellIsWbD;
	bool cellIsLutOn;
	int cellIsRed;				// 1 nolut, 2 noexp, 4 nograde, 8 nobloom (the CPU drops cellIsBloomOn)
	bool cellIsVanCurve;		// lane FIX1101A T: WW_TONEMAP_RED=vanilla, the game's filmic curve (else Physical)
	// the measure's blurred bright pass, a quarter of the view; rect = viewport origin, 1 / size
	bool cellIsBloomOn;
	float4 cellIsBloomRect;
	// one depth cube per shadow-casting light (distance to the caster / radius); texel = 2 / face
	bool cellShadowOn;
	float cellShadowTexel;
	bool cellShadowFadeOn;		// lane GAPLOOK1 (LIGHTSYS-3): the budget's per-slot strength (celllights.cpp shFade)
	float cellShadowFade[16];
#if defined(WW_CELL_RESTIR)
	// lane RESTIRDI1 (src/features/gi/rtddgicell.cpp, src/features/gi/rtddgi/restir.comp): the RT quality preset Medium+. ReSTIR DI's ratio a pixel
	// (Heitz 2018: the shadowed / unshadowed estimates of the shadow-casting lights) and the linear depth it was traced for
	bool cellRestirOn;
	float4 cellRestirSize;		// W, H, z = 1: red WW_CELL_RESTIR_MISS_RED (a fragment off the ratio's depth is unshadowed)
#endif
	// distant light from the surfels (src/features/far-lod/farlight.h), set by celllights.cpp
	bool cellFarOn;
	int cellFarBits;
	int cellFarProbe;			// the longest walk any record's key needs
	float cellFarCell;
	float2 cellFarBand;			// D0, D1: w = smoothstep(D0, D1, the camera distance)
	int cellFarRed;				// WW_CELL_FAR_RED: 1 nosurfel, 2 noblend, 4 flipside
	bool cellFarPlacedOn;
	float3 cellFarPlacedOrigin;
	float cellFarPlacedVoxel;
	float3 cellFarPlacedDims;
#if (defined( WW_CELL_HDR ))
	bool cellIsFoBloomOn;
	float4 cellIsFoBloomParams;	// radius (pixels), bloom weight, dispersion, 0
	float2 cellIsFoBloomFrame;	// the frame's size: the combine's uv
	// lane PHYSUNITS1: the energy-conserving glare's k (0: the additive FO4CS bloom); cellFoBloomHere is then blur x k
	float cellIsFoBloomEc;
	// LATE 7 (PHYSUNITS1 note): the most the fog's gain may raise that k (x 2); red WW_CELL_FOGBLOOM_RED=glare: 1e9
	float cellIsFoBloomFogCap;
	// lane FOGBLOOM1 (src/features/post/cellfogbloom.h): the bloom-intensity target, (gain - 1) / 4 per sample; cell_hdr.frag sets
	// cellBloomGain from it, which scales both blooms (1.0 exactly in clear air: the same bits)
	bool cellFogBloomOn;
#endif
	/* lane GAPLOOK1 (C8): contact shadows for a lamp WITHOUT a shadow map, FO4CS's port (res/Lighting/
	 * fo4cs_contact_shadows.hlsli, its defaults): a view-space march toward the light over the shared opaque pass's depth,
	 * reach min(0.15 x radius, 12) but at least 4 units, a hit inside [0.6 + 0.008 z, max(20, 0.1 z)] units behind the
	 * depth darkens by (1 - travelled / reach) x 0.5. Steps: 48, fewer when the march covers fewer pixels (a pixel read
	 * twice adds nothing); the phase jittered per pixel. A mapped light keeps its map: dark places never darken twice. */
	bool cellCtOn;
	float4 cellCtProj;		// P00, P11, P20, P21 of the projection
	float4 cellCtRect;		// the target's origin in this viewport, the g-buffer's size
	int cellCtRed;			// 1: march away from the light
	/* lane PERF1 (the GI volumes, the Division deck's D4): the leak guard is the volume a pixel stands in, not a march.
	 * The outdoors is one volume (room 0); every interior room is a volume of its own, drawn as primitive shapes into a
	 * screen mask (src/gl/celllights.cpp giVolPass, cell_givol.frag). A pixel reads only the grid voxels (slots) of its own
	 * volume: the 8 voxels round P + N x half a voxel, trilinear, renormalized. Where a threshold joins two volumes the
	 * voxels near it carry its weight t and the other side's room (baked, src/features/gi/probegi.cpp probeGiRoomsInto), and the pixel
	 * blends the other side's read in by t / 2: both ways, so light passes in and out through thresholds only.
	 *   cellGiVolume: one copy of the read, loops kept as loops (the bound is a uniform), so a call site costs little.
	 *   Its volume: the screen mask (the full-screen GI pass, cell_gihalf.frag), else the fine room grid at P + N x 0.75
	 *   cell, else 1.75 cell (blended and effect draws, the probe passes); neither: every voxel's first slot.
	 * Red WW_CELL_GI_VIS=march compiles WW_CELL_GI_MARCH: today's chain (cellGiSampleFull), every call, as before PERF1.
	 * Probe 95 paths, in lane GIBLACK2's words (tests/features/gi/cell_giblack_check.py): 1 the volume's own voxels (a room's
	 * blend; also with a threshold's), 2 the nearest own-volume voxel (a room's fill), 4 no volume known or none of its
	 * voxels in reach (the trilinear); outdoors, a surface the room grid cannot place (lane GIBLACK2's stranded): 6 and 7
	 * the outdoors' volume, 5 none in reach. A probe picture reads the mask at its own pixel (the screen pass's read). */
	int cellGiLoop8;		// 8: the corner loop's bound, a uniform so the loop stays one body; MARCHTOOLS: declared for both modes (EMISSIVEGRID1 reads it in the march mode too)
#if !(defined(WW_CELL_PRT)) && ((defined( WW_CELL_RESTIRGI ) && defined( WW_CELL_RTDDGI )))
	/* RTEMIS1 part 2 (src/features/gi/rtddgicell.cpp rtddgiRestirGiPass, src/features/gi/rtddgi/restirgi.comp): the RT quality
	 * preset High+. ReSTIR GI's irradiance a pixel (Ouyang 2021: one bounce ray a pixel on the volume's BVH, shaded by the
	 * volume, reused in time and space, then a running mean and a depth/normal filter) in place of the volume's E where the
	 * fragment is the surface the opaque pass saw; the volume's fade and sky visibility stay. Elsewhere (blended, a probe
	 * picture, the Sky visibility pass, a depth miss) the volume's read, as below High. */
	bool cellRestirGiOn;
	float4 cellRestirGiSize;		// W, H
#endif
	// the baked AO decals of the big movable statics (src/gl/cellaodecalgl.h), one texel per pixel,
	// multiplied into the probe term only (the grid was rebuilt without those copies' blocked sky)
	bool cellAoDecalOn;
	float4 cellAoDecalRect;		// the viewport's x, y
	float cellSkyVis;		// the Sky visibility row, 0..1 (src/gl/celllights.h)
	/* lane SPECOCC1: specular occlusion from the baked sky visibility (FO4CS ambient_ibl_pass.hlsl, M2.3 horizon specular
	 * occlusion), on top of the material AO. Outdoors the cube reflection is the sky's, so it keeps the probes' open-sky
	 * share in the direction it reflects: the sky grid read on the lobe's dominant direction (R for a mirror, bending to N
	 * as it roughens), n^2 axis weights, the Sky visibility row's strength; 1 where the grid has no evidence. Red hemi:
	 * FO4CS's own read, the normal's share faded out by smoothness (a mirror sees one direction, not the hemisphere). */
	int cellSpecOcc;		// 0 off (the material AO alone), 1 on, 2 the hemi red (src/gl/celllights.h)
	float cellSpecSkyBase;		// the sky grid's slot 0 rides cellGi after its own slabs (6, or 12 with rooms): no new sampler
	/* lane LIGHTS5 (GOBO1): a spot's pattern (LIGH NAM0) at D = P - the light, axis = its direction. Texel 5 = (local y,
	 * 1 + the pattern's header texel; 0 none), texel 6 = (local z, tan(FOV / 2)); header (w, h), mean, then rows. */
	bool cellGoboOn;
	int cellGoboRot;	// WW_CELL_GOBO_ROT: bit 0 flips u, bit 1 flips v, bit 2 swaps them
	bool cellGoboFlat;	// red flat: each pattern's mean
	bool cellGoboGam;	// GOBO3 red WW_CELL_GOBO_GAMMA=2.2: the stored value ^2.2 (the game reads it linear)
	/* lane GOBO2: an omni/hemisphere/box light's pattern. Its textures are two squares stacked (two half-sphere views).
	 * GOBO3, from Todd's treat: a dual paraboloid about the ref's local x (the axis a spot shines along); u from local z,
	 * v from local y; the top half is +x, the bottom half (more than 1 unit behind) has u mirrored. Pin WW_CELL_GOBO_OMNI
	 * (cellGoboMap), reds: 1 no mirror, 2 u/v axes swapped, 4 world axes; 8 + bits = GOBO2's guess (pole local z,
	 * top +z; then 1 swaps the halves, 2 takes local x as the pole, 4 world axes). */
	int cellGoboTbl;	// texel of the per-light table: 1 + the light's record (local x + 1 + header, y, z); 0 none
	int cellGoboMap;
#if !defined(WW_CELL_FX)
	/* per draw (src/gl/renderer.cpp): the material's specular scale (0 with its specular switch off), its smoothness,
	 * 3 when its OWN env map is bound and sampled as sRGB (1: own but untagged, decoded here; 2: a .pbrm shape), a tag */
	float4 cellCubeMat;
#endif
	// lane SSGI1: the screen-space bounce (src/features/gi/cellssgi.h, cell_ssgi.frag): the probe GI keeps the share of the
	// hemisphere the march left open, the screen's bounce is added (FO4CS's probe-fill law; red "stack" adds on top)
	bool cellSsgiOn;
	float4 cellSsgiRect;		// the viewport: x, y, 1 / w, 1 / h
	float2 cellSsgiK;			// FO4CS's fAoAmount, fIndirectAmount
	int cellSsgiRed;
#else
#ifndef WW_GLSL_SIZE_Texture3D_float4_
#define WW_GLSL_SIZE_Texture3D_float4_
int3 textureSize(Texture3D<float4> t, int lod) { uint w, h, d, l; t.GetDimensions(uint(lod), w, h, d, l); return int3(w, h, d); }
#endif
#ifndef WW_GLSL_MOD
#define WW_GLSL_MOD	// GLSL mod: x - y floor(x / y) (HLSL fmod truncates)
float mod(float x, float y) { return x - y * floor(x / y); }
float2 mod(float2 x, float2 y) { return x - y * floor(x / y); }
float3 mod(float3 x, float3 y) { return x - y * floor(x / y); }
float4 mod(float4 x, float4 y) { return x - y * floor(x / y); }
float2 mod(float2 x, float y) { return x - y * floor(x / y); }
float3 mod(float3 x, float y) { return x - y * floor(x / y); }
float4 mod(float4 x, float y) { return x - y * floor(x / y); }
#endif
#ifndef WW_GLSL_SIZE_TextureCube_float4_
#define WW_GLSL_SIZE_TextureCube_float4_
int2 textureSize(TextureCube<float4> t, int lod) { uint w, h, l; t.GetDimensions(uint(lod), w, h, l); return int2(w, h); }
#endif
// The cell's placed lights, interior ambient and directional (docs/PRTP2_LIGHT_MODEL.md).
// Included only when WW_CELLLIGHTS is defined; src/gl/celllights.cpp sets every uniform.


Buffer<float4> cellLights : register(t0, space0);	// 8 texels a light: pos+radius, color+cosOuter (-2 omni, -3 hemisphere, -4 box), dir+cone,
									// bias scale exponent flags (1 noSpec, 2 noRim, 4 ignoreRoughness), shadow slot (-1 none)
									// kind near-clip xlig-bias, then the box's three rows
#define CELL_TPL 8					// texels a light (celllights.cpp kTexelsPerLight)

#include "cell_cluster.hlsli"

// set by the caller before cellLit (exterior sun only): the cascades' light, the viewport ambient, the sun's specular (linear)
static float cellDirVis = 1.0;
static float cellBackK = 0.0;	// MODULES1 R6-1: the sun through a back-lit leaf, per unit sun x albedo (fo4_default.frag sets it)
static float cellAirDif = 0.0;	// MODULES1 G3: the sun the air scattered down onto a level surface, per unit sun (wwVolAirSunTD)
static float3 cellExtAmb = (float3)( 0.0 );
static float3 cellExtSpec = (float3)( 0.0 );
static float3 cellReflAdd = (float3)( 0.0 );	// lane REFLPROBE1: the probes' reflection, light already (cell_reflprobe.glsl)
// lane DEBUGVIEW1: the live debug views (src/features/overlays/debugview.h), compiled only while one is on
#ifdef WW_DEBUG_VIEWS
#include "../overlays/debug_view.hlsli"
#endif
static float3 cellSpecTint = (float3)( 1.0 );	// lane GAPFIX1: the colour cellLit puts on the lights' specular (probe 31)
Texture3D<float4> cellGi : register(t1, space0); SamplerState _cellGi_sampler : register(s1, space0);
Texture3D<float4> cellGiSlots : register(t2, space0); SamplerState _cellGiSlots_sampler : register(s2, space0);
Texture3D<float4> cellRooms : register(t3, space0); SamplerState _cellRooms_sampler : register(s3, space0);
// the old baked probe GI (the grid above, the PRT) compiles only under WW_CELL_PRT, the loader's define for
// the red WW_CELL_PRT_RED=1.
#ifdef WW_CELL_PRT
#define CELL_GI_UP cellGiOn
#else
#define cellGiOn false
#ifdef WW_CELL_GI_MARCH
#undef WW_CELL_GI_MARCH
#endif
#ifdef WW_CELL_RTDDGI
#define CELL_GI_UP cellDdgiOn
#else
#define CELL_GI_UP false
#endif
#endif


Texture3D<float4> cellIsLut : register(t4, space0); SamplerState _cellIsLut_sampler : register(s4, space0);
Texture2D<float4> cellIsBloom : register(t5, space0); SamplerState _cellIsBloom_sampler : register(s5, space0);
TextureCubeArray<float4> cellShadow : register(t6, space0); SamplerComparisonState _cellShadow_sampler : register(s6, space0);
#ifdef WW_CELL_RESTIR

Buffer<float4> cellRestir : register(t7, space0);	// RG32F, bottom row first: ratio, linear depth
#endif

Buffer<float4> cellFarSlots : register(t8, space0);	// a slot: x, y, z x 8 + side, record + 1 (0 empty); exact integers
Buffer<float4> cellFarRecs : register(t9, space0);	// 2 texels a record: pos, E (the shader's diff units)
Texture3D<float4> cellFarPlaced : register(t10, space0); SamplerState _cellFarPlaced_sampler : register(s10, space0);	// the GI grid relit by the placed lights alone: slot 0's six slabs
// the FO4CS bloom (src/features/post/cellbloom.h), the tone map's final combine: cell_hdr.frag sets cellFoBloomHere
// to tent(the chain's top) x the bloom weight before it calls cellImageSpace
#if defined( WW_CELL_HDR )

Texture2D<float4> cellIsFoBloom : register(t11, space0); SamplerState _cellIsFoBloom_sampler : register(s11, space0);
static float3 cellFoBloomHere = (float3)( 0.0 );
Texture2DMS<float4> cellFogBloom : register(t12, space0);
static float cellBloomGain = 1.0;
#endif

#include "../exposure/cell_tonemap.hlsli"

// sqrt-of-linear in (this program's convention), display out. Shaders011.fxp, the tonemap PS and the LUT PS.
// lane FIX1101A T (LATE ITEM 26): the curve is Fusion Shaders' Physical (cell_tonemap.glsl) on the exposed light; the
// bloom, the exposure, the grade, the 1 / 2.2 and the LUT are the game's, in the same order. Red: cellIsVanCurve.
float3 cellImageSpace( float3 sqrtColor )
{
	float3 x = max( sqrtColor, (float3)( 0.0 ) );
	x = x * x;
// an effect blends over a surface that already took the bloom; the HDR tone map takes it once
#if !defined( WW_CELL_FX ) || defined( WW_CELL_HDR )
	if ( cellIsBloomOn )	// the tonemap PS adds the bloom target before its exposure multiply
#if defined( WW_CELL_HDR )
		x += cellBloomGain * cellIsBloom.Sample( _cellIsBloom_sampler, ( gl_FragCoord.xy - cellIsBloomRect.xy ) * cellIsBloomRect.zw ).rgb;
#else
		x += cellIsBloom.Sample( _cellIsBloom_sampler, ( gl_FragCoord.xy - cellIsBloomRect.xy ) * cellIsBloomRect.zw ).rgb;
#endif
#endif
	x = x * ( ( cellIsRed & 2 ) != 0 ? 1.0 : cellIsExposure ) * 2.0;
	x *= (float3)( 1.0 ) + cellIsWbD;	// lane FIX1101A: the white balance (x 1 exactly by default)
#if defined( WW_CELL_HDR )
	if ( cellIsFoBloomOn && cellIsFoBloomEc > 0.0 ) {
		// lane PHYSUNITS1: the frame lerped toward its blur (moves light, adds none); the fog's gain raises k, at most 1
		float kg = min( cellIsFoBloomEc * ( cellIsFoBloomFogCap > 0.0 ? min( cellBloomGain, cellIsFoBloomFogCap ) : cellBloomGain ), 1.0 );
		x = x * ( 1.0 - kg ) + 2.0 * ( kg / cellIsFoBloomEc ) * cellFoBloomHere;
	} else if ( cellIsFoBloomOn )	// FO4CS adds its bloom to the exposed light, before the curve (x 2: the curve's input)
		x += 2.0 * cellBloomGain * cellFoBloomHere;
#endif
	float3 c;
	if ( cellIsVanCurve ) {
		float E = cellIsE;
		c = ( x * ( 0.15 * x + 0.05 ) + 0.2 * E ) / ( x * ( 0.15 * x + 0.5 ) + 0.06 ) - E / 0.3;
		c /= ( 0.2 * E + 19.376 ) * 0.040856 - E / 0.3;		// the curve at W 11.2
	} else
		c = wwTonemapPhysical( 0.5 * x );	// x is 2 x the exposed light: the filmic curve's own input bias
	if ( ( cellIsRed & 4 ) == 0 ) {
		float luma = dot( c, float3( 0.2125, 0.7154, 0.0721 ) );
		c = lerp( (float3)( luma ), c, cellIsCine.x );
		c = lerp( c, luma * cellIsTint.yzw, cellIsTint.x );
		c = cellIsCine.z * ( cellIsCine.y * c - cellIsAdapted ) + cellIsAdapted;
	}
	c = pow( max( c, (float3)( 0.0 ) ), (float3)( 1.0 / 2.2 ) );
	if ( cellIsLutOn && ( cellIsRed & 1 ) == 0 )
		c = cellIsLut.Sample( _cellIsLut_sampler, c * 0.9375 + 0.03125 ).rgb;
	return c;
}

float3 cellWorldPos( float3 posView )
{
	float4 p = float4( posView, 1.0 );
	return float3( dot( cellRow[0], p ), dot( cellRow[1], p ), dot( cellRow[2], p ) );
}

float3 cellWorldDir( float3 dirView )
{
	return normalize( float3( dot( cellRow[0].xyz, dirView ), dot( cellRow[1].xyz, dirView ), dot( cellRow[2].xyz, dirView ) ) );
}

float cellAttVan( float x, float3 c )
{
	float xe = c.z > 0.0 ? pow( x, c.z ) : 1.0;
	return pow( 1.0 - clamp( c.y * xe + c.x, 0.0, 1.0 ), 2.2 );
}

float cellPhysRaw( float x, float s )
{
	float x2 = x * x;
	float w = clamp( 1.0 - x2 * x2, 0.0, 1.0 );
	float den = x2 + s * s;
	return den > 0.0 ? w * w / den : 0.0;
}

// lane LIGHTPHYS1: the physical or advanced falloff for the game's value `van` at x = d / r (lightphys.h falloff)
float cellPhysFalloff( float van, float x, float r, float3 c )
{
	float a = clamp( cellLightPhys.y, 0.0, 1.0 );
	if ( or( or( !( c.z > 0.0 ), !( clamp( c.y + c.x, 0.0, 1.0 ) > clamp( c.x, 0.0, 1.0 ) ) ), cellAttVan( 1.0, c ) >= 1.0 / 255.0 ) )
		return van;	// an authored curve that is no falloff keeps the game's
	float vA = cellAttVan( a, c );
	float b = 1.0 - a * a;
	if ( vA > ( b > 0.0 ? pow( b, 1.1 ) : 0.0 ) )
		return van;
	int red = int( cellLightPhysB.z + 0.5 );
	if ( cellLightPhys.x > 1.5 ) {
		float x0 = r > 0.0 ? clamp( max( max( cellLightPhysB.x, 0.001 ), max( 0.1 * r / 69.99, 0.2 ) ) * 69.99 / r, 1e-4, 0.5 ) : 0.5;
		float den = 0.5 * ( 1.0 + 0.8 ) - ( 2.0 / 3.0 ) * x0;
		float t = clamp( ( x - 0.8 ) / ( 1.0 - 0.8 ), 0.0, 1.0 );
		float w = ( red & 2 ) != 0 ? 1.0 : 1.0 - t * t * ( 3.0 - 2.0 * t );
		float m = max( x0, x );
		return den > 0.0 ? ( cellLightPhys.w / den ) * w / ( m * m ) : 0.0;
	}
	float s = cellLightPhys.z;
	float raw = cellPhysRaw( a, s );
	float k = ( red & 1 ) != 0 ? 1.0 : raw > 0.0 ? vA / raw : 0.0;
	return k > 0.0 ? k * cellPhysRaw( x, s ) : van;
}

// PRTP2 section 1: x = saturate(d / r); atten = pow(1 - saturate(scale * x^exponent + bias), 2.2)
float cellRadial( float d, float r, float3 bse )
{
	float x = clamp( d / max( r, 0.001 ), 0.0, 1.0 );
	float xe = bse.z > 0.0 ? pow( x, bse.z ) : 1.0;
	float k = 1.0 - clamp( bse.y * xe + bse.x, 0.0, 1.0 );
	if ( ( cellRed & 1 ) != 0 )
		return k;
	return cellLightPhys.x > 0.5 ? cellPhysFalloff( pow( k, 2.2 ), x, r, bse ) : pow( k, 2.2 );
}

/* light i's shadow factor at P (normal N, facing it): 1 lit, 0 shadowed. The point lifted
 * 1.5 texels along N (the slope bias), then its distance - 1 unit against 3x3 taps a texel apart (each
 * a hardware 2x2 compare), / 9, as the game's 9 taps. A hemisphere lights nothing behind its plane. */
float cellShadowF( int i, float3 P, float3 N )
{
	float4 t4 = cellLights.Load( i * CELL_TPL + 4 );
	if ( or( !cellShadowOn, t4.y < 0.5 ) )
		return 1.0;
	float4 t0 = cellLights.Load( i * CELL_TPL );
	if ( t4.y > 1.5 && t4.y < 2.5 && dot( P - t0.xyz, cellLights.Load( i * CELL_TPL + 2 ).xyz ) < 0.0 )
		return 0.0;	// the mask's paraboloid: behind the hemisphere's plane is unlit (in a slot or not)
#ifdef WW_CELL_RESTIR
	/* lane RESTIRDI1: every shadow-casting light takes the pixel's ray-traced ratio in place of its cube (the 16 slots are
	 * not drawn). A fragment that is not the surface the opaque pass saw (blended, an effect, water) is unshadowed, as a
	 * light past the slot budget was */
	if ( cellRestirOn ) {
#ifdef WW_AIR_LEAK
		if ( ( volLeakRed & 4 ) == 0 )
			return 1.0;	// lane FOGLEAK1 fix 3: a froxel is no pixel; the air's lights go through the volume (cell_volair.frag airLeakLightK)
#endif
		int2 q = int2( gl_FragCoord.xy );
		int W = int( cellRestirSize.x + 0.5 ), H = int( cellRestirSize.y + 0.5 );
		if ( or( or( or( q.x < 0, q.y < 0 ), q.x >= W ), q.y >= H ) )
			return 1.0;
		float2 rr = cellRestir.Load( q.y * W + q.x ).rg;
		// the fragment's linear depth: world -> view through cellRow (rows of the view's inverse, / scale)
		float3 w = P - float3( cellRow[0].w, cellRow[1].w, cellRow[2].w );
		float s2 = 1.0 / max( dot( cellRow[0].xyz, cellRow[0].xyz ), 1e-12 );
		float D = -s2 * ( cellRow[0].z * w.x + cellRow[1].z * w.y + cellRow[2].z * w.z );
		if ( abs( D - rr.y ) <= 0.02 * rr.y + 2.0 )
			return rr.x;
#ifdef CELL_RESTIR_MISS_PIXEL
		/* MODULES1 R7 item 2 (bungo: one-frame flashes, stray lights): a SURFACE fragment (fo4_default, pbrm_default) off
		 * the traced depth took 1.0 = EVERY shadow-casting lamp unshadowed. A shape the opaque pre-pass missed for one
		 * frame then lit up with the light of a lamp behind a wall, sharp at the shape's outline, for that frame. Now it
		 * takes the pixel's ratio (the surface behind it, traced this frame). No surface behind (the g-buffer's 1e6):
		 * unshadowed, as before. Red WW_CELL_RESTIR_MISS_RED=1 (cellRestirSize.z). */
		return ( or( cellRestirSize.z > 0.5, rr.y > 9.0e5 ) ) ? 1.0 : rr.x;
#else
		return 1.0;	// an effect card, a particle, water, a froxel: free in the air, not the traced surface's shadow
#endif
	}
#ifndef WW_CELL_RESTIR_CUBES_RED
	/* lane PRESETPERF1 (PRESETPERM1): under WW_CELL_RESTIR no cube is drawn and every slot is let go (celllights.cpp, the
	 * cube pass), so t4.x < 0 and the taps below never run: the Medium+ programs need not carry them. Red (the loader's
	 * WW_CELL_RESTIR_CUBES_RED for WW_PRESETPERM1_RED=1): the taps compiled in, as before. */
	return 1.0;
#endif
#endif
	if ( t4.x < -0.5 )
		return 1.0;	// a shadow light beyond the slot budget: unshadowed (the game's budget is unread)
	float3 r = P - t0.xyz;
	float3 rn = r + N * ( 1.5 * length( r ) * cellShadowTexel );
	float d = length( rn );
	float ref = ( d - 1.0 ) / max( t0.w, 0.001 );
	float3 a = rn / max( d, 0.001 );
	float3 u = normalize( cross( a, abs( a.z ) < 0.9 ? float3( 0.0, 0.0, 1.0 ) : float3( 1.0, 0.0, 0.0 ) ) );
	float3 v = cross( a, u );
	float s = 0.0;
	for ( int y = -1; y <= 1; y++ )
		for ( int x = -1; x <= 1; x++ )
			s += cellShadow.SampleCmp( _cellShadow_sampler, float4( a + ( float( x ) * u + float( y ) * v ) * cellShadowTexel, t4.x ), ref );
	return cellShadowFadeOn ? lerp( 1.0, s / 9.0, cellShadowFade[clamp( int( t4.x + 0.5 ), 0, 15 )] ) : s / 9.0;
}
Texture2D<float4> cellCtGbuf : register(t13, space0); SamplerState _cellCtGbuf_sampler : register(s13, space0);	// RGBA32F, bottom row first: view normal, linear depth (1e6 none)

float cellContactLocal( int i, float3 P )
{
	float4 t4 = cellLights.Load( i * CELL_TPL + 4 );
	if ( or( !cellCtOn, ( cellShadowOn && t4.y > 0.5 && t4.x > -0.5 ) ) )
		return 1.0;
	float4 t0 = cellLights.Load( i * CELL_TPL );
	// world to view: the inverse of cellRow's rotation (orthonormal) and translation
	float3 w = P - float3( cellRow[0].w, cellRow[1].w, cellRow[2].w ), l = t0.xyz - float3( cellRow[0].w, cellRow[1].w, cellRow[2].w );
	float3x3 R = float3x3( cellRow[0].xyz, cellRow[1].xyz, cellRow[2].xyz );	// columns = rows of view-to-world: R = its transpose
	float3 pv = mul(w, R), lv = mul(l, R);
	float3 toL = lv - pv;
	float dist = length( toL );
	if ( dist < 1e-3 )
		return 1.0;
	float3 dir = toL / dist * ( cellCtRed == 1 ? -1.0 : 1.0 );
	float z = max( -pv.z, 8.0 );
	float bias = 0.8 + z * 0.012, thickMin = 0.6 + z * 0.008, thickMax = max( 20.0, z * 0.1 );
	float reach = max( min( 0.15 * t0.w, 12.0 ), 4.0 );
	float march = min( dist - bias, reach );
	if ( march <= 0.0 )
		return 1.0;
	float3 S0 = pv + dir * bias, S1 = S0 + dir * march;
	float2 a = ( cellCtProj.xy * S0.xy + cellCtProj.zw * S0.z ) / max( -S0.z, 1e-4 );
	float2 b = ( cellCtProj.xy * S1.xy + cellCtProj.zw * S1.z ) / max( -S1.z, 1e-4 );
	float px = length( ( b - a ) * 0.5 * cellCtRect.zw );
	int steps = clamp( int( px ), 2, 48 );
	float jit = frac( 52.9829189 * frac( dot( gl_FragCoord.xy, float2( 0.06711056, 0.00583715 ) ) ) );
	for ( int k = 1; k <= 48; k++ ) {
		if ( k > steps )
			break;
		float t = ( float( k ) - jit ) / float( steps );
		float3 S = S0 + dir * ( march * t );
		if ( S.z >= -1.0 )
			break;
		float2 ndc = ( cellCtProj.xy * S.xy + cellCtProj.zw * S.z ) / -S.z;
		float2 q = ( ndc * 0.5 + 0.5 ) * cellCtRect.zw;
		if ( or( any( (q < (float2)( 0.0 )) ), any( (q >= cellCtRect.zw) ) ) )
			break;
		float delta = -S.z - cellCtGbuf.Load( int3(int2( q ), 0) ).a;
		if ( delta > thickMin && delta < thickMax )
			return 1.0 - clamp( 1.0 - march * t / reach, 0.0, 1.0 ) * 0.5;
	}
	return 1.0;
}

/* probe 7: the factors of shadow slots 0, 1, 2 in r, g, b as 2/255 + f x 253/255; 0 where that light is out
 * of reach or faces away (N.L under 0.05), or no light holds the slot */
float3 cellShadowProbe( float3 P, float3 N )
{
	float3 o = (float3)( 0.0 );
	int nL;
	int bL = cellLightsBegin( P, nL );
	for ( int k = 0; k < nL; k++ ) {
		int i = cellLightAt( bL, k );
		float slot = cellLights.Load( i * CELL_TPL + 4 ).x;
		if ( or( slot < -0.5, slot > 2.5 ) )
			continue;
		float4 t0 = cellLights.Load( i * CELL_TPL );
		float3 Lv = t0.xyz - P;
		float d = length( Lv );
		if ( or( d >= t0.w, dot( N, Lv / max( d, 0.001 ) ) < 0.05 ) )
			continue;
		o[int( slot + 0.5 )] = ( 2.0 + cellShadowF( i, P, N ) * 253.0 ) / 255.0;
	}
	return o;
}

// the bounce's irradiance at P, normal N: the three facing slabs blended by n^2, sampled half a
// voxel off the surface (the grid's voxels behind a wall are its other room's)
int2 cellRoomUnpack( float v )
{
	int k = int( v + 0.5 );
	return int2( k / 4096 - 1, k - ( k / 4096 ) * 4096 - 1 );
}
int2 cellRoomAt( float3 q )
{
	float3 c = floor( ( q - cellRoomsOrigin ) / cellRoomsCell );
	if ( or( any( (c < (float3)( 0.0 )) ), any( (c >= cellRoomsDims) ) ) )
		return (int2)( -1 );
	return cellRoomUnpack( cellRooms.Load( int4(int3( c ), 0) ).r );
}
/* lane GISHADE1: what a surface sees of the grid. A room open to the street (a shop's doorway) is the outdoors' room,
 * so the room clamp keeps a voxel whose centre is across the shop's wall: the sunlit facade's light on the ceiling
 * along that wall (Concord's hardware store: 20-56x the brute-force reference). A voxel counts only when the fine
 * room grid shows no solid cell between the surface's view point and the voxel's centre, tapped every half cell, the
 * centre itself left out (a voxel's centre may sit in the wall it straddles). Red: WW_CELL_PV_RED=nosee. Lane GIWALL2
 * (cellGiWall): the centre is tapped too, the taps never more than half a cell apart. */
bool cellRoomSolid( float3 q )
{
	float3 c = floor( ( q - cellRoomsOrigin ) / cellRoomsCell );
	if ( or( any( (c < (float3)( 0.0 )) ), any( (c >= cellRoomsDims) ) ) )
		return false;
	return cellRoomUnpack( cellRooms.Load( int4(int3( c ), 0) ).r ).x < 0;
}
// the view point: the room read (P + N x 0.75 cell, else 1.75) when air, else the center of the air cell beside
// either read nearest it (cellGiRoomSample's rule)
float3 cellGiSeeFrom( float3 P, float3 N )
{
	for ( int k = 0; k < 2; k++ ) {
		float3 q = P + N * ( ( k == 0 ? 0.75 : 1.75 ) * cellRoomsCell );
		if ( !cellRoomSolid( q ) )
			return q;
	}
	float3 aN = abs( N );
	int m = ( aN.x >= aN.y && aN.x >= aN.z ) ? 0 : ( aN.y >= aN.z ? 1 : 2 );
	int ta = m == 0 ? 1 : 0, tb = m == 2 ? 1 : 2;
	float best = 3.0;
	float3 r = P + N * ( 0.75 * cellRoomsCell );
	for ( int k = 0; k < 2; k++ ) {
		float3 q = P + N * ( ( k == 0 ? 0.75 : 1.75 ) * cellRoomsCell );
		float3 u = ( q - cellRoomsOrigin ) / cellRoomsCell;
		float3 f = u - floor( u );
		for ( int sa = -1; sa <= 1; sa++ ) {
			for ( int sb = -1; sb <= 1; sb++ ) {
				if ( sa == 0 && sb == 0 )
					continue;
				float3 e = (float3)( 0.0 );
				e[ta] = float( sa );
				e[tb] = float( sb );
				float3 qq = q + e * cellRoomsCell;
				if ( cellRoomSolid( qq ) )
					continue;
				float d = ( sa > 0 ? 1.0 - f[ta] : sa < 0 ? f[ta] : 0.0 ) + ( sb > 0 ? 1.0 - f[tb] : sb < 0 ? f[tb] : 0.0 );
				if ( d < best ) {
					best = d;
					r = cellRoomsOrigin + ( floor( ( qq - cellRoomsOrigin ) / cellRoomsCell ) + 0.5 ) * cellRoomsCell;
				}
			}
		}
	}
	return r;
}
/* lane GIWALL2: the wall test of a voxel c seen from `from`: 2 seen (no solid cell on the segment, its centre air), 1
 * buried (its centre in the wall the surface faces: air up to the first solid tap, solid from there to the centre), 0
 * not seen (a solid centre with `loose` false, or air past a wall: the voxel is behind it). GISHADE1's taps left the
 * centre out and stopped at 12: at Concord's 267-unit voxels a segment's taps fell more than a room cell apart, past a
 * one-cell wall, and a voxel straddling the store's wall with its centre inside it passed (the hot ceiling points: 64%
 * of the blend in solid-centre voxels, 87.5% behind a wall, the sunlit street's light). Now the taps run to the centre,
 * no tap more than half a cell from the last on any axis, so a wall a cell thick is never stepped over. loose (the
 * fills' and the volumes' last answer when nothing is seen): walk on past the first solid tap to tell buried from
 * behind. Red
 * WW_CELL_GIWALL_RED=off (cellPassRed 8192): GISHADE1's taps exactly (2 or 0). */
int cellGiWall( float3 from, int3 c, bool loose )
{
	float3 d = cellGiOrigin + ( float3( c ) + 0.5 ) * cellGiVoxel - from;
	float3 a = abs( d );
	bool red = ( cellPassRed & 8192 ) != 0;	// one loop for both: the program's size (GIDARK1's link limit)
	int nt = red ? clamp( int( ceil( length( d ) / ( 0.5 * cellRoomsCell ) ) ), 1, 12 )
	             : clamp( int( ceil( max( a.x, max( a.y, a.z ) ) / ( 0.5 * cellRoomsCell ) ) ), 1, 255 );
	bool hit = false;
	for ( int j = 1; j <= ( red ? nt : nt + 1 ); j++ ) {	// j = nt + 1: the centre itself
		bool s = cellRoomSolid( from + d * ( float( j ) / float( nt + 1 ) ) );
		if ( s && ( or( red, !loose ) ) )
			return 0;
		if ( hit && !s )
			return 0;	// air past the wall: behind it
		hit = or( hit, s );
	}
	return hit ? 1 : 2;
}
bool cellGiSeen( float3 from, int3 c )
{
	if ( ( cellPassRed & 16 ) != 0 )
		return true;
	return cellGiWall( from, c, false ) == 2;
}
// the blend of the 8 voxels' slots of room L, trilinear weights (sum in .rgba, weight in ws),
// only the voxels seen from `from` (cellGiSeen)
float4 cellGiRoomBlend( float3 P, float3 N, int2 L, float3 from, out float ws )
{
	float3 g = ( P + N * ( 0.5 * cellGiVoxel ) - cellGiOrigin ) / cellGiVoxel - 0.5;
	int3 i0 = int3( floor( g ) );
	float3 f = g - float3( i0 );
	int3 dm = int3( cellGiDims );
	int3 sl = int3( N.x >= 0.0 ? 0 : 1, N.y >= 0.0 ? 2 : 3, N.z >= 0.0 ? 4 : 5 ) * dm.z;
	float3 n2 = N * N;
	float4 s = (float4)( 0.0 );
	ws = 0.0;
	for ( int k = 0; k < 8; k++ ) {
		int3 o = int3( k & 1, ( k >> 1 ) & 1, ( k >> 2 ) & 1 );
		int3 c = clamp( i0 + o, (int3)( 0 ), dm - 1 );
		float3 t = lerp( 1.0 - f, f, float3( o ) );
		float w = t.x * t.y * t.z;
		int2 S = cellRoomUnpack( cellGiSlots.Load( int4(c, 0) ).r );
		int base = ( S.x >= 0 && ( or( S.x == L.x, S.x == L.y ) ) ) ? 0 : ( S.y >= 0 && ( or( S.y == L.x, S.y == L.y ) ) ) ? 6 * dm.z : -1;
		if ( or( or( base < 0, w <= 0.0 ), !cellGiSeen( from, c ) ) )
			continue;
		/* the read cell and the voxel may both hold two rooms (an opening): the mean of both lit slots (debug
		 * WW_CELL_PV_RED=firstslot). One loop for both slots: a second inlined copy pushed fo4_cell past what the driver links. */
		bool two = ( cellPassRed & 256 ) == 0 && L.x >= 0 && L.y >= 0 && L.x != L.y && base == 0
			&& S.y >= 0 && ( or( S.y == L.x, S.y == L.y ) );
		float4 v = (float4)( 0.0 );
		float nv = 0.0;
		for ( int q = 0; q < ( two ? 2 : 1 ); q++ ) {
			int bq = q == 0 ? base : 6 * dm.z;
			float4 u = n2.x * cellGi.Load( int4(int3( c.xy, c.z + bq + sl.x ), 0) )
			       + n2.y * cellGi.Load( int4(int3( c.xy, c.z + bq + sl.y ), 0) )
			       + n2.z * cellGi.Load( int4(int3( c.xy, c.z + bq + sl.z ), 0) );
			if ( u.a > 0.0 ) {
				v += u;
				nv += 1.0;
			}
		}
		if ( nv <= 0.0 )
			continue;   // an empty slot (no probe of its room reached the voxel): no weight
		v /= nv;
		s += w * v;
		ws += w;
	}
	return s;
}
// the blend of only the voxels of the surface's own room, read at P + N x 0.75 cell, else 1.75 cell; failing that, the
// nearest air cell beside either read whose blend has weight. .a < 0: no room with weight (use the plain trilinear).
// Ls: the surface's room whether or not its blend has weight (-1: none), for the gap fill;
// outdoorsOnly: the search borrows only an outdoors (0) cell (cellGiSample's lend rule)
float4 cellGiRoomSample( float3 P, float3 N, float3 from, bool outdoorsOnly, out int2 Ls )
{
	float ws;
	int2 L = cellRoomAt( P + N * ( 0.75 * cellRoomsCell ) );
	if ( L.x < 0 )
		L = cellRoomAt( P + N * ( 1.75 * cellRoomsCell ) );
	Ls = L;
	if ( L.x >= 0 && !( outdoorsOnly && L.x != 0 && L.y != 0 ) ) {	// lent: an indoors read is not taken
		float4 s = cellGiRoomBlend( P, N, L, from, ws );
		if ( ws > 0.0 )
			return s / ws;
	}
	float3 aN = abs( N );
	int m = ( aN.x >= aN.y && aN.x >= aN.z ) ? 0 : ( aN.y >= aN.z ? 1 : 2 );
	int ta = m == 0 ? 1 : 0, tb = m == 2 ? 1 : 2;
	float best = 3.0, bestAny = 3.0;
	bool direct = L.x >= 0;
	bool found = false;
	float4 r = (float4)( 0.0 );
	for ( int k = 0; k < 2; k++ ) {
		float3 q = P + N * ( ( k == 0 ? 0.75 : 1.75 ) * cellRoomsCell );
		float3 u = ( q - cellRoomsOrigin ) / cellRoomsCell;
		float3 f = u - floor( u );
		for ( int sa = -1; sa <= 1; sa++ ) {
			for ( int sb = -1; sb <= 1; sb++ ) {
				if ( sa == 0 && sb == 0 )
					continue;
				float3 e = (float3)( 0.0 );
				e[ta] = float( sa );
				e[tb] = float( sb );
				int2 c = cellRoomAt( q + e * cellRoomsCell );
				if ( c.x < 0 )
					continue;
				float d = ( sa > 0 ? 1.0 - f[ta] : sa < 0 ? f[ta] : 0.0 ) + ( sb > 0 ? 1.0 - f[tb] : sb < 0 ? f[tb] : 0.0 );
				if ( !direct && d < bestAny ) {
					bestAny = d;
					Ls = c;	// the nearest air cell, with weight or not
				}
				if ( or( d >= best, ( outdoorsOnly && c.x != 0 && c.y != 0 ) ) )
					continue;
				float4 s = cellGiRoomBlend( P, N, c, from, ws );
				if ( ws > 0.0 ) {
					found = true;
					best = d;
					r = s / ws;
				}
			}
		}
	}
	return found ? r : (float4)( -1.0 );
}

/* the gap fill. A surface whose blend has no valid weight (no probe reached the 8 voxels: today
 * black, or with rooms the plain trilinear, which ignores walls) takes the weighted mean of the valid voxels within
 * 2 voxels (the 4x4x4 block round the sample), of the surface's own room (L) when the grid has rooms; weight
 * (1 - d / 2.5)^2 in voxels. Returns rgba / the weight (cellGiE divides by .a); .a = 0: nothing in reach.
 * of a room (not the last fill) only the voxels seen from `from` (cellGiWall): the room's slot is no wall
 * guard where the room is open to the street (a shop is the outdoors' room); none seen, the room's buried voxels
 * (centre in the wall the surface faces), never one behind a wall. Red WW_CELL_GIWALL_RED=off: every one. */
static const int2 cellGiAnyRoom = (int2)( -2 );	// lane WALLSTACK1: cellGiFillAt's L for the last fill (every voxel's first slot, as the trilinear reads)
float4 cellGiFillAt( float3 P, float3 N, int2 L, float3 from )
{
	float3 g = ( P + N * ( 0.5 * cellGiVoxel ) - cellGiOrigin ) / cellGiVoxel - 0.5;
	int3 i0 = int3( floor( g ) );
	int3 dm = int3( cellGiDims );
	int3 sl = int3( N.x >= 0.0 ? 0 : 1, N.y >= 0.0 ? 2 : 3, N.z >= 0.0 ? 4 : 5 ) * dm.z;
	float3 n2 = N * N;
	float4 s = (float4)( 0.0 ), sb = (float4)( 0.0 );
	float ws = 0.0, wb = 0.0;
	bool see = cellGiRooms && any(L != cellGiAnyRoom) && ( cellPassRed & ( 16 | 8192 ) ) == 0;
	for ( int k = 0; k < 64; k++ ) {
		int3 c = i0 + int3( k & 3, ( k >> 2 ) & 3, ( k >> 4 ) & 3 ) - 1;
		if ( or( any( (c < (int3)( 0 )) ), any( (c >= dm) ) ) )
			continue;
		float w = 1.0 - length( float3( c ) - g ) / 2.5;
		if ( w <= 0.0 )
			continue;
		int base = 0;
		if ( cellGiRooms && any(L != cellGiAnyRoom) ) {
			if ( L.x < 0 )
				return (float4)( 0.0 );	// rooms but no room here: nothing safe to borrow
			int2 S = cellRoomUnpack( cellGiSlots.Load( int4(c, 0) ).r );
			base = ( S.x >= 0 && ( or( S.x == L.x, S.x == L.y ) ) ) ? 0 : ( S.y >= 0 && ( or( S.y == L.x, S.y == L.y ) ) ) ? 6 * dm.z : -1;
			if ( base < 0 )
				continue;
		}
		float4 v = n2.x * cellGi.Load( int4(int3( c.xy, c.z + base + sl.x ), 0) )
		       + n2.y * cellGi.Load( int4(int3( c.xy, c.z + base + sl.y ), 0) )
		       + n2.z * cellGi.Load( int4(int3( c.xy, c.z + base + sl.z ), 0) );
		if ( v.a <= 0.0 )
			continue;
		int sn = see ? cellGiWall( from, c, true ) : 2;
		if ( sn == 0 )
			continue;
		w *= w;
		if ( sn == 1 ) {
			sb += w * v;
			wb += w;
			continue;
		}
		s += w * v;
		ws += w;
	}
	return ws > 0.0 ? s / ws : wb > 0.0 ? sb / wb : (float4)( 0.0 );
}

/* no room's blend has weight: the voxels of the 4x4x4 block the surface sees (any room), weight
 * (1 - d / 2.5)^2 in voxels, before the plain trilinear (which sees through walls). .a = 0: none seen.
 * Lane GIWALL2: the last answer before the trilinear (which reads through walls), so an outdoor recess (GIBLACK1) or a
 * pocket keeps its light where every voxel round it now fails the wall test: none seen, the buried voxels (centre in
 * the wall the surface faces, cellGiWall 1), never one behind a wall. Red WW_CELL_GIWALL_RED=off: none. */
float4 cellGiSeenFill( float3 P, float3 N, float3 from )
{
	float3 g = ( P + N * ( 0.5 * cellGiVoxel ) - cellGiOrigin ) / cellGiVoxel - 0.5;
	int3 i0 = int3( floor( g ) );
	int3 dm = int3( cellGiDims );
	int3 sl = int3( N.x >= 0.0 ? 0 : 1, N.y >= 0.0 ? 2 : 3, N.z >= 0.0 ? 4 : 5 ) * dm.z;
	float3 n2 = N * N;
	float4 s = (float4)( 0.0 ), sb = (float4)( 0.0 );
	float ws = 0.0, wb = 0.0;
	bool loose = ( cellPassRed & ( 16 | 8192 ) ) == 0;
	for ( int k = 0; k < 64; k++ ) {
		int3 c = i0 + int3( k & 3, ( k >> 2 ) & 3, ( k >> 4 ) & 3 ) - 1;
		if ( or( any( (c < (int3)( 0 )) ), any( (c >= dm) ) ) )
			continue;
		float w = 1.0 - length( float3( c ) - g ) / 2.5;
		if ( w <= 0.0 )
			continue;
		int see = ( cellPassRed & 16 ) != 0 ? 2 : cellGiWall( from, c, loose );
		if ( see == 0 )
			continue;
		float4 v = n2.x * cellGi.Load( int4(int3( c.xy, c.z + sl.x ), 0) )
		       + n2.y * cellGi.Load( int4(int3( c.xy, c.z + sl.y ), 0) )
		       + n2.z * cellGi.Load( int4(int3( c.xy, c.z + sl.z ), 0) );
		if ( v.a <= 0.0 )
			continue;
		w *= w;
		if ( see == 1 ) {
			sb += w * v;
			wb += w;
			continue;
		}
		s += w * v;
		ws += w;
	}
	return ws > 0.0 ? s / ws : wb > 0.0 ? sb / wb : (float4)( 0.0 );
}
#ifdef WW_CELL_GI_MARCH
/* lane MARCHLINK1: cellGiFillAt( P, N, L, from ) (when `room`) and cellGiSeenFill( P, N, from ) (in `seen`) in one pass
 * over the 4x4x4 block, one wall test a voxel: the stranded answer's two fills, inlined at each of its sites, took
 * fo4_effectdecalcsm past what the driver links in the march mode. Same terms in the same order: the same values. */
float4 cellGiFillPair( float3 P, float3 N, int2 L, float3 from, bool room, out float4 seen )
{
	float3 g = ( P + N * ( 0.5 * cellGiVoxel ) - cellGiOrigin ) / cellGiVoxel - 0.5;
	int3 i0 = int3( floor( g ) );
	int3 dm = int3( cellGiDims );
	int3 sl = int3( N.x >= 0.0 ? 0 : 1, N.y >= 0.0 ? 2 : 3, N.z >= 0.0 ? 4 : 5 ) * dm.z;
	float3 n2 = N * N;
	float4 s = (float4)( 0.0 ), sb = (float4)( 0.0 ), t = (float4)( 0.0 ), tb = (float4)( 0.0 );
	float ws = 0.0, wb = 0.0, wt = 0.0, wtb = 0.0;
	bool byRoom = cellGiRooms && any(L != cellGiAnyRoom);
	bool see = byRoom && ( cellPassRed & ( 16 | 8192 ) ) == 0;
	bool seenOn = ( cellPassRed & 16 ) == 0;
	bool loose = ( cellPassRed & ( 16 | 8192 ) ) == 0;
	bool roomNone = room && byRoom && L.x < 0;	// rooms but no room here: nothing safe to borrow
	room = room && !roomNone;
	for ( int k = 0; k < 64; k++ ) {
		int3 c = i0 + int3( k & 3, ( k >> 2 ) & 3, ( k >> 4 ) & 3 ) - 1;
		if ( or( any( (c < (int3)( 0 )) ), any( (c >= dm) ) ) )
			continue;
		float w = 1.0 - length( float3( c ) - g ) / 2.5;
		if ( w <= 0.0 )
			continue;
		int base = 0;
		bool rOn = room;
		if ( rOn && byRoom ) {
			int2 S = cellRoomUnpack( cellGiSlots.Load( int4(c, 0) ).r );
			base = ( S.x >= 0 && ( or( S.x == L.x, S.x == L.y ) ) ) ? 0 : ( S.y >= 0 && ( or( S.y == L.x, S.y == L.y ) ) ) ? 6 * dm.z : -1;
			rOn = base >= 0;
		}
		int wl = ( or( ( rOn && see ), seenOn ) ) ? cellGiWall( from, c, loose ) : 2;
		w *= w;
		float4 v0 = n2.x * cellGi.Load( int4(int3( c.xy, c.z + sl.x ), 0) )
		        + n2.y * cellGi.Load( int4(int3( c.xy, c.z + sl.y ), 0) )
		        + n2.z * cellGi.Load( int4(int3( c.xy, c.z + sl.z ), 0) );
		if ( rOn ) {
			float4 v = base == 0 ? v0 : n2.x * cellGi.Load( int4(int3( c.xy, c.z + base + sl.x ), 0) )
			                        + n2.y * cellGi.Load( int4(int3( c.xy, c.z + base + sl.y ), 0) )
			                        + n2.z * cellGi.Load( int4(int3( c.xy, c.z + base + sl.z ), 0) );
			int sn = see ? wl : 2;
			if ( v.a > 0.0 && sn == 1 ) {
				sb += w * v;
				wb += w;
			} else if ( v.a > 0.0 && sn == 2 ) {
				s += w * v;
				ws += w;
			}
		}
		if ( seenOn && v0.a > 0.0 && wl == 1 ) {
			tb += w * v0;
			wtb += w;
		} else if ( seenOn && v0.a > 0.0 && wl == 2 ) {
			t += w * v0;
			wt += w;
		}
	}
	seen = wt > 0.0 ? t / wt : wtb > 0.0 ? tb / wtb : (float4)( 0.0 );
	return ws > 0.0 ? s / ws : wb > 0.0 ? sb / wb : (float4)( 0.0 );
}
#endif

/* a stranded surface (both own reads solid, cellGiSample's lend rule) sits in air the room grid could not resolve,
 * so guessing the outdoors can light a building's inside with the sky. Search the air cells at P + N x (0.75, 1.75,
 * 2.75) cell, up to 2 cells round each in the surface plane, keeping the nearest indoor and nearest outdoor (room 0). */
void cellGiStrandSearch( float3 P, float3 N, out float dIn, out int2 LIn, out float3 CIn, out float dOut, out int2 LOut, out float3 COut )
{
	dIn = 1e9;
	dOut = 1e9;
	LIn = (int2)( -1 );
	LOut = (int2)( -1 );
	CIn = P;
	COut = P;
	float3 aN = abs( N );
	int m = ( aN.x >= aN.y && aN.x >= aN.z ) ? 0 : ( aN.y >= aN.z ? 1 : 2 );
	int ta = m == 0 ? 1 : 0, tb = m == 2 ? 1 : 2;
	for ( int t = 0; t < 3; t++ ) {
		float3 q0 = P + N * ( ( float( t ) + 0.75 ) * cellRoomsCell );
		for ( int sa = -2; sa <= 2; sa++ ) {
			for ( int sb = -2; sb <= 2; sb++ ) {
				float3 e = (float3)( 0.0 );
				e[ta] = float( sa );
				e[tb] = float( sb );
				float3 q = q0 + e * cellRoomsCell;
				int2 L = cellRoomAt( q );
				if ( L.x < 0 )
					continue;
				float3 lo = cellRoomsOrigin + floor( ( q - cellRoomsOrigin ) / cellRoomsCell ) * cellRoomsCell;
				float3 c = lo + 0.5 * cellRoomsCell;
				if ( dot( c - P, N ) <= 0.0 )
					continue;
				float d = length( max( max( lo - P, P - lo - cellRoomsCell ), (float3)( 0.0 ) ) );
				if ( or( L.x == 0, L.y == 0 ) ) {
					if ( d < dOut ) {
						dOut = d;
						LOut = L;
						COut = c;
					}
				} else if ( d < dIn ) {
					dIn = d;
					LIn = L;
					CIn = c;
				}
			}
		}
	}
}
/* a stranded surface's answer from room L seen from the air cell's center C: its blend (path 6), indoors the room's
 * fill (7), the seen fill from C (8), indoors with nothing in reach the sealed room's dark (9); outdoors with nothing:
 * .a < 0 (the plain trilinear). */
float4 cellGiStrandAnswer( float3 P, float3 N, int2 L, float3 C, out int path )
{
	bool indoor = L.x != 0 && L.y != 0;
	float ws;
	float4 s = cellGiRoomBlend( P, N, L, C, ws );
	path = 6;
	if ( ws > 0.0 )
		return s / ws;
#ifdef WW_CELL_GI_MARCH
	float4 fs;
	float4 fr = cellGiFillPair( P, N, L, C, indoor, fs );	// lane MARCHLINK1: both fills in one pass
	if ( indoor ) {
		path = 7;
		if ( fr.a > 0.01 )
			return fr;
	}
	path = 8;
	if ( ( cellPassRed & 16 ) == 0 && fs.a > 0.01 )
		return fs;
#else
	if ( indoor ) {
		path = 7;
		float4 f = cellGiFillAt( P, N, L, C );
		if ( f.a > 0.01 )
			return f;
	}
	path = 8;
	if ( ( cellPassRed & 16 ) == 0 ) {
		float4 f = cellGiSeenFill( P, N, C );
		if ( f.a > 0.01 )
			return f;
	}
#endif
	path = indoor ? 9 : 5;
	return indoor ? float4( 0.0, 0.0, 0.0, 1.0 ) : (float4)( -1.0 );
}
/* lane GIEDGE1: 1 inside the box, falling to 0 across its x/y edge voxels (g in voxels). CLAMP_TO_EDGE smeared the
 * edge column's sky-filled ground layers sideways into a sheet at a fixed world z (GISTRIPE1); past the box the
 * weather ambient applies alone. Exactly 1 more than cellGiPad voxels in. */
float cellGiEdgeFade( float2 g )
{
	if ( cellGiPad <= 0.0 )
		return 1.0;
	float2 e = min( g, cellGiDims.xy - g );
	return clamp( min( e.x, e.y ) / cellGiPad, 0.0, 1.0 );
}
// the hardware trilinear of the one grid (slot 0): the bake's own light at P, walls not seen
float4 cellGiTrilinear( float3 P, float3 N )
{
	float3 g = ( P + N * ( 0.5 * cellGiVoxel ) - cellGiOrigin ) / cellGiVoxel;
	float2 xy = g.xy / cellGiDims.xy;
	float z = clamp( g.z, 0.5, cellGiDims.z - 0.5 );
	float depth = float( textureSize( cellGi, 0 ).z );	// lanes EMISSIVEGRID1 + SPECOCC1: the fine atlas / sky slabs may follow
	float3 n2 = N * N;
	float4 s = n2.x * cellGi.Sample( _cellGi_sampler, float3( xy, ( z + ( N.x >= 0.0 ? 0.0 : 1.0 ) * cellGiDims.z ) / depth ) )
	     + n2.y * cellGi.Sample( _cellGi_sampler, float3( xy, ( z + ( N.y >= 0.0 ? 2.0 : 3.0 ) * cellGiDims.z ) / depth ) )
	     + n2.z * cellGi.Sample( _cellGi_sampler, float3( xy, ( z + ( N.z >= 0.0 ? 4.0 : 5.0 ) * cellGiDims.z ) / depth ) );
	float f = cellGiEdgeFade( g.xy );
	return f < 1.0 ? s * f : s;
}
/* only indoor cells reached: the indoor room; only outdoors: room 0; both: the nearer, or within a cell of each other
 * (a thin wall the room grid cannot resolve) the mean of both. Nothing reached: .a < 0 (the plain trilinear). */
float4 cellGiStrandPick( float3 P, float3 N, out int path )
{
	float dIn, dOut;
	int2 LIn, LOut;
	float3 CIn, COut;
	cellGiStrandSearch( P, N, dIn, LIn, CIn, dOut, LOut, COut );
	path = 5;
	if ( LIn.x < 0 && LOut.x < 0 )
		return (float4)( -1.0 );
		/* buried three cells deep with only the outdoors in reach, the search may have gone round a corner: take the darker
		 * of the outdoor answer and the plain trilinear (debug WW_CELL_PV_RED=lonesky). A tie takes the mean of both answers
		 * (debug tiepick: the answer nearer the trilinear). Keep call sites few: each is inlined into every cellGiSample,
		 * and more took fo4_cell past what the driver links. */
	const float3 Y = float3( 0.2126, 0.7152, 0.0722 );
	bool lone = LIn.x < 0 && ( cellPassRed & 1024 ) == 0 && cellRoomSolid( P + N * ( 0.75 * cellRoomsCell ) )
		&& cellRoomSolid( P + N * ( 1.75 * cellRoomsCell ) ) && cellRoomSolid( P + N * ( 2.75 * cellRoomsCell ) );
	bool tie = LIn.x >= 0 && LOut.x >= 0 && abs( dIn - dOut ) < cellRoomsCell;
	bool wantIn = LIn.x >= 0 && ( or( or( tie, LOut.x < 0 ), dIn < dOut ) );
	bool wantOut = LOut.x >= 0 && ( or( tie, !wantIn ) );
	int pi = 5, po = 5;
	float4 a = (float4)( -1.0 ), o = (float4)( -1.0 );
#ifdef WW_CELL_GI_MARCH
	/* lane MARCHLINK1: one answer site in the march mode. GIWALL2's wall-tested fills made each inlined answer bigger,
	 * and the two sites took fo4_effectdecalcsm and pbrm_cellcsm past what the driver links. The bounds are not
	 * constants, so the loop is not unrolled into two copies again. Same calls in the same order: same answers. */
	for ( int t = wantIn ? 0 : 1; t <= ( wantOut ? 1 : 0 ); t++ ) {
		int pt = 5;
		float4 r = cellGiStrandAnswer( P, N, t == 0 ? LIn : LOut, t == 0 ? CIn : COut, pt );
		if ( t == 0 ) {
			a = r;
			pi = pt;
		} else {
			o = r;
			po = pt;
		}
	}
#else
	if ( wantIn )
		a = cellGiStrandAnswer( P, N, LIn, CIn, pi );
	if ( wantOut )
		o = cellGiStrandAnswer( P, N, LOut, COut, po );
#endif
	bool needT = or( lone, ( tie && o.a >= 0.0 && ( cellPassRed & 512 ) != 0 ) );
	float4 tr = needT ? cellGiTrilinear( P, N ) : (float4)( 0.0 );
	if ( lone ) {
		if ( o.a >= 0.0 && tr.a > 0.0 && dot( tr.rgb, Y ) / tr.a < dot( o.rgb, Y ) / max( o.a, 1e-6 ) ) {
			path = 4;
			return tr;
		}
		path = po;
		return o;
	}
	if ( or( !tie, o.a < 0.0 ) ) {
		path = wantIn ? pi : po;
		return wantIn ? a : o;
	}
	if ( ( cellPassRed & 512 ) == 0 ) {
		path = ( or( pi == 6, po == 6 ) ) ? 6 : pi;
		return 0.5 * ( a + o );
	}
	float t = dot( tr.rgb, Y );
	bool useIn = abs( dot( a.rgb, Y ) - t ) <= abs( dot( o.rgb, Y ) - t );
	path = useIn ? pi : po;
	return useIn ? a : o;
}

static int cellGiPath = 0;	// probe 95: the branch cellGiSample took (1 room blend, 2 room fill, 3 seen fill, 4 trilinear, 5 stranded with nothing reached; stranded 6 room blend, 7 room fill, 8 seen fill, 9 sealed dark; 10 the last fill)
float4 cellGiSampleFull( float3 P, float3 N )
{
	bool fill = cellGiFill && cellInterior && !cellGiSky;	// interiors only: an exterior's gap keeps the weather's ambient (cellGiSkyK), also under debug keepamb
	int2 Ls = (int2)( -1 );
	if ( cellGiRooms && ( cellPassRed & 8 ) == 0 ) {	// debug noclamp: WW_CELL_PV_RED
		float3 from = cellGiSeeFrom( P, N );
		/* the lend rule: outdoors, a surface whose own reads (P + N x 0.75 and 1.75 cell) are both solid, or whose 1.75 read
		 * lands indoors, sits in air the room grid could not resolve; its neighbor search could borrow a sealed pocket's
		 * dark across the wall. Debug WW_CELL_PV_RED=borrow: the old search. */
		int2 L2 = cellRoomAt( P + N * ( 1.75 * cellRoomsCell ) );
		bool lend = cellGiSky && ( cellPassRed & 32 ) == 0 && cellRoomSolid( P + N * ( 0.75 * cellRoomsCell ) )
		         && ( or( L2.x < 0, ( L2.x != 0 && L2.y != 0 ) ) );
		/* such a stranded surface takes the room its evidence search reaches (cellGiStrandPick), else the plain trilinear,
		 * never the weather's flat ambient. Debug WW_CELL_PV_RED=strand: outdoors-only borrow, then the flat ambient. */
		if ( lend && ( cellPassRed & 128 ) == 0 ) {
			int sp;
			float4 sr = cellGiStrandPick( P, N, sp );
			cellGiPath = sp;
			if ( sr.a >= 0.0 )
				return sr;
			if ( ( cellPassRed & 64 ) != 0 )
				return (float4)( 0.0 );	// debug WW_CELL_PV_RED=lendamb: the flat ambient
			return cellGiTrilinear( P, N );
		}
		float4 r = cellGiRoomSample( P, N, from, lend, Ls );
		cellGiPath = 1;
		if ( or( r.a > 0.01, ( r.a >= 0.0 && !fill ) ) )
			return r;
		cellGiPath = 2;
		if ( fill ) {	// a gap, or no room with weight (the plain trilinear ignores walls)
			float4 f = cellGiFillAt( P, N, Ls, from );
			if ( or( f.a > 0.01, r.a >= 0.0 ) )
				return f.a > 0.01 ? f : r;
		}
		cellGiPath = 3;
		int2 Lf = cellRoomAt( from );
		if ( ( cellPassRed & 16 ) == 0 && ( or( or( !lend, Lf.x == 0 ), Lf.y == 0 ) ) ) {
			float4 f = cellGiSeenFill( P, N, from );
			if ( f.a > 0.01 )
				return f;
		}
		/* a lent surface with nothing outdoors to borrow takes the plain trilinear (path 5), never the weather's flat
		 * ambient: such a surface sits in a recess. Debug WW_CELL_PV_RED=lendamb. */
		cellGiPath = lend ? 5 : 4;
		if ( lend && ( cellPassRed & ( 64 | 128 ) ) != 0 )
			return (float4)( 0.0 );	// debug WW_CELL_PV_RED=lendamb, strand: the flat ambient
	} else
		cellGiPath = 4;
	float4 s = cellGiTrilinear( P, N );
	/* the last fill (path 10): a surface whose room no probe reached and whose seen fill finds nothing would sit on empty
	 * voxels inside the wall and draw black; take the trilinear's reach over valid voxels only (debug nolastfill).
	 * Keep one call site: the program's size (see cellPassOut). */
	if ( fill && s.a <= 0.01 && ( or( !cellGiRooms, ( cellPassRed & ( 8 | 4096 ) ) == 0 ) ) ) {	// the noclamp red keeps its old tail
		float4 f = cellGiFillAt( P, N, cellGiRooms ? cellGiAnyRoom : Ls, P );	// lane GIWALL2: the last fill reads through walls (no view point)
		if ( f.a > 0.01 ) {
			if ( cellGiRooms )
				cellGiPath = 10;
			return f;
		}
	}
	return s;
}

// a unit normal as one float: octahedral, 11 bits a component, exact in a float's mantissa
float cellGiKeyPack( float3 n )
{
	float2 o = n.xy / ( abs( n.x ) + abs( n.y ) + abs( n.z ) );
	if ( n.z < 0.0 )
		o = ( 1.0 - abs( o.yx ) ) * float2( o.x >= 0.0 ? 1.0 : -1.0, o.y >= 0.0 ? 1.0 : -1.0 );
	float2 q = floor( clamp( o * 0.5 + 0.5, 0.0, 1.0 ) * 2047.0 + 0.5 );
	return q.x * 2048.0 + q.y;
}

float3 cellGiKeyNormal( float k )
{
	float2 q = float2( floor( k / 2048.0 ), mod( k, 2048.0 ) );
	float2 o = q / 2047.0 * 2.0 - 1.0;
	float3 n = float3( o, 1.0 - abs( o.x ) - abs( o.y ) );
	if ( n.z < 0.0 )
		n.xy = ( 1.0 - abs( n.yx ) ) * float2( n.x >= 0.0 ? 1.0 : -1.0, n.y >= 0.0 ? 1.0 : -1.0 );
	return normalize( n );
}
#ifdef WW_CELL_PRT
#ifdef WW_CELL_GI_MARCH
#ifdef WW_CELL_GI_MARCH_ONCE
/* lane GIBLACKLINK1: the march once a fragment (fo4_default: its main primes it). Every call site inlines the whole chain,
 * and ten copies took fo4_cellcsm past NVIDIA's 64k-instruction link limit; every caller reads this (P, N). */
static float4 cellGiMarchMemo = (float4)( 0.0 );
void cellGiMarchPrime( float3 P, float3 N )
{
	cellGiMarchMemo = cellGiSampleFull( P, N );
}
float4 cellGiSample( float3 P, float3 N )
{
	return cellGiMarchMemo;
}
#else
float4 cellGiSample( float3 P, float3 N )
{
	return cellGiSampleFull( P, N );
}
#endif
#else
#include "../ao/cell_gihalfat.hlsli"

// the volume of a point with no mask: the fine room grid's two names at P + N x 0.75 cell, else 1.75; -2: unknown
int2 cellGiVolAt( float3 P, float3 N )
{
	if ( !cellGiRooms )
		return (int2)( -2 );
	for ( int k = 0; k < 2; k++ ) {
		int2 L = cellRoomAt( P + N * ( ( k == 0 ? 0.75 : 1.75 ) * cellRoomsCell ) );
		if ( L.x >= 0 )
			return L;
	}
	return (int2)( -2 );
}

#ifdef WW_CELL_RTDDGI
#define cellGiVolume cellGiVolumeGrid	// lane RTXGI1 M2: the baked grid's read keeps its body; the volume's read wraps it below
#endif
float4 cellGiVolume( float3 P, float3 N, int2 L )
{
	float3 g = ( P + N * ( 0.5 * cellGiVoxel ) - cellGiOrigin ) / cellGiVoxel - 0.5;
	int3 i0 = int3( floor( g ) );
	float3 f = g - float3( i0 );
	int3 r0 = int3( floor( g + 0.5 ) );
	int3 dm = int3( cellGiDims );
	int3 sl = int3( N.x >= 0.0 ? 0 : 1, N.y >= 0.0 ? 2 : 3, N.z >= 0.0 ? 4 : 5 ) * dm.z;
	float3 n2 = N * N;
	/* outdoors (a sky-lit grid), a surface in no interior volume whose own reads (0.75, 1.75 cell) are solid stands in the
	 * outdoors' volume (room 0), as the march's stranded pick takes it (probe 95: 6, 7); buried a third cell deep, the darker
	 * of that and the trilinear (lane GIDARK1's guard: a pillar beside a shop front took the street's light) */
	bool strand = cellGiRooms && cellGiSky && L.x == -2;
	if ( strand )
		L = int2( 0, -1 );
	bool anyRoom = or( !cellGiRooms, L.x == -2 );
	/* lane GIWALL2: a volume's slot is no wall guard (Concord's hardware store is open to the street: one volume with the
	 * street, its voxels straddling the wall read the sun outside), so every pass keeps only the voxels the surface sees
	 * (cellGiWall from cellGiSeeFrom, the march's rule): no solid centre, no wall between. Pass 1 keeps the buried voxels
	 * apart as the last answer before the trilinear, which reads through walls (an outdoor recess, GIBLACK1). Red
	 * WW_CELL_GIWALL_RED=off: no test, the old weights. */
	bool wall = cellGiRooms && ( cellPassRed & ( 16 | 8192 ) ) == 0;
	float3 from = wall ? cellGiSeeFrom( P, N ) : P;
	float4 own = (float4)( 0.0 ), oth = (float4)( 0.0 ), bur = (float4)( 0.0 );
	float wOwn = 0.0, wOth = 0.0, tAcc = 0.0, tBest = 0.0, wBur = 0.0;
	int other = -1;
	bool nearPick = false;
	// pass 0: the 8 corners, own volume (and the thresholds' weight); 1: none had it, the 27 round the nearest corner;
	// 2: the threshold's other side, the 8 corners
	for ( int ps = 0; ps < cellGiLoop8 - 5; ps++ ) {	// 3 passes; the bound a uniform: one body, never unrolled
		if ( ps == 1 && wOwn > 0.0 )
			continue;
		if ( ps == 2 && ( or( or( other < 0, tAcc <= 0.0 ), wOwn <= 0.0 ) ) )
			break;
		int n = ps == 1 ? 3 * cellGiLoop8 + 3 : cellGiLoop8;	// 27, 8
		int2 R = ps == 2 ? (int2)( other ) : L;
		for ( int k = 0; k < n; k++ ) {
			int3 c;
			float w;
			if ( ps == 1 ) {
				c = r0 + int3( k % 3, ( k / 3 ) % 3, k / 9 ) - 1;
				float3 d = float3( c ) - g;
				w = 1.0 / ( 1.0 + dot( d, d ) );
			} else {
				int3 o = int3( k & 1, ( k >> 1 ) & 1, ( k >> 2 ) & 1 );
				c = i0 + o;
				float3 t = lerp( 1.0 - f, f, float3( o ) );
				w = t.x * t.y * t.z;
			}
			if ( or( or( w <= 0.0, any( (c < (int3)( 0 )) ) ), any( (c >= dm) ) ) )
				continue;
			float4 S4 = cellGiRooms ? cellGiSlots.Load( int4(c, 0) ) : (float4)( 0.0 );
			int2 S = cellRoomUnpack( S4.r );
			int q = ( anyRoom && ps < 2 ) ? 0
			      : ( S.x >= 0 && ( or( S.x == R.x, S.x == R.y ) ) ) ? 0 : ( S.y >= 0 && ( or( S.y == R.x, S.y == R.y ) ) ) ? 1 : -1;
			if ( q < 0 )
				continue;
			int bq = q * 6 * dm.z;
			float4 u = n2.x * cellGi.Load( int4(int3( c.xy, c.z + bq + sl.x ), 0) )
			       + n2.y * cellGi.Load( int4(int3( c.xy, c.z + bq + sl.y ), 0) )
			       + n2.z * cellGi.Load( int4(int3( c.xy, c.z + bq + sl.z ), 0) );
			if ( u.a <= 0.0 )
				continue;	// an empty slot (no probe of its volume reached the voxel): no weight
			int see = wall ? cellGiWall( from, c, ps == 1 ) : 2;
			if ( see == 0 )
				continue;
			if ( see == 1 ) {
				bur += w * u;
				wBur += w;
				continue;
			}
			if ( ps == 2 ) {
				oth += w * u;
				wOth += w;
				continue;
			}
			own += w * u;
			wOwn += w;
			nearPick = or( nearPick, ps == 1 );
			if ( ps == 0 && !anyRoom ) {	// the threshold this slot sits by: its weight, and the room across it
				float t = q == 0 ? S4.b : S4.a;
				int2 O = cellRoomUnpack( S4.g );
				int oq = q == 0 ? O.x : O.y;
				tAcc += w * t;
				if ( oq >= 0 && t > tBest ) {
					tBest = t;
					other = oq;
				}
			}
		}
	}
	if ( wOwn <= 0.0 && wBur > 0.0 ) {	// pass 1 found only buried voxels (tAcc is 0: no pass 2)
		own = bur;
		wOwn = wBur;
		nearPick = true;
	}
	float4 s = wOwn > 0.0 ? own / wOwn : (float4)( 0.0 );
	if ( wOth > 0.0 )
		s = lerp( s, oth / wOth, 0.5 * clamp( tAcc / wOwn, 0.0, 1.0 ) );
	cellGiPath = anyRoom ? 4 : ( nearPick ? 2 : 1 ) + ( strand ? 5 : 0 );
	bool lone = strand && wOwn > 0.0 && cellRoomSolid( P + N * ( 2.75 * cellRoomsCell ) );
	if ( or( wOwn <= 0.0, lone ) ) {	// one trilinear site: each cellGiSample inlines it
		const float3 Y = float3( 0.2126, 0.7152, 0.0722 );
		float4 tr = cellGiTrilinear( P, N );
		if ( or( wOwn <= 0.0, ( tr.a > 0.0 && dot( tr.rgb, Y ) / tr.a < dot( s.rgb, Y ) / max( s.a, 1e-6 ) ) ) ) {
			cellGiPath = strand && wOwn <= 0.0 ? 5 : 4;
			return tr;
		}
	}
	// lane GIEDGE1: the voxels' answer fades over the pad as the trilinear does (no lit skin a voxel past the face)
	float fe = cellGiEdgeFade( g.xy + 0.5 );
	return fe < 1.0 ? s * fe : s;
}

#ifdef WW_CELL_RTDDGI
#undef cellGiVolume
/* lane RTXGI1 M2 (src/features/gi/rtddgicell.h): the ray-traced GI volume's read (cell_ddgi.glsl) in place of the baked grid's
 * inside its probe box; the grid's read (cellGiVolumeGrid above, unchanged) past the box, across its last cell (the
 * fade), and wherever none of the 8 probes is usable. Its .a is 1: the volume holds the sky (its misses) and every
 * bounce, so it replaces the flat ambient whole (cellGiSkyK's k; the linear law of ww-probe-sky-shade). The Sky
 * visibility pass (cellPass 2) shows the volume's sky visibility (the irradiance tiles' alpha). Probe 95: path 12. One
 * read site and one grid site: every cellGiSample inlines this (the program's size). */
#include "../gi/cell_ddgi.hlsli"
float4 cellGiVolume( float3 P, float3 N, int2 L )
{
	CellDdgiOut d;
	d.E = (float3)( 0.0 );
	d.vis = 0.0;
	d.w = 0.0;
	d.fade = 0.0;
	if ( cellDdgiOn )
		d = cellDdgiRead( P, N, normalize( float3( cellRow[0].w, cellRow[1].w, cellRow[2].w ) - P ), N );
	bool own = d.w > 0.0 && d.fade > 0.0;
	float4 g = (float4)( 0.0 );
	if ( or( !own, d.fade < 1.0 ) )
		g = cellGiVolumeGrid( P, N, L );
	if ( !own )
		return g;
	float4 r = cellPass == 2 ? float4( (float3)( d.vis ), 1.0 ) : float4( d.E, 1.0 );
	cellGiPath = 12;
	return d.fade < 1.0 ? lerp( g, r, d.fade ) : r;
}
#endif

float4 cellGiSample( float3 P, float3 N )
{
	float4 s;
	float m = 0.0;
	if ( cellGiHalfOn && cellPass == 0 ) {
		if ( cellProbe == 0 && cellGiHalfAt( P, N, s ) ) {
			cellGiPath = 11;
			return s;
		}
		if ( cellProbe != 0 )	// a probe picture: cellGiHalf holds the volumes' mask (src/gl/celllights.cpp)
			m = cellGiHalf.Load( int3(int2( gl_FragCoord.xy - cellGiHalfRect.xy ), 0) ).r;
	}
	return cellGiVolume( P, N, m > 0.5 ? int2( int( m + 0.5 ) - 1, -1 ) : cellGiVolAt( P, N ) );
}
#endif
#else
/* lane PRTOFF1: no baked grid. The surfaces' GI is the ray-traced volume's read alone (RTXGI1 M2's cellGiVolume with no
 * grid under it): inside its box E with .a 1 (the volume holds the sky and every bounce, so it replaces the flat ambient
 * whole); across its last cell E and .a fade together, so past the box the ambient applies alone (.a 0). The Sky
 * visibility pass (cellPass 2): the volume's sky visibility. Probe 95: path 12. Without the volume: nothing. */
#ifdef WW_CELL_RTDDGI
#include "../gi/cell_ddgi.hlsli"
#include "../ao/cell_gihalfat.hlsli"
#endif
/* the uniform (true on a weather-lit exterior, src/gl/celllights.cpp) counts only while the volume is read, so the sky's
 * branches below and in the programs (the PBR ambient, cellGiSkyK) stay the no-GI ones before the volume is up or with
 * it off. Defined here, after cellDdgiOn: the grid's helpers above never run without the PRT. */
#define cellGiSky ( cellGiSky && CELL_GI_UP )
#ifdef WW_CELL_RTDDGI
/* FIX1101A (manager hold 1 part C, c3 still_close5: the opaque pass 12.4 ms GI off, 54.7 one volume, 110.9 cascades): one
 * read a fragment. A program asks for the sample at the same (P, N) two or three times (cellGiSkyK, cellLit's cellGiE,
 * the PBR ambient); the first call keeps its answer and a later call at the same point returns it, bit for bit (same
 * inputs, same value). Red WW_CELL_RTDDGI_RED=slowread (cellDdgiRed & 16): every call reads (today's main) */
static bool cellGiMemoOk = false;
static float3 cellGiMemoP = (float3)( 0.0 );
static float3 cellGiMemoN = (float3)( 0.0 );
static float4 cellGiMemoS = (float4)( 0.0 );
static float2 cellGiMemoV = float2( 1.0, 0.0 );
/* GIFIX1101 HO: the last cellGiSample's sky visibility at the normal and its fade (1, 0 = no volume read): cellSsgiE's
 * hand-over takes the volume's own occlusion out of the screen's */
static float2 cellGiVisN = float2( 1.0, 0.0 );
/* DDGIREADPERF1 (Museum Medium: opaque 102 ms, 12 with the volume off; the read itself < 1 ms): a program that inlines the
 * read at every cellGiSample site (fo4_cell: 7) runs its light loop 1.8x slower with no read executed (offline bench,
 * 300 lights: 40 -> 72 ms). A program that defines CELL_GI_ONEREAD (fo4_default.frag, pbrm_default.frag) samples once in
 * main and sets cellGiOneRead = true after it, unconditionally: the later sites (all at the same P, N) then
 * compile to the memo alone. Red WW_CELL_RTDDGI_ONEREAD=0 (the loader's WW_CELL_GI_ONEREAD_RED): a read at every site */
#if defined( CELL_GI_ONEREAD ) && !defined( WW_CELL_GI_ONEREAD_RED )
#define CELL_GI_ONE 1
#endif
static bool cellGiOneRead = false;
static float2 cellGiSpecV = float2( 1.0, 0.0 );	// cellGiFirst's sky visibility on cellSpecSky's lobe (vis2) and its fade (0: no own sky)
/* DDGIREADPERF2 (fo4_cell 50 ms offline vs 40 with the volume off, the read already one site): every cellGiSample site
 * and cellSpecSky still inlined the screen texel's upsample (cellGiHalfAt). Under CELL_GI_ONE cellGiFirst takes the
 * texel once too and leaves it in the memo and cellGiSpecV, the values the sites computed (same call, same P and N).
 * Red WW_CELL_RTDDGI_ONEHALF=0 (the loader's WW_CELL_GI_ONEHALF_RED): the upsample at every site */
#if defined( CELL_GI_ONE ) && !defined( WW_CELL_GI_ONEHALF_RED )
#define CELL_GI_HALF1 1
#endif
#endif
#if defined( WW_CELL_RESTIRGI ) && defined( WW_CELL_RTDDGI )
#define cellGiSample cellGiSampleVol	// RTEMIS1 part 2: the volume's read under its own name; the wrapper below takes cellGiSample
#endif
float4 cellGiSample( float3 P, float3 N )
{
#ifdef WW_CELL_RTDDGI
	if ( !cellDdgiOn )
		return (float4)( 0.0 );
	/* PERFFIX2 (PERFRES1101 A gap 2): the volume read once a screen texel (cell_gihalf.frag, giHalfPass) and this
	 * surface upsampled from the texels that are its own (cellGiHalfAt: plane + normal keys); a miss (an edge, a
	 * blended draw, a probe picture) reads the volume itself below. Red WW_CELL_GI_RATE=inline: no pass. */
#ifndef CELL_GI_HALF1
	float4 hs;
	if ( cellGiHalfOn && cellPass == 0 && cellProbe == 0 && cellGiHalfAt( P, N, hs ) ) {
		if ( hs.a > 0.0 )
			cellGiPath = 12;
		cellGiVisN = hs.a > 0.0 ? float2( cellGiHalfVis.x, hs.a ) : float2( 1.0, 0.0 );
		return hs;
	}
#endif
	if ( or( cellGiOneRead, ( cellGiMemoOk && ( cellDdgiRed & 16 ) == 0 && all(P == cellGiMemoP) && all(N == cellGiMemoN) ) ) ) {
		if ( cellGiMemoS.a > 0.0 )
			cellGiPath = 12;
		cellGiVisN = cellGiMemoV;
		return cellGiMemoS;
	}
	CellDdgiOut d = cellDdgiRead( P, N, normalize( float3( cellRow[0].w, cellRow[1].w, cellRow[2].w ) - P ), N );
	float4 s = (float4)( 0.0 );
	if ( d.w > 0.0 && d.fade > 0.0 ) {
		cellGiPath = 12;
		s = d.fade * ( cellPass == 2 ? float4( (float3)( d.vis ), 1.0 ) : float4( d.E, 1.0 ) );
	}
	cellGiMemoOk = true;
	cellGiMemoP = P;
	cellGiMemoN = N;
	cellGiMemoS = s;
	cellGiMemoV = s.a > 0.0 ? float2( d.vis, d.fade ) : float2( 1.0, 0.0 );
	cellGiVisN = cellGiMemoV;
	return s;
#else
	return (float4)( 0.0 );
#endif
}
#if defined( WW_CELL_RESTIRGI ) && defined( WW_CELL_RTDDGI )
#undef cellGiSample

Buffer<float4> cellRestirGi : register(t14, space0);	// RGBA32F, bottom row first: E rgb, linear depth
float4 cellGiSample( float3 P, float3 N )
{
	float4 s = cellGiSampleVol( P, N );
	if ( or( or( or( !cellRestirGiOn, cellPass != 0 ), cellProbe != 0 ), !( s.a > 0.0 ) ) )
		return s;
	int2 q = int2( gl_FragCoord.xy );
	int W = int( cellRestirGiSize.x + 0.5 ), H = int( cellRestirGiSize.y + 0.5 );
	if ( or( or( or( q.x < 0, q.y < 0 ), q.x >= W ), q.y >= H ) )
		return s;
	float4 e = cellRestirGi.Load( q.y * W + q.x );
	float3 w = P - float3( cellRow[0].w, cellRow[1].w, cellRow[2].w );
	float s2 = 1.0 / max( dot( cellRow[0].xyz, cellRow[0].xyz ), 1e-12 );
	float D = -s2 * ( cellRow[0].z * w.x + cellRow[1].z * w.y + cellRow[2].z * w.z );
	return e.w > 0.0 && abs( D - e.w ) <= 0.02 * e.w + 2.0 ? float4( e.rgb * s.a, s.a ) : s;
}
#endif
#endif


Texture2D<float4> cellAoDecal : register(t15, space0); SamplerState _cellAoDecal_sampler : register(s15, space0);
float cellAoDecalAt()
{
	return cellAoDecal.Load( int3(int2( gl_FragCoord.xy - cellAoDecalRect.xy ), 0) ).r;
}

/* lane EMISSIVEGRID1 (src/features/gi/probegifine.h): inside a fine box the coarse grid's share of the box's glow (C, kC of it in
 * the sample) gives way to the box's own (D), faded back to the coarse grid over the box's outer two voxels. A fine
 * voxel counts only when its centre is air and the fine room grid shows no solid cell between the surface's room
 * read and it (cellGiSeen's rule). One call site (cellGiE0): the program link limit (GIDARK1). */
float3 cellGiFineAdd( float3 P, float3 N, float3 base, float kC )
{
	int b = -1;
	float3 g = (float3)( 0.0 );
	for ( int k = 0; k < cellGiFineN; k++ ) {
		float3 gk = ( P + N * ( 0.5 * cellGiFineBox[k].w ) - cellGiFineBox[k].xyz ) / cellGiFineBox[k].w - 0.5;
		if ( all( (gk >= (float3)( 0.0 )) ) && all( (gk < (cellGiFineDim[k].xyz - 1.0)) ) ) {
			b = k;
			g = gk;
			break;
		}
	}
	if ( b < 0 )
		return base;
	float4 o = cellGiFineBox[b];
	float4 dm = cellGiFineDim[b];
	int3 i0 = int3( floor( g ) );
	float3 f = g - float3( i0 );
	int3 sl = int3( N.x >= 0.0 ? 0 : 1, N.y >= 0.0 ? 2 : 3, N.z >= 0.0 ? 4 : 5 ) * int( dm.z ) + int( dm.w );
	int cOff = 6 * int( dm.z );
	float3 n2 = N * N;
	bool see = cellGiRooms && ( cellGiFineRed & 1 ) == 0;
	float3 from = P + N * ( 0.75 * cellRoomsCell );
	if ( see && cellRoomSolid( from ) ) {
		from = P + N * ( 1.75 * cellRoomsCell );
		see = !cellRoomSolid( from );
	}
	float3 D = (float3)( 0.0 ), C = (float3)( 0.0 );
	float ws = 0.0;
	for ( int k = 0; k < cellGiLoop8; k++ ) {
		int3 oo = int3( k & 1, ( k >> 1 ) & 1, ( k >> 2 ) & 1 );
		int3 c = i0 + oo;
		float3 t = lerp( 1.0 - f, f, float3( oo ) );
		float w = t.x * t.y * t.z;
		if ( w <= 0.0 )
			continue;
		float4 d = n2.x * cellGi.Load( int4(int3( c.xy, c.z + sl.x ), 0) )
		       + n2.y * cellGi.Load( int4(int3( c.xy, c.z + sl.y ), 0) )
		       + n2.z * cellGi.Load( int4(int3( c.xy, c.z + sl.z ), 0) );
		if ( d.a <= 0.0 )
			continue;	// solid at its centre
		if ( see ) {
			float3 dv = o.xyz + ( float3( c ) + 0.5 ) * o.w - from;
			int nt = clamp( int( ceil( length( dv ) / ( 0.5 * cellRoomsCell ) ) ), 1, 8 );
			bool hit = false;
			for ( int j = 1; j <= nt && !hit; j++ )
				hit = cellRoomSolid( from + dv * ( float( j ) / float( nt + 1 ) ) );
			if ( hit )
				continue;
		}
		float4 e = n2.x * cellGi.Load( int4(int3( c.xy, c.z + sl.x + cOff ), 0) )
		       + n2.y * cellGi.Load( int4(int3( c.xy, c.z + sl.y + cOff ), 0) )
		       + n2.z * cellGi.Load( int4(int3( c.xy, c.z + sl.z + cOff ), 0) );
		D += w * d.rgb;
		C += w * e.rgb;
		ws += w;
	}
	if ( ws <= 0.0 )
		return base;
	float3 edge = min( g, dm.xyz - 1.0 - g );
	float fade = clamp( min( edge.x, min( edge.y, edge.z ) ) / 2.0, 0.0, 1.0 );
	float3 sub = ( cellGiFineRed & 2 ) != 0 ? (float3)( 0.0 ) : kC * C / ws;
	return max( base + fade * ( D / ws - sub ), (float3)( 0.0 ) );
}

float3 cellGiE0( float3 P, float3 N )
{
	float4 s = cellGiSample( P, N );
#ifdef WW_CELL_PRT
	if ( cellGiFineOn )	// lane EMISSIVEGRID1: the sample as below, then the fine boxes
		return cellGiFineAdd( P, N, cellGiSky ? max( s.rgb, (float3)( 0.0 ) ) : s.a > 0.01 ? max( s.rgb / s.a, (float3)( 0.0 ) ) : (float3)( 0.0 ),
			cellGiSky ? clamp( s.a, 0.0, 1.0 ) : s.a > 0.01 ? 1.0 : 0.0 );
#endif
	// with the sky in the grid the sample keeps its valid share (E x share), the other
	// ( 1 - share ) being the weather's own ambient the program leaves in (cellGiSkyK)
	if ( cellGiSky )
		return max( s.rgb, (float3)( 0.0 ) );
	return s.a > 0.01 ? max( s.rgb / s.a, (float3)( 0.0 ) ) : (float3)( 0.0 );
}

float3 cellGiE( float3 P, float3 N )
{
	float3 e = cellGiE0( P, N );
#ifndef WW_CELL_PRT
	return e;	// lane PRTOFF1: the AO decals are the PRT relight's
#else
	if ( !( cellAoDecalOn && cellPass < 2 ) )
		return e;
	float d = cellAoDecalAt();
	// outdoors the decal is blocked sky; the Sky visibility row opens it as the relight opens the grid's
	if ( cellGiSky && cellSkyVis < 1.0 )
		d = 1.0 + cellSkyVis * ( d - 1.0 );
	return e * d;
#endif
}
// the share of the weather's ambient the grid's sky replaces at P (0: not a sky grid)
float cellGiSkyK( float3 P, float3 N )
{
	if ( or( or( !CELL_GI_UP, !cellGiSky ), cellInterior ) )
		return 0.0;
	return clamp( cellGiSample( P, N ).a, 0.0, 1.0 );
}
float cellSpecSky( float3 P, float3 N, float3 R, float rough )
{
	if ( or( or( or( cellSpecOcc == 0, !CELL_GI_UP ), !cellGiSky ), cellInterior ) )
		return 1.0;
	float r = clamp( rough, 0.0, 1.0 );
	float3 D = cellSpecOcc == 2 ? N : normalize( lerp( N, R, ( 1.0 - r ) * ( sqrt( 1.0 - r ) + r ) ) );
#ifdef WW_CELL_PRT
	float3 t = ( P + N * ( 0.5 * cellGiVoxel ) - cellGiOrigin ) / cellGiVoxel;	// texels: voxel i's centre is i + 0.5
	float z = clamp( t.z, 0.5, cellGiDims.z - 0.5 );	// the slabs stack on z: never filter across one
	float3 sl = float3( D.x >= 0.0 ? 0.0 : 1.0, D.y >= 0.0 ? 2.0 : 3.0, D.z >= 0.0 ? 4.0 : 5.0 );
	float3 w = D * D;
	float depth = float( textureSize( cellGi, 0 ).z );
	float2 a = (float2)( 0.0 );
	for ( int k = 0; k < 3; k++ ) {
		float4 s = cellGi.Sample( _cellGi_sampler, float3( t.xy / cellGiDims.xy, ( ( cellSpecSkyBase + sl[k] ) * cellGiDims.z + z ) / depth ) );
		a += w[k] * s.ra;
	}
#if defined( WW_CELL_RTDDGI ) && !defined( WW_CELL_GI_MARCH )
	if ( cellDdgiOn ) {	// lane RTXGI1 M2: the volume's sky visibility on the lobe's direction (the grid's past its box)
		CellDdgiOut dv = cellDdgiRead( P, N, normalize( float3( cellRow[0].w, cellRow[1].w, cellRow[2].w ) - P ), D );
		if ( dv.w > 0.0 && dv.fade > 0.0 )
			a = float2( lerp( a.y < 0.01 ? 1.0 : clamp( a.x / a.y, 0.0, 1.0 ), dv.vis, dv.fade ), 1.0 );
	}
#endif
	if ( a.y < 0.01 )
		return 1.0;
	float f = cellGiEdgeFade( t.xy );	// lane GIEDGE1: no occlusion read off the clamped edge column
	float v = 1.0 + ( f < 1.0 ? f * cellSkyVis : cellSkyVis ) * ( clamp( a.x / a.y, 0.0, 1.0 ) - 1.0 );
#elif defined( WW_CELL_RTDDGI )
	// lane PRTOFF1: the volume's sky visibility on the lobe's direction (RTXGI1 M2's read with no grid under it: 1 past the box)
	/* FIX1101A (the read's cost): one read at the normal also takes the lobe's visibility with the same weights (vis2),
	 * and leaves its answer for cellGiSample at this (P, N) (the memo): the surfaces' sample costs no second read. Red
	 * WW_CELL_RTDDGI_RED=slowread: the lobe's own read, as before */
	/* PERFFIX2: with the screen pass, its texel (cellGiHalfAt) holds the sky visibility at the normal, half way and at
	 * the mirror direction; the lobe's is the three-point read at D's own bend (cellGiHalfLobeVis): no read of its own */
#ifndef CELL_GI_HALF1
	float4 hs;
	if ( cellGiHalfOn && cellPass == 0 && cellProbe == 0 && cellGiHalfAt( P, N, hs ) ) {
		if ( hs.a <= 0.0 )
			return 1.0;
		float vl = cellSpecOcc == 2 ? cellGiHalfVis.x : cellGiHalfLobe( ( 1.0 - r ) * ( sqrt( 1.0 - r ) + r ), N, R );	// GIFIX1101 SO
		float vh = 1.0 + hs.a * cellSkyVis * ( clamp( vl, 0.0, 1.0 ) - 1.0 );
		return cellSpecOcc == 2 ? lerp( 1.0, vh, r ) : vh;
	}
#endif
#ifdef CELL_GI_ONE
	if ( cellGiSpecV.y <= 0.0 )	// DDGIREADPERF1: cellGiFirst's read took the lobe too
		return 1.0;
	float v = 1.0 + cellGiSpecV.y * cellSkyVis * ( clamp( cellGiSpecV.x, 0.0, 1.0 ) - 1.0 );
#else
	float3 oSky = normalize( float3( cellRow[0].w, cellRow[1].w, cellRow[2].w ) - P );
	bool fast = ( cellDdgiRed & 16 ) == 0;
	cellDdgiTwo = fast;
	cellDdgiD2 = D;
	CellDdgiOut dv = cellDdgiRead( P, N, oSky, fast ? N : D );
	cellDdgiTwo = false;
	bool ownSky = dv.w > 0.0 && dv.fade > 0.0;
	if ( fast ) {
		cellGiMemoOk = true;
		cellGiMemoP = P;
		cellGiMemoN = N;
		cellGiMemoS = ownSky ? dv.fade * ( cellPass == 2 ? float4( (float3)( dv.vis ), 1.0 ) : float4( dv.E, 1.0 ) ) : (float4)( 0.0 );
		cellGiMemoV = ownSky ? float2( dv.vis, dv.fade ) : float2( 1.0, 0.0 );
	}
	if ( !ownSky )
		return 1.0;
	float v = 1.0 + dv.fade * cellSkyVis * ( clamp( fast ? dv.vis2 : dv.vis, 0.0, 1.0 ) - 1.0 );
#endif
#else
	float v = 1.0;
#endif
	return cellSpecOcc == 2 ? lerp( 1.0, v, r ) : v;
}
#ifdef CELL_GI_ONE
/* DDGIREADPERF1: the program's one volume read, called in main before cellSpecSky and every cellGiSample. Without a
 * screen texel it reads once at (P, Ns) and, where cellSpecSky will want it, the lobe's visibility with the same weights
 * (FIX1101A's two-direction read, the one cellSpecSky made). cellGiSample's memo and cellSpecSky's answer come from it,
 * bit for bit the values the old sites read (same call, same arguments). Ns: the sample's normal (N, or a pass red's up).
 * The slowread red (cellDdgiRed 16) needs WW_CELL_RTDDGI_ONEREAD=0 as well: here every site takes this read. */
void cellGiFirst( float3 P, float3 N, float3 Ns, float3 R, float rough )
{
	if ( !cellDdgiOn )
		return;
	float4 hs;
	if ( cellGiHalfOn && cellPass == 0 && cellProbe == 0 && cellGiHalfAt( P, Ns, hs ) ) {
#ifdef CELL_GI_HALF1
		// DDGIREADPERF2: the texel for every site (cellPass 0 here, so Ns is N): cellGiSample's and cellSpecSky's answers
		float r = clamp( rough, 0.0, 1.0 );
		bool own = hs.a > 0.0;
		cellGiMemoOk = true;
		cellGiMemoP = P;
		cellGiMemoN = Ns;
		cellGiMemoS = hs;
		cellGiMemoV = own ? float2( cellGiHalfVis.x, hs.a ) : float2( 1.0, 0.0 );
		if ( own && cellSpecOcc != 0 && cellGiSky && !cellInterior )
			cellGiSpecV = float2( cellSpecOcc == 2 ? cellGiHalfVis.x : cellGiHalfLobe( ( 1.0 - r ) * ( sqrt( 1.0 - r ) + r ), N, R ), hs.a );
		else
			cellGiSpecV = float2( 1.0, 0.0 );
#endif
		return;	// without CELL_GI_HALF1 every site takes the screen texel itself
	}
	float r = clamp( rough, 0.0, 1.0 );
	cellDdgiTwo = cellSpecOcc != 0 && cellGiSky && !cellInterior;
	cellDdgiD2 = cellSpecOcc == 2 ? N : normalize( lerp( N, R, ( 1.0 - r ) * ( sqrt( 1.0 - r ) + r ) ) );
	CellDdgiOut d = cellDdgiRead( P, Ns, normalize( float3( cellRow[0].w, cellRow[1].w, cellRow[2].w ) - P ), Ns );
	cellDdgiTwo = false;
	bool own = d.w > 0.0 && d.fade > 0.0;
	cellGiMemoOk = true;
	cellGiMemoP = P;
	cellGiMemoN = Ns;
	cellGiMemoS = own ? d.fade * ( cellPass == 2 ? float4( (float3)( d.vis ), 1.0 ) : float4( d.E, 1.0 ) ) : (float4)( 0.0 );
	cellGiMemoV = own ? float2( d.vis, d.fade ) : float2( 1.0, 0.0 );
	cellGiSpecV = own ? float2( d.vis2, d.fade ) : float2( 1.0, 0.0 );
}
#endif

/* probe 90 indoors: the share of the cell's ambient (DALC) the probe light replaced at P under cellAmbGi's rule:
 * the grid's valid share with replace (the default), 0 with keep or without the GI */
float cellAmbShare( float3 P, float3 N )
{
	if ( !cellInterior )
		return cellGiSkyK( P, N );
	if ( or( or( !CELL_GI_UP, !cellHasDalc ), cellGiAmb != 1 ) )
		return 0.0;
	return clamp( cellGiSample( P, N ).a, 0.0, 1.0 );
}
float3 cellGobo( int i, float3 D, float3 axis, bool spot )
{
	if ( !cellGoboOn )
		return (float3)( 1.0 );
	int h;
	float2 uv;
	int hf = -1;	// GOBO2: the half of a non-spot pattern (0 top, 1 bottom)
	if ( spot ) {
		float4 t5 = cellLights.Load( i * CELL_TPL + 5 );
		if ( t5.w < 0.5 )
			return (float3)( 1.0 );
		float4 t6 = cellLights.Load( i * CELL_TPL + 6 );
		h = int( t5.w + 0.5 ) - 1;
		uv = float2( dot( D, t6.xyz ), dot( D, t5.xyz ) ) / ( 2.0 * max( dot( D, axis ), 1e-3 ) * max( t6.w, 1e-4 ) ) + 0.5;
	} else {
		if ( cellGoboTbl <= 0 )
			return (float3)( 1.0 );
		int r = int( cellLights.Load( cellGoboTbl + i / 4 )[i % 4] + 0.5 ) - 1;
		if ( r < 0 )
			return (float3)( 1.0 );
		float4 a0 = cellLights.Load( r );
		h = int( a0.w + 0.5 ) - 1;
		float3 l = ( cellGoboMap & 4 ) != 0 ? D : float3( dot( D, a0.xyz ), dot( D, cellLights.Load( r + 1 ).xyz ),
		                                              dot( D, cellLights.Load( r + 2 ).xyz ) );
		if ( ( cellGoboMap & 8 ) != 0 ) {	// GOBO2's guess (red)
			float3 d = normalize( ( cellGoboMap & 2 ) != 0 ? l.yzx : l );
			hf = ( d.z >= 0.0 ) == ( ( cellGoboMap & 1 ) == 0 ) ? 0 : 1;
			uv = d.xy / ( 1.0 + abs( d.z ) ) * 0.5 + 0.5;
		} else {
			float3 d = normalize( l );
			hf = l.x < -1.0 ? 1 : 0;
			float2 e = ( cellGoboMap & 2 ) != 0 ? d.yz : d.zy;
			if ( hf == 1 && ( cellGoboMap & 1 ) == 0 )
				e.x = -e.x;
			uv = e / max( hf == 1 ? 1.0 - d.x : 1.0 + d.x, 1e-4 ) * 0.5 + 0.5;
		}
	}
	if ( cellGoboFlat )
		return cellGoboGam ? pow( cellLights.Load( h + 1 ).rgb, (float3)( 2.2 ) ) : cellLights.Load( h + 1 ).rgb;
	if ( ( cellGoboRot & 4 ) != 0 )
		uv = uv.yx;
	if ( ( cellGoboRot & 1 ) != 0 )
		uv.x = 1.0 - uv.x;
	if ( ( cellGoboRot & 2 ) != 0 )
		uv.y = 1.0 - uv.y;
	int2 sz = int2( cellLights.Load( h ).xy + 0.5 );
	int2 pz = hf < 0 ? sz : int2( sz.x, sz.y / 2 );	// GOBO2: one half, no bleed into the other
	float2 p = clamp( uv, 0.0, 1.0 ) * float2( pz ) - 0.5;
	int2 q = int2( floor( p ) );
	float2 f = p - float2( q );
	int2 q0 = clamp( q, (int2)( 0 ), pz - 1 ), q1 = clamp( q + 1, (int2)( 0 ), pz - 1 );
	int b = h + 2 + max( hf, 0 ) * pz.y * sz.x;
	float3 c0 = lerp( cellLights.Load( b + q0.y * sz.x + q0.x ).rgb, cellLights.Load( b + q0.y * sz.x + q1.x ).rgb, f.x );
	float3 c1 = lerp( cellLights.Load( b + q1.y * sz.x + q0.x ).rgb, cellLights.Load( b + q1.y * sz.x + q1.x ).rgb, f.x );
	return cellGoboGam ? pow( lerp( c0, c1, f.y ), (float3)( 2.2 ) ) : lerp( c0, c1, f.y );
}

/* the game draws a hemisphere or box light as an omni light clipped by its volume (celllights.h):
 * true when P lies inside light i's. shape = texel 1.w (-3 hemisphere, -4 box; anything else has none). */
bool cellShapeIn( int i, float3 P, float3 Lpos, float shape )
{
	if ( shape > -2.5 )
		return true;
	if ( shape > -3.5 )
		return dot( P - Lpos, cellLights.Load( i * CELL_TPL + 2 ).xyz ) >= 0.0;
	float4 b0 = cellLights.Load( i * CELL_TPL + 5 );
	float4 b1 = cellLights.Load( i * CELL_TPL + 6 );
	float4 b2 = cellLights.Load( i * CELL_TPL + 7 );
	float3 k = float3( dot( b0.xyz, P ) + b0.w, dot( b1.xyz, P ) + b1.w, dot( b2.xyz, P ) + b2.w );
	return all( (abs( k ) <= (float3)( 1.0 )) );
}

/* light i at world point P, normal N: its colour x the radial curve x the spot cone (no N.L), and
 * the direction to it; zero when out of reach or behind the surface. The PBR path's per-light term. */
float3 cellLightE( int i, float3 P, float3 N, out float3 L, out bool noSpec )
{
	float4 t0 = cellLights.Load( i * CELL_TPL );
	float3 Lv = t0.xyz - P;
	float d = length( Lv );
	L = Lv / max( d, 0.001 );
	noSpec = true;
	if ( or( d >= t0.w, dot( N, L ) <= 0.0 ) )
		return (float3)( 0.0 );
	float4 t1 = cellLights.Load( i * CELL_TPL + 1 );
	if ( !cellShapeIn( i, P, t0.xyz, t1.w ) )
		return (float3)( 0.0 );
	float4 t3 = cellLights.Load( i * CELL_TPL + 3 );
	float a = cellRadial( d, t0.w, t3.xyz );
	float3 g = (float3)( 1.0 );	// lane LIGHTS5 (GOBO1)
	float3 gAx = (float3)( 0.0 );
	if ( t1.w > -1.5 ) {
		float4 t2 = cellLights.Load( i * CELL_TPL + 2 );
		float base = clamp( 1.0 - ( 1.0 - dot( -L, t2.xyz ) ) / max( 1.0 - t1.w, 1e-4 ), 0.0, 1.0 );
		a *= min( pow( base, max( t2.w, 1e-3 ) ), 1.0 );
		gAx = t2.xyz;
	}
	if ( a > 0.0 )
		g = cellGobo( i, -Lv, gAx, t1.w > -1.5 );	// one call for every shape
	if ( a > 0.0 )
		a *= cellShadowF( i, P, N );
	noSpec = ( int( t3.w + 0.5 ) & 1 ) != 0;
	return t1.rgb * g * a;
}

// lane LIGHTPHYS1 Area lights: the specular direction to a sphere's closest point to the reflected ray, its widened roughness
void cellAreaSpec( int i, float3 P, float3 N, float3 V, inout float3 Ls, inout float rS )
{
	float4 t0 = cellLights.Load( i * CELL_TPL );
	float3 Lv = t0.xyz - P;
	float d = length( Lv );
	float R = clamp( min( cellLightPhysB.y, t0.w * cellLightPhys.z ), 2.0, 32.0 );
	float3 r = reflect( -V, N );
	float3 c = dot( Lv, r ) * r - Lv;
	float3 p = Lv + c * clamp( R / max( length( c ), 1e-4 ), 0.0, 1.0 );
	Ls = normalize( p );
	if ( ( int( cellLightPhysB.z + 0.5 ) & 4 ) == 0 )
		rS = sqrt( clamp( min( max( rS * rS, 1e-4 ) + R / max( 2.0 * d, 1e-4 ), 0.9999 ), 0.0, 1.0 ) );
}

/* the far blend at world point P, 0 near, 1 far (debug noblend: a hard switch at the band's middle) */
float cellFarW( float3 P )
{
	if ( !cellFarOn )
		return 0.0;
	float d = length( P - float3( cellRow[0].w, cellRow[1].w, cellRow[2].w ) );
	if ( ( cellFarRed & 2 ) != 0 )
		return step( 0.5 * ( cellFarBand.x + cellFarBand.y ), d );
	return smoothstep( cellFarBand.x, cellFarBand.y, d );
}

// the hash farLightHash (src/features/far-lod/farlight.h) uses: an int's bits as uint, products wrap
uint cellFarHash( int3 k, int side )
{
	return ( uint( k.x ) * 73856093u ) ^ ( uint( k.y ) * 19349663u ) ^ ( uint( k.z ) * 83492791u ) ^ ( uint( side ) * 2654435761u );
}

static int cellFarIters = 0;	// probe 94: table slots walked
/* F: the lit surfels on P's side in the 27 cells round P's key cell, w = (1 - d^2 / R^2)^2, R = 1.5 cells
 * (must match the bake's sideOf and keyOf). Debug flipside: the opposite side's. */
float3 cellFarF( float3 P, float3 N )
{
	float3 a = abs( N );
	int ax = 0;
	if ( a.y > a.x )
		ax = 1;
	if ( a.z > a[ax] )
		ax = 2;
	int side = 2 * ax + ( N[ax] >= 0.0 ? 0 : 1 );
	float3 nudge = (float3)( 0.0 );
	nudge[ax] = ( side & 1 ) != 0 ? -0.5 : 0.5;
	int3 k0 = int3( floor( ( P - nudge ) / cellFarCell ) );
	if ( ( cellFarRed & 4 ) != 0 )
		side ^= 1;
	uint mask = ( 1u << uint( cellFarBits ) ) - 1u;
	float R2 = 2.25 * cellFarCell * cellFarCell;
	float3 s = (float3)( 0.0 );
	float ws = 0.0;
	for ( int dz = -1; dz <= 1; dz++ )
	for ( int dy = -1; dy <= 1; dy++ )
	for ( int dx = -1; dx <= 1; dx++ ) {
		int3 k = k0 + int3( dx, dy, dz );
		uint h = cellFarHash( k, side ) & mask;
		float3 key = float3( float( k.x ), float( k.y ), float( k.z * 8 + side ) );
		for ( int j = 0; j < cellFarProbe && j < 64; j++ ) {
			cellFarIters++;
			float4 t = cellFarSlots.Load( int( h ) );
			if ( t.w == 0.0 )
				break;
			if ( all(t.xyz == key) ) {
				int r = int( t.w ) - 1;
				float3 d = P - cellFarRecs.Load( r * 2 ).xyz;
				float q = 1.0 - dot( d, d ) / R2;
				if ( q > 0.0 ) {
					q *= q;
					s += q * cellFarRecs.Load( r * 2 + 1 ).rgb;
					ws += q;
				}
			}
			h = ( h + 1u ) & mask;
		}
	}
	return ws > 0.0 ? s / ws : (float3)( 0.0 );
}

// the GI grid's placed share at P (its E, read as cellGiE reads the grid: the sky grid's rgb carries its share)
float3 cellFarPlacedE( float3 P, float3 N )
{
	if ( !cellFarPlacedOn )
		return (float3)( 0.0 );
	float3 g = ( P + N * ( 0.5 * cellFarPlacedVoxel ) - cellFarPlacedOrigin ) / cellFarPlacedVoxel;
	float2 xy = g.xy / cellFarPlacedDims.xy;
	float z = clamp( g.z, 0.5, cellFarPlacedDims.z - 0.5 );
	float depth = 6.0 * cellFarPlacedDims.z;
	float3 n2 = N * N;
	float4 s = n2.x * cellFarPlaced.Sample( _cellFarPlaced_sampler, float3( xy, ( z + ( N.x >= 0.0 ? 0.0 : 1.0 ) * cellFarPlacedDims.z ) / depth ) )
	       + n2.y * cellFarPlaced.Sample( _cellFarPlaced_sampler, float3( xy, ( z + ( N.y >= 0.0 ? 2.0 : 3.0 ) * cellFarPlacedDims.z ) / depth ) )
	       + n2.z * cellFarPlaced.Sample( _cellFarPlaced_sampler, float3( xy, ( z + ( N.z >= 0.0 ? 4.0 : 5.0 ) * cellFarPlacedDims.z ) / depth ) );
	if ( cellGiSky )
		return max( s.rgb, (float3)( 0.0 ) );
	return s.a > 0.01 ? max( s.rgb / s.a, (float3)( 0.0 ) ) : (float3)( 0.0 );
}

/* the far terms at P: returns w; F (debug nosurfel: 0) and the GI's placed share / pi (gP, the caller takes w x gP
 * out of its GI). The caller's placed diffuse: (1 - w) x real (cellSumLights fades it) + w x F. */
float cellFarTerms( float3 P, float3 N, out float3 F, out float3 gP )
{
	F = (float3)( 0.0 );
	gP = (float3)( 0.0 );
	float w = cellFarW( P );
	if ( w <= 0.0 )
		return 0.0;
	if ( ( cellFarRed & 1 ) == 0 )
		F = cellFarF( P, N );
	gP = cellFarPlacedE( P, N ) * 0.31830989;
	return w;
}

/* debug probes, sqrt( clamp( E / 4 ) ) so a dim far town keeps its bits: 91 F, 92 the real placed
 * irradiance (Lambert, unfaded) + the GI's placed share / pi, 93 w, 94 (lights the shading loops + table slots
 * walked) / 256 */
float3 cellFarProbeOut( float3 P, float3 N )
{
	float w = cellFarW( P );
	if ( cellProbe == 93 )
		return (float3)( w );
	if ( cellProbe == 91 )
		return sqrt( clamp( ( cellFarOn ? cellFarF( P, N ) : (float3)( 0.0 ) ) * 0.25, 0.0, 1.0 ) );
	if ( cellProbe == 94 ) {
		if ( w > 0.0 && ( cellFarRed & 1 ) == 0 )
			cellFarF( P, N );
		int nW;
		cellLightsBegin( P, nW );
		return (float3)( float( ( w >= 1.0 ? 0 : nW ) + cellFarIters ) / 256.0 );
	}
	float3 E = (float3)( 0.0 );
	int nL;
	int bL = cellLightsBegin( P, nL );
	for ( int k = 0; k < nL; k++ ) {
		int i = cellLightAt( bL, k );
		float3 L;
		bool ns;
		float3 c = cellLightE( i, P, N, L, ns );
		E += c * max( dot( N, L ), 0.0 );
	}
	E += cellFarPlacedE( P, N ) * 0.31830989;
	return sqrt( clamp( E * 0.25, 0.0, 1.0 ) );
}

// debug probes for a program without the legacy BRDF helpers (pbrm_cell)
float3 cellSsgiE( float3 eProbe );	// lane SSGI2: probe 5 reads what the surface gets (below)

float3 cellProbeRaw( float3 P, float3 N )
{
	if ( cellProbe >= 91 && cellProbe <= 94 )
		return cellFarProbeOut( P, N );
	if ( cellProbe >= 70 && cellProbe <= 74 )	// the lit effects' probes: a surface writes black
		return (float3)( 0.0 );
	if ( cellProbe == 5 )	// lane SSGI2: E_probe x the open share + E_screen when SSGI runs (the double-count gate)
		return clamp( cellSsgiE( CELL_GI_UP ? cellGiE( P, N ) : (float3)( 0.0 ) ) * 0.31830989, 0.0, 1.0 );
	if ( cellProbe == 90 )	// the share of the weather's ambient the grid's sky replaced (indoors the DALC's)
		return (float3)( cellAmbShare( P, N ) );
	if ( cellProbe == 95 ) {	// r = the branch cellGiSample took / 16, g = its .a, b = its E luminance / pi (sqrt)
		float4 s = CELL_GI_UP ? cellGiSample( P, N ) : (float4)( 0.0 );
		return float3( float( cellGiPath ) * 0.0625, clamp( s.a, 0.0, 1.0 ),
		             sqrt( clamp( dot( max( s.rgb, (float3)( 0.0 ) ), float3( 0.2126, 0.7152, 0.0722 ) ) * 0.31830989, 0.0, 1.0 ) ) );
	}
	if ( cellProbe == 1 ) {
		float3 E = (float3)( 0.0 );
		int nL;
		int bL = cellLightsBegin( P, nL );
		for ( int k = 0; k < nL; k++ ) {
			int i = cellLightAt( bL, k );
			float3 L;
			bool ns;
			float3 c = cellLightE( i, P, N, L, ns );
			E += c * max( dot( N, L ), 0.0 );
		}
		return clamp( E * 0.25, 0.0, 1.0 );
	}
	if ( cellProbe == 7 )
		return cellShadowProbe( P, N );
	float3 q = clamp( floor( P - cellCenter + 32768.0 ), 0.0, 65535.0 );
	if ( cellProbe == 2 )
		return floor( q / 256.0 ) / 255.0;
	if ( cellProbe == 3 )
		return mod( q, 256.0 ) / 255.0;
	return N * 0.5 + 0.5;
}

/* a probe pass's picture on a white surface: no albedo, direct light, fog or imagespace. GI = E(N) / pi; Sky
 * visibility = the sky grid read the same way. Display: pow(clamp(x), 1 / 2.2), a fixed curve so two bakes compare.
 * Cyan = no probe reaches here (not magenta, which means a missing texture); in the surfel passes every surface is
 * cyan and the surfel tiles draw over it (src/features/gi/cellprobeview.h). */
float3 cellPassOut( float3 P, float3 N )
{
	const float3 none = float3( 0.0, 1.0, 1.0 );	// cyan, never magenta (magenta = missing texture)
	/* Probe coverage (pass 6): magenta where the GI sample's valid share k is 0 (this pass draws no texture, so it is
	 * never a missing one), elsewhere gray 0.25 + 0.5 k. Debug nofilter (bit 2048): the plain trilinear's k.
	 * Shares the one cellGiSample call below: one more call site takes fo4_cell past what the driver links. */
	const float3 hole = float3( 1.0, 0.0, 1.0 );
	if ( or( ( cellPass >= 3 && cellPass != 6 ), !CELL_GI_UP ) )
		return cellPass == 6 ? hole : none;
#ifdef WW_CELL_PRT
	float4 s = cellPass == 6 && ( cellPassRed & 2048 ) != 0 ? cellGiTrilinear( P, N )
	       : cellGiSample( P, cellPass != 6 && ( cellPassRed & 2 ) != 0 ? float3( 0.0, 0.0, 1.0 ) : N );
#else
	float4 s = cellGiSample( P, cellPass != 6 && ( cellPassRed & 2 ) != 0 ? float3( 0.0, 0.0, 1.0 ) : N );
#endif
	if ( cellPass == 6 )
		return s.a <= 0.01 ? hole : (float3)( 0.25 + 0.5 * clamp( s.a, 0.0, 1.0 ) );
	if ( s.a <= 0.01 )
		return none;
	float3 v = max( s.rgb / s.a, (float3)( 0.0 ) );
	if ( cellPass == 1 ) {
		v *= 0.31830989;
		if ( ( cellPassRed & 1 ) != 0 ) {	// debug "direct": the cell's lights leak into the pass
			int nL;
			int bL = cellLightsBegin( P, nL );
			for ( int k = 0; k < nL; k++ ) {
				int i = cellLightAt( bL, k );
				float3 L;
				bool ns;
				float3 c = cellLightE( i, P, N, L, ns );
				v += c * max( dot( N, L ), 0.0 );
			}
		}
	}
	return pow( clamp( v, 0.0, 1.0 ), (float3)( 1.0 / 2.2 ) );
}

#if !defined( WW_CELL_PBR ) && ( !defined( WW_CELL_FX ) || defined( WW_FX_DECALLIT ) )	// effects take none of the surface lobes; decals do
/* The game's own light specular (its deferred point/spot light shaders): normalized Blinn-Phong, n = 2^(gloss x 10 + 1),
 * D = NdotH^n (n + 2) / 2pi, Schlick F0 = 0.2, Cook-Torrance G with 1/NdotV folded in, x 1/4, clamped at 15, x pi.
 * The caller multiplies the light's irradiance (N.L inside) and the material's mask and color. */
float cellSpecGame( float3 N, float3 L, float3 V, float gloss )
{
	float n = exp2( gloss * 10.0 + 1.0 );
	float3 H = normalize( V + L );
	float NdotL = clamp( dot( N, L ), 0.0, 1.0 );
	float NdotV = clamp( dot( N, V ), 0.0, 1.0 );
	float VdotH = clamp( dot( V, H ), 0.0, 1.0 );
	float NdotH = clamp( dot( N, H ), 0.0, 1.0 );
	float D = pow( NdotH, n ) * ( n + 2.0 ) * 0.159155;
	float m = min( NdotL, NdotV );
	float G = ( VdotH >= 2.0 * NdotH * m ) ? 2.0 * NdotH * ( NdotV == m ? 1.0 : NdotL / max( NdotV, 1e-4 ) ) / max( VdotH, 1e-4 )
	                                       : 1.0 / max( NdotV, 1e-4 );
	float f1 = 1.0 - VdotH;
	float f4 = f1 * f1 * f1 * f1;
	float F = min( ( 1.0 - f1 * f4 ) * 0.2 + f4 * f1, 1.0 );
	return min( D * G * F * 0.25, 15.0 ) * 3.141593;
}

/* The game's legacy diffuse: Oren-Nayar with sigma = 1 - gloss, A = 1 - 0.5 s2 / (s2 + 0.57), B = 0.45 s2 / (s2 + 0.09),
 * the azimuth cosine from the UNnormalized tangent-plane projections, x sinL sinV / max(NdotL, NdotV).
 * Returns the factor that multiplies NdotL. */
float cellOren( float3 N, float3 L, float3 V, float NdotL, float gloss )
{
	if ( ( cellRed & 8 ) != 0 )
		return 1.0;	// WW_CELL_LIT_RED=lambert
	float s2 = ( 1.0 - gloss ) * ( 1.0 - gloss );
	float A = 1.0 - 0.5 * s2 / ( s2 + 0.57 );
	float B = 0.45 * s2 / ( s2 + 0.09 );
	float NdotV = dot( N, V );
	float3 tV = V - N * NdotV, tL = L - N * NdotL;
	float cosPhi = dot( tV, tL );
	if ( ( cellRed & 16 ) != 0 )
		cosPhi /= max( length( tV ) * length( tL ), 1e-6 );	// WW_CELL_LIT_RED=normalised: the textbook azimuth
	float geom = sqrt( clamp( ( 1.0 - NdotL * NdotL ) * ( 1.0 - NdotV * NdotV ), 0.0, 1.0 ) ) / max( max( NdotL, NdotV ), 1e-4 );
	return max( cosPhi, 0.0 ) * B * geom + A;
}

/* The game's back-light term, added to the diffuse by every light and the sun (legacy materials only):
 * saturate(dot(V, -L)) x (1 - NdotV)^0.01 x (1 - gloss), times NdotL and the light (shadow included). */
float cellRim( float3 N, float3 L, float3 V, float gloss )
{
	if ( ( cellRed & ( 8 | 32 ) ) != 0 )
		return 0.0;	// WW_CELL_LIT_RED=lambert / norim
	float edge = pow( clamp( 1.0 - dot( N, V ), 0.0, 1.0 ), 0.01 );
	return clamp( dot( V, -L ), 0.0, 1.0 ) * edge * ( 1.0 - gloss );
}

/* the placed lights at world point P, world normal N: the irradiance (Lambert, what the GI, the reflection and
 * probe 1 read), the game's Oren-Nayar diffuse (what the albedo takes) and the specular sum */
static float3 cellRimSum;	// the rim part of diffOn alone, for probe 10 (set by cellSumLights)
static bool cellFarFade = true;	// false = the lights unfaded
void cellSumLights( float3 P, float3 N, float3 Vw, float gloss, out float3 diff, out float3 diffOn, out float3 spec )
{
	diff = (float3)( 0.0 );
	diffOn = (float3)( 0.0 );
	cellRimSum = (float3)( 0.0 );
	spec = (float3)( 0.0 );
	float fw = cellFarFade ? cellFarW( P ) : 0.0;	// the real lights fade out over the far band
	if ( fw >= 1.0 )
		return;
	int nL;
	int bL = cellLightsBegin( P, nL );
	for ( int k = 0; k < nL; k++ ) {
		int i = cellLightAt( bL, k );
		float4 t0 = cellLights.Load( i * CELL_TPL );
		float3 Lv = t0.xyz - P;
		float d = length( Lv );
		if ( d >= t0.w )
			continue;
		float3 L = Lv / max( d, 0.001 );
		float NdotL = dot( N, L );
		if ( NdotL <= 0.0 )
			continue;
		float4 t1 = cellLights.Load( i * CELL_TPL + 1 );
		if ( !cellShapeIn( i, P, t0.xyz, t1.w ) )
			continue;
		float4 t3 = cellLights.Load( i * CELL_TPL + 3 );
		float a = cellRadial( d, t0.w, t3.xyz );
		float3 g = (float3)( 1.0 );	// lane LIGHTS5 (GOBO1)
		float3 gAx = (float3)( 0.0 );
		if ( t1.w > -1.5 ) {
			// PRTP2 section 2: base = saturate(1 - (1 - dot(-L, dir)) / (1 - cosOuter)), cone = min(base^falloff, 1)
			float4 t2 = cellLights.Load( i * CELL_TPL + 2 );
			float base = clamp( 1.0 - ( 1.0 - dot( -L, t2.xyz ) ) / max( 1.0 - t1.w, 1e-4 ), 0.0, 1.0 );
			a *= min( pow( base, max( t2.w, 1e-3 ) ), 1.0 );
			gAx = t2.xyz;
		}
		if ( a > 0.0 )
			g = cellGobo( i, -Lv, gAx, t1.w > -1.5 );	// GOBO2
		if ( a > 0.0 )
			a *= cellShadowF( i, P, N ) * cellContactLocal( i, P );	// lane SHADOW1; GAPLOOK1 (C8)
		float3 E = t1.rgb * g * a * NdotL;
		diff += E;
		// No Rim Lighting (2) drops the back-light; Ignore Roughness (4) also takes Lambert's diffuse
		int fl = int( t3.w + 0.5 ) & ( ( cellRed & 64 ) != 0 ? 1 : 7 );	// WW_CELL_LIT_RED=rimflags
		float rim = ( fl & 6 ) != 0 ? 0.0 : cellRim( N, L, Vw, gloss );
		diffOn += E * ( ( ( fl & 4 ) != 0 ? 1.0 : cellOren( N, L, Vw, NdotL, gloss ) ) + rim );
		cellRimSum += E * rim;
		// Non Specular (1) has no specular in the game's light shaders either
		if ( or( ( fl & 1 ) == 0, ( cellRed & 65536 ) != 0 ) )	// WW_CELL_SPEC_RED=nonspec
			spec += E * cellSpecGame( N, L, Vw, gloss );
	}
	if ( fw > 0.0 ) {
		diff *= 1.0 - fw;
		diffOn *= 1.0 - fw;
		cellRimSum *= 1.0 - fw;
		spec *= 1.0 - fw;
	}
}


#endif

// the scale an Ambient Only volume puts on the ambient at P (1 outside every volume)
float3 cellAmboScale( float3 P )
{
	for ( int i = 0; i < cellAmboCount; i++ ) {
		if ( cellAmboK[i].w > 0.5 ) {	// linked to a box, it fills the box (not the sphere)
			float4 b0 = cellAmboBox[i * 3], b1 = cellAmboBox[i * 3 + 1], b2 = cellAmboBox[i * 3 + 2];
			float3 k = float3( dot( b0.xyz, P ) + b0.w, dot( b1.xyz, P ) + b1.w, dot( b2.xyz, P ) + b2.w );
			if ( all( (abs( k ) < (float3)( 1.0 )) ) )
				return cellAmboK[i].rgb;
			continue;
		}
		float3 d = P - cellAmbo[i].xyz;
		if ( dot( d, d ) < cellAmbo[i].w * cellAmbo[i].w )
			return cellAmboK[i].rgb;
	}
	return (float3)( 1.0 );
}

// the ambient's affine sum per channel, before its power (scaled inside an Ambient Only volume)
float3 cellAmbientSum( float3 N, float3 P )
{
	float4 n1 = float4( N, 1.0 );
	return float3( dot( cellDalc[0], n1 ), dot( cellDalc[1], n1 ), dot( cellDalc[2], n1 ) ) * cellAmboScale( P );
}

// PRTP2 section 4: ambient(n) = pow(max(dot(row, (n, 1)), 0), 2.2) per channel
#ifdef WW_FX_AMBSHARE
/* MODULES1 R8-1 (the effect and particle programs only, cell_fxlaw.glsl fxLocalSky): the share of the weather's ambient
 * a lit card keeps, 1 - k of the GI volume's sky (cellGiSkyK), as a surface at the same place; 1 until it is set */
static float fxAmbShare = 1.0;
float3 cellAmbient( float3 N, float3 P )
{
	return pow( max( cellAmbientSum( N, P ), (float3)( 0.0 ) ), (float3)( 2.2 ) ) * fxAmbShare;
}
#else
float3 cellAmbient( float3 N, float3 P )
{
	return pow( max( cellAmbientSum( N, P ), (float3)( 0.0 ) ), (float3)( 2.2 ) );
}
#endif

/* The game's ambient specular (its sun pass): (1 - saturate(NdotV))^(3 - gloss) x 0.25 x the ambient at the mirror
 * direction R = 2 NdotV N - V (NdotV not clamped there). The caller multiplies the spec mask and colour. */
float3 cellEnvSpecGame( float3 N, float3 V, float3 P, float gloss )
{
	float NdotV = dot( N, V );
	return pow( 1.0 - clamp( NdotV, 0.0, 1.0 ), 3.0 - gloss ) * 0.25 * cellAmbient( 2.0 * NdotV * N - V, P );
}

/* the interior ambient beside our GI, as WW_CELL_GI_AMB asks (gi = the GI's irradiance / pi): keep (0), replace (1,
 * default: ambient x (1 - the grid's valid share)), off (2), max (3), asgi (4, debug: ambient := GI).
 * Without the GI every rule but off keeps the ambient. */
float3 cellAmbGi( float3 amb, float3 gi, float3 P, float3 N )
{
	if ( cellGiAmb == 2 )
		return (float3)( 0.0 );	// off: also with the GI off
	if ( or( cellGiAmb == 0, !CELL_GI_UP ) )
		return amb;
	if ( cellGiAmb == 1 )
		return amb * ( 1.0 - clamp( cellGiSample( P, N ).a, 0.0, 1.0 ) );
	if ( cellGiAmb == 3 )
		return max( amb - gi, (float3)( 0.0 ) );
	return gi;
}

#ifndef WW_CELL_FX


bool cellCubeGameOn()
{
	return or( abs( cellCubeMat.z - 1.0 ) < 0.5, abs( cellCubeMat.z - 3.0 ) < 0.5 );
}

/* the game's interior env reflection before the light (its deferred composite). Indoors only the material's own env
 * map reflects. sRGB texel at mip (1 - gloss) x 6 + view depth / 512 on a 128 cube, x 3 x saturate(spec) x
 * min(sqrt(saturate(gloss - 0.3)), 1) x min(env scale, 50). No fresnel, specular color or env mask. Linear. */
float3 cellCubeGame( TextureCube<float4> cube, SamplerState _cube_sampler, float3 dir, float gloss, float spec, float envScale, float3 posView )
{
	if ( !cellCubeGameOn() )
		return (float3)( 0.0 );
	float g = clamp( gloss, 0.0, 1.0 );
	float depth = abs( posView.z ) * length( cellRow[0].xyz );	// view units -> game units
	float lod = ( 1.0 - g ) * 6.0 + depth * 0.001953 + log2( float( textureSize( cube, 0 ).x ) / 128.0 );
	float3 c = cube.SampleLevel( _cube_sampler, dir, max( lod, 0.0 ) ).rgb;
	if ( cellCubeMat.z < 2.0 )	// an untagged cube: the game's sampler decodes sRGB before the filter, this after
		c = lerp( c / 12.92, pow( ( c + 0.055 ) / 1.055, (float3)( 2.4 ) ), step( (float3)( 0.04045 ), c ) );
	float k = 3.0 * clamp( spec, 0.0, 1.0 ) * min( sqrt( clamp( g - 0.3, 0.0, 1.0 ) ), 1.0 ) * clamp( envScale, 0.0, 50.0 );
	return c * k;
}

/* debug probes: 50 the reflection above / 4 (the caller's), 51 (u hi, u lo, tag), 52 (v hi, v lo,
 * tag) of the texture coordinate's fraction, 53 the normal's rounding residual (16 bits a component) */
float3 cellCubeProbe( float2 uv, float3 normalView )
{
	float2 f = clamp( floor( frac( uv ) * 65535.0 + 0.5 ), 0.0, 65535.0 );
	float tag = cellCubeMat.w / 255.0;
	if ( cellProbe == 51 )
		return float3( floor( f.x / 256.0 ) / 255.0, mod( f.x, 256.0 ) / 255.0, tag );
	if ( cellProbe == 52 )
		return float3( floor( f.y / 256.0 ) / 255.0, mod( f.y, 256.0 ) / 255.0, tag );
	float3 n = ( cellWorldDir( normalView ) * 0.5 + 0.5 ) * 255.0;
	return n - floor( n + 0.5 ) + 0.5;
}
#endif


Texture2D<float4> cellSsgi : register(t16, space0); SamplerState _cellSsgi_sampler : register(s16, space0);		// the work size, bottom row first: rgb the bounce E, a the open share

float3 cellSsgiE( float3 eProbe )
{
	if ( !cellSsgiOn )
		return eProbe;
	float4 s = cellSsgi.Sample( _cellSsgi_sampler, ( gl_FragCoord.xy - cellSsgiRect.xy ) * cellSsgiRect.zw );
	float keep = ( cellSsgiRed & 2 ) != 0 ? 1.0 : lerp( 1.0, clamp( s.a, 0.0, 1.0 ), cellSsgiK.x );
#ifdef WW_CELL_RTDDGI
	/* GIFIX1101 HO (PERFFIX2 2c): the hand-over to the traced volume. Its E already holds the occlusion and the bounce of
	 * what its probes see (1 - vis at the normal, by its fade); the screen keeps only the occluded share beyond that
	 * (the contact scale under the probe spacing), and adds that share of its bounce. An occluder both see darkens
	 * once. Sky-lit exteriors only; interiors keep this fill law, handed over by scale instead: the march stops at the
	 * volume's probe spacing (lane INTGI1, cellssgiplan.h wwSsgiRadius). Red WW_CELL_SSGI_RED=nohandover (8192) */
	if ( cellDdgiOn && cellGiSky && !cellInterior && ( cellSsgiRed & ( 2 | 8192 ) ) == 0 && cellGiVisN.y > 0.0 ) {
		float oS = 1.0 - clamp( s.a, 0.0, 1.0 );
		float oP = ( 1.0 - clamp( cellGiVisN.x, 0.0, 1.0 ) ) * clamp( cellGiVisN.y, 0.0, 1.0 );
		float n = max( oS - oP, 0.0 );
		return eProbe * ( 1.0 - cellSsgiK.x * n ) + max( s.rgb, (float3)( 0.0 ) ) * ( cellSsgiK.y * ( oS > 1e-4 ? n / oS : 0.0 ) );
	}
#endif
	return eProbe * keep + max( s.rgb, (float3)( 0.0 ) ) * cellSsgiK.y;
}

#if !defined( WW_CELL_PBR ) && ( !defined( WW_CELL_FX ) || defined( WW_FX_DECALLIT ) )

/* The lit color, in the program's sqrt-of-linear space. Interior: the cell's ambient, directional
 * and placed lights replace the viewport light. Exterior: the placed lights add to what the
 * viewport (or the Lookdev sun) already lit. */
float3 cellLit( float3 color, float3 albedo, float3 normalView, float3 posView, float3 Vview, float specMask, float3 specCol,
              float alphaR, float kSmith, float3 emissive, float3 cubeK )
{
	float3 P = cellWorldPos( posView );
	float3 N = cellWorldDir( normalView );
	float3 Vw = cellWorldDir( Vview );
	float3 diff, diffOn, spec;
	float gloss = 1.0 - sqrt( alphaR );	// the program's rough = 1 - gloss, alphaR = rough^2
	cellSumLights( P, N, Vw, gloss, diff, diffOn, spec );
	float3 alb = albedo * albedo;
	float3 gi = cellSsgiE( CELL_GI_UP ? cellGiE( P, N ) : (float3)( 0.0 ) ) * 0.31830989;
	float3 farF, farGP;	// the far placed light (diffuse only); the GI's placed share out x w
	float farW = cellFarTerms( P, N, farF, farGP );
	if ( farW > 0.0 ) {
		gi = max( gi - farW * farGP, (float3)( 0.0 ) );
		diff += farW * farF;
		diffOn += farW * farF;
	}
	float3 add = alb * ( diffOn + gi ) + spec * specMask * specCol;	// albedo x the game's diffuse; the GI stays Lambert
#ifdef WW_DV
	wwDvGi = alb * gi;
	wwDvAmb = color * color;
	wwDvSpec = spec * specMask * specCol;
#endif
	/* an exterior with the weather's sun takes the game's terms: the sun through Oren-Nayar + rim, shadowed by the
	 * cascades (cellDirVis), and the cube at the game's strength; color is then the viewport's ambient alone.
	 * Without the sun: the viewport light plus the cell's lights. */
	if ( !cellInterior && !cellHasDir )
		return sqrt( color * color + add + cellReflAdd );
	if ( !cellInterior ) {
		float3 Ld = normalize( cellDirTo );
		float nl = dot( N, Ld );
		float3 dir = cellDirColor * max( nl, 0.0 ) * cellDirVis;
		// MODULES1 G3: what the air took out of the beam, as soft light from the sky's half (no cascade shadow)
		float3 airDif = cellDirColor * ( cellAirDif * ( 0.5 + 0.5 * N.z ) );
		float3 Ex = diff + gi + dir + airDif + cellExtAmb;
		float3 Edx = diffOn + gi + airDif + dir * ( cellOren( N, Ld, Vw, nl, gloss ) + cellRim( N, Ld, Vw, gloss ) )
		         + cellDirColor * ( cellBackK * cellDirVis );
		float3 ambDim = (float3)( 1.0 );
		float specDim = 1.0;
#ifdef WW_SUNSHADOW
		ussTerrainDim( ambDim, specDim );	// lane SHADOWPAR1: FO4CS's ambient / specular dimming in terrain shadow
#endif
		float3 linx = color * color * ambDim + alb * Edx + spec * specMask * specCol + cellExtSpec
		          + cubeK * specDim * ( ( cellRed & 512 ) != 0 ? Ex : Edx + cellExtAmb ) + cellReflAdd;
#ifdef WW_DV
		wwDvSun = alb * ( Edx - diffOn - gi - airDif );
		wwDvAmb = color * color * ambDim + alb * airDif;
		wwDvSpec = spec * specMask * specCol + cellExtSpec;
		wwDvRefl = cubeK * specDim * ( ( cellRed & 512 ) != 0 ? Ex : Edx + cellExtAmb ) + cellReflAdd;
#endif
		return sqrt( max( linx, (float3)( 0.0 ) ) ) + emissive;
	}
	// the light reaching this point (a white surface would show it): the cubemap reflection is lit by it,
	// so a dark corner's reflection is dark (the viewport path scales it by its own light the same way)
	float3 E = diff + gi;
	float3 Ed = diffOn + gi;	// what the albedo takes: the direct terms through Oren-Nayar
	if ( cellHasDalc ) {
		float3 amb = cellAmbient( N, P );
		if ( cellGiAmb != 0 )
			amb = cellAmbGi( amb, gi, P, N );
		E += amb;
		Ed += amb;
		spec += cellEnvSpecGame( N, Vw, P, gloss );	// the game's ambient specular, from the same DALC
#ifdef WW_DV
		wwDvAmb = alb * amb;
#endif
	}
	if ( cellHasDir ) {
		float3 Ld = normalize( cellDirTo );
		float nl = dot( N, Ld );
		float3 dir = cellDirColor * max( nl, 0.0 );
		E += dir;
		Ed += dir * ( cellOren( N, Ld, Vw, nl, gloss ) + cellRim( N, Ld, Vw, gloss ) ) + cellDirColor * cellBackK;
		spec += dir * cellSpecGame( N, Ld, Vw, gloss );	// the game's sun shader lights it, specular included
	}
#ifdef WW_DV
	wwDvSpec = spec * specMask * specCol;
#endif
	// the game multiplies its cube term by the diffuse light (Ed); debug cubeold takes the Lambert sum
#ifdef WW_DV
	wwDvRefl = cubeK * ( ( cellRed & 512 ) != 0 ? E : Ed ) + cellReflAdd;
	wwDvSun = alb * ( Ed - diffOn - gi ) - ( cellHasDalc ? wwDvAmb : (float3)( 0.0 ) );
#endif
	float3 lin = alb * Ed + spec * specMask * specCol + cubeK * ( ( cellRed & 512 ) != 0 ? E : Ed ) + cellReflAdd;
	return sqrt( max( lin, (float3)( 0.0 ) ) ) + emissive;
}

// debug probes (WW_CELL_LIT_PROBE): raw values, no tonemap
float3 cellProbeOut( float3 normalView, float3 posView, float alphaR, float kSmith )
{
	float3 P = cellWorldPos( posView );
	float3 N = cellWorldDir( normalView );
	if ( cellProbe >= 70 && cellProbe <= 74 )	// the lit effects' probes: a surface writes black
		return (float3)( 0.0 );
	if ( cellProbe == 5 )	// lane SSGI2: as cellProbeRaw's
		return clamp( cellSsgiE( CELL_GI_UP ? cellGiE( P, N ) : (float3)( 0.0 ) ) * 0.31830989, 0.0, 1.0 );
	if ( cellProbe == 90 )	// the share of the weather's ambient the grid's sky replaced (indoors the DALC's)
		return (float3)( cellAmbShare( P, N ) );
	if ( cellProbe == 95 )	// the branch cellGiSample took, its .a and its E (cellProbeRaw's)
		return cellProbeRaw( P, N );
	if ( cellProbe >= 91 && cellProbe <= 94 )
		return cellFarProbeOut( P, N );
	if ( or( or( or( or( cellProbe == 1, cellProbe == 8 ), cellProbe == 10 ), cellProbe == 30 ), cellProbe == 31 ) ) {
		// 1: the irradiance / 4; 8: the game's diffuse (Oren-Nayar + rim) / 4, seen from the camera;
		// 10: the rim alone x 4, so the per-light rim flags show
		float3 diff, diffOn, spec;
		cellSumLights( P, N, cellProbe == 1 ? N : cellWorldDir( -posView ), 1.0 - sqrt( alphaR ), diff, diffOn, spec );
		if ( cellProbe == 10 )
			return clamp( cellRimSum * 4.0, 0.0, 1.0 );
		if ( cellProbe == 30 )	// the placed lights' specular / 4, before the mask
			return clamp( spec * 0.25, 0.0, 1.0 );
		if ( cellProbe == 31 )	// lane GAPFIX1: the same after the colour cellLit puts on it (deferred = white)
			return clamp( spec * cellSpecTint * 0.25, 0.0, 1.0 );
		return clamp( ( cellProbe == 8 ? diffOn : diff ) * 0.25, 0.0, 1.0 );
	}
	if ( cellProbe == 9 )
		return float3( 1.0 - sqrt( alphaR ), 0.0, 0.0 );	// the gloss the diffuse used
	if ( cellProbe == 11 )	// the interior ambient's affine sum before its 2.2, x 8 (it is dim)
		return cellHasDalc ? clamp( cellAmbientSum( N, P ) * 8.0, 0.0, 1.0 ) : (float3)( 0.0 );
	if ( cellProbe == 7 )
		return cellShadowProbe( P, N );
	// the position, 16 bits an axis over the 65536-unit box around the center
	float3 q = clamp( floor( P - cellCenter + 32768.0 ), 0.0, 65535.0 );
	if ( cellProbe == 2 )
		return floor( q / 256.0 ) / 255.0;
	if ( cellProbe == 3 )
		return mod( q, 256.0 ) / 255.0;
	return N * 0.5 + 0.5;
}
#endif
#endif	// WW_LOOSE_UNIFORMS
#else	// WW_HLSL_SRC: the per-variant files' half (HLSLBULK1, WW_FN_<name> picks functions); it goes with the last of them
// Shared HLSL for res/shaders/features/lights/cell_lights.glsl (include-only GLSL), HLSLBULK1 round 2 (notes/hlslbulk1/tools/factor.py). No bindings: the
// functions below are the converted ones, they read the declarations of the shader that includes them (its cbuffers,
// resources, statics and LooseUniforms: the per-module shim and the entry own those). Define WW_FN_<name> for each
// wanted function before the #include (and #undef after), or WW_FN_ALL for all of them; a _V<n> suffix is a variant
// of the same GLSL function (a different define set at the entry). Fusion Shaders (F4FS) reuses the math from here.
// Included by: cell_hdr_fd_ps.hlsl, cell_hdr_ps.hlsl, cell_precip_gpu_gs.hlsl, cell_precip_look_gs.hlsl, cell_precip_ps.hlsl, cell_reflrelight_ps.hlsl, cell_volair_ps.hlsl, fo4_cell_ps.hlsl, fo4_cellcsm_ps.hlsl, fo4_cellcsmfgc_ps.hlsl, fo4_cellcsmfgl_ps.hlsl, fo4_cellcsmfgp_ps.hlsl, fo4_cellcsmfs_ps.hlsl, fo4_cellfgc_ps.hlsl, fo4_cellfgl_ps.hlsl, fo4_cellfgp_ps.hlsl, fo4_cellfs_ps.hlsl, fo4_effectcell_ps.hlsl, fo4_effectcellfx_ps.hlsl, fo4_effectdecal_ps.hlsl, fo4_effectdecalcsm_ps.hlsl, fo4_water_ps.hlsl, fo4_waterlook_ps.hlsl, fo4_waterlookd_ps.hlsl, fo4_waterlookdp_ps.hlsl, fo4_waterlookm_ps.hlsl, fo4_waterlookp_ps.hlsl, impostor_cell_ps.hlsl, impostor_cellinst_ps.hlsl, impostor_cellinstez_ps.hlsl, lookdev_ground_cell_ps.hlsl, particles_cell_ps.hlsl, pbrm_cell_ps.hlsl, pbrm_cellcsm_ps.hlsl
// Not here (declarations, or no converted entry carried them): cellRoomSolid, cellGiSeeFrom, cellGiWall, cellGiSeen, cellGiRoomBlend, cellGiRoomSample, cellGiFillAt, cellGiSeenFill, cellGiFillPair, cellGiStrandSearch, cellGiStrandAnswer, cellGiTrilinear, cellGiStrandPick, cellGiSampleFull, cellGiKeyPack, cellGiKeyNormal, cellGiMarchPrime, cellGiVolAt, cellGiVolume, cellAoDecalAt, cellGiFineAdd, cellGiFirst
#ifdef WW_FN_ALL
#ifndef CELL_TPL
#define CELL_TPL 8
#endif
#ifndef CELL_GI_ONE
#define CELL_GI_ONE 1
#endif
#ifndef CELL_GI_HALF1
#define CELL_GI_HALF1 1
#endif
#endif

#if defined(WW_FN_cellImageSpace) || defined(WW_FN_ALL)
float3 cellImageSpace(float3 sqrtColor)
{
    float3 x = max(sqrtColor, 0.0f.xxx);
    x *= x;
    if (cellIsBloomOn)
    {
        x += cellIsBloom.Sample(_cellIsBloom_sampler, (gl_FragCoord.xy - cellIsBloomRect.xy) * cellIsBloomRect.zw).xyz;
    }
    x = (x * (((cellIsRed & 2) != 0) ? 1.0f : cellIsExposure)) * 2.0f;
    x *= (1.0f.xxx + cellIsWbD);
    float3 c;
    if (cellIsVanCurve)
    {
        float E = cellIsE;
        c = (((x * ((x * 0.1500000059604644775390625f) + 0.0500000007450580596923828125f.xxx)) + (0.20000000298023223876953125f * E).xxx) / ((x * ((x * 0.1500000059604644775390625f) + 0.5f.xxx)) + 0.0599999986588954925537109375f.xxx)) - (E / 0.300000011920928955078125f).xxx;
        c /= ((((0.20000000298023223876953125f * E) + 19.37599945068359375f) * 0.0408560000360012054443359375f) - (E / 0.300000011920928955078125f)).xxx;
    }
    else
    {
        float3 cPart2 = x * 0.5f;
        float3 cPart = wwTonemapPhysical(cPart2);
        c = cPart;
    }
    if ((cellIsRed & 4) == 0)
    {
        float luma = dot(c, float3(0.2125000059604644775390625f, 0.7153999805450439453125f, 0.07209999859333038330078125f));
        c = lerp(luma.xxx, c, cellIsCine.x.xxx);
        c = lerp(c, cellIsTint.yzw * luma, cellIsTint.x.xxx);
        c = (((c * cellIsCine.y) - cellIsAdapted.xxx) * cellIsCine.z) + cellIsAdapted.xxx;
    }
    c = pow(max(c, 0.0f.xxx), 0.4545454680919647216796875f.xxx);
    bool cond;
    if (cellIsLutOn)
    {
        cond = (cellIsRed & 1) == 0;
    }
    else
    {
        cond = cellIsLutOn;
    }
    if (cond)
    {
        c = cellIsLut.Sample(_cellIsLut_sampler, (c * 0.9375f) + 0.03125f.xxx).xyz;
    }
    return c;
}
#endif

#if defined(WW_FN_cellImageSpace_V2) || defined(WW_FN_ALL)
float3 cellImageSpace(float3 sqrtColor)
{
    float3 x = max(sqrtColor, 0.0f.xxx);
    x *= x;
    x = (x * (((cellIsRed & 2) != 0) ? 1.0f : cellIsExposure)) * 2.0f;
    x *= (1.0f.xxx + cellIsWbD);
    float3 c;
    if (cellIsVanCurve)
    {
        float E = cellIsE;
        c = (((x * ((x * 0.1500000059604644775390625f) + 0.0500000007450580596923828125f.xxx)) + (0.20000000298023223876953125f * E).xxx) / ((x * ((x * 0.1500000059604644775390625f) + 0.5f.xxx)) + 0.0599999986588954925537109375f.xxx)) - (E / 0.300000011920928955078125f).xxx;
        c /= ((((0.20000000298023223876953125f * E) + 19.37599945068359375f) * 0.0408560000360012054443359375f) - (E / 0.300000011920928955078125f)).xxx;
    }
    else
    {
        float3 cPart2 = x * 0.5f;
        float3 cPart = wwTonemapPhysical(cPart2);
        c = cPart;
    }
    if ((cellIsRed & 4) == 0)
    {
        float luma = dot(c, float3(0.2125000059604644775390625f, 0.7153999805450439453125f, 0.07209999859333038330078125f));
        c = lerp(luma.xxx, c, cellIsCine.x.xxx);
        c = lerp(c, cellIsTint.yzw * luma, cellIsTint.x.xxx);
        c = (((c * cellIsCine.y) - cellIsAdapted.xxx) * cellIsCine.z) + cellIsAdapted.xxx;
    }
    c = pow(max(c, 0.0f.xxx), 0.4545454680919647216796875f.xxx);
    bool cond;
    if (cellIsLutOn)
    {
        cond = (cellIsRed & 1) == 0;
    }
    else
    {
        cond = cellIsLutOn;
    }
    if (cond)
    {
        c = cellIsLut.Sample(_cellIsLut_sampler, (c * 0.9375f) + 0.03125f.xxx).xyz;
    }
    return c;
}
#endif

#if defined(WW_FN_cellImageSpace_V3) || defined(WW_FN_ALL)
float3 cellImageSpace(float3 sqrtColor)
{
    float3 x = max(sqrtColor, 0.0f.xxx);
    x *= x;
    if (cellIsBloomOn)
    {
        x += (cellIsBloom.Sample(_cellIsBloom_sampler, (gl_FragCoord.xy - cellIsBloomRect.xy) * cellIsBloomRect.zw).xyz * cellBloomGain);
    }
    x = (x * (((cellIsRed & 2) != 0) ? 1.0f : cellIsExposure)) * 2.0f;
    x *= (1.0f.xxx + cellIsWbD);
    if (cellIsFoBloomOn && (cellIsFoBloomEc > 0.0f))
    {
        float cPart2;
        if (cellIsFoBloomFogCap > 0.0f)
        {
            cPart2 = min(cellBloomGain, cellIsFoBloomFogCap);
        }
        else
        {
            cPart2 = cellBloomGain;
        }
        float kg = min(cellIsFoBloomEc * cPart2, 1.0f);
        x = (x * (1.0f - kg)) + (cellFoBloomHere * (2.0f * (kg / cellIsFoBloomEc)));
    }
    else
    {
        if (cellIsFoBloomOn)
        {
            x += (cellFoBloomHere * (2.0f * cellBloomGain));
        }
    }
    float3 c;
    if (cellIsVanCurve)
    {
        float E = cellIsE;
        c = (((x * ((x * 0.1500000059604644775390625f) + 0.0500000007450580596923828125f.xxx)) + (0.20000000298023223876953125f * E).xxx) / ((x * ((x * 0.1500000059604644775390625f) + 0.5f.xxx)) + 0.0599999986588954925537109375f.xxx)) - (E / 0.300000011920928955078125f).xxx;
        c /= ((((0.20000000298023223876953125f * E) + 19.37599945068359375f) * 0.0408560000360012054443359375f) - (E / 0.300000011920928955078125f)).xxx;
    }
    else
    {
        float3 cPart = x * 0.5f;
        float3 cond = wwTonemapPhysical(cPart);
        c = cond;
    }
    if ((cellIsRed & 4) == 0)
    {
        float luma = dot(c, float3(0.2125000059604644775390625f, 0.7153999805450439453125f, 0.07209999859333038330078125f));
        c = lerp(luma.xxx, c, cellIsCine.x.xxx);
        c = lerp(c, cellIsTint.yzw * luma, cellIsTint.x.xxx);
        c = (((c * cellIsCine.y) - cellIsAdapted.xxx) * cellIsCine.z) + cellIsAdapted.xxx;
    }
    c = pow(max(c, 0.0f.xxx), 0.4545454680919647216796875f.xxx);
    bool cond2;
    if (cellIsLutOn)
    {
        cond2 = (cellIsRed & 1) == 0;
    }
    else
    {
        cond2 = cellIsLutOn;
    }
    if (cond2)
    {
        c = cellIsLut.Sample(_cellIsLut_sampler, (c * 0.9375f) + 0.03125f.xxx).xyz;
    }
    return c;
}
#endif

#if defined(WW_FN_cellWorldPos) || defined(WW_FN_ALL)
float3 cellWorldPos(float3 posView)
{
    float4 p = float4(posView, 1.0f);
    return float3(dot(cellRow[0], p), dot(cellRow[1], p), dot(cellRow[2], p));
}
#endif

#if defined(WW_FN_cellWorldDir) || defined(WW_FN_ALL)
float3 cellWorldDir(float3 dirView)
{
    return normalize(float3(dot(cellRow[0].xyz, dirView), dot(cellRow[1].xyz, dirView), dot(cellRow[2].xyz, dirView)));
}
#endif

#if defined(WW_FN_cellAttVan) || defined(WW_FN_ALL)
float cellAttVan(float x, float3 c)
{
    float cPart2;
    if (c.z > 0.0f)
    {
        cPart2 = pow(x, c.z);
    }
    else
    {
        cPart2 = 1.0f;
    }
    float xe = cPart2;
    return pow(1.0f - clamp((c.y * xe) + c.x, 0.0f, 1.0f), 2.2000000476837158203125f);
}
#endif

#if defined(WW_FN_cellPhysRaw) || defined(WW_FN_ALL)
float cellPhysRaw(float x, float s)
{
    float x2 = x * x;
    float w = clamp(1.0f - (x2 * x2), 0.0f, 1.0f);
    float den = x2 + (s * s);
    float cPart2;
    if (den > 0.0f)
    {
        cPart2 = (w * w) / den;
    }
    else
    {
        cPart2 = 0.0f;
    }
    return cPart2;
}
#endif

#if defined(WW_FN_cellPhysFalloff) || defined(WW_FN_ALL)
float cellPhysFalloff(float van, float x, float r, float3 c)
{
    float a = clamp(cellLightPhys.y, 0.0f, 1.0f);
    bool cPart2 = !(c.z > 0.0f);
    bool cPart;
    if (!cPart2)
    {
        cPart = !(clamp(c.y + c.x, 0.0f, 1.0f) > clamp(c.x, 0.0f, 1.0f));
    }
    else
    {
        cPart = cPart2;
    }
    bool cond;
    if (!cPart)
    {
        float cond2 = 1.0f;
        float3 condPart = c;
        cond = cellAttVan(cond2, condPart) >= 0.0039215688593685626983642578125f;
    }
    else
    {
        cond = cPart;
    }
    if (cond)
    {
        // an authored curve that is no falloff keeps the game's
        return van;
    }
    float vAPart2 = a;
    float3 vAPart = c;
    float vA = cellAttVan(vAPart2, vAPart);
    float b = 1.0f - (a * a);
    float test;
    if (b > 0.0f)
    {
        test = pow(b, 1.10000002384185791015625f);
    }
    else
    {
        test = 0.0f;
    }
    if (vA > test)
    {
        return van;
    }
    // one loop for both: the program's size (GIDARK1's link limit)
    int red = int(cellLightPhysB.z + 0.5f);
    if (cellLightPhys.x > 1.5f)
    {
        float x0Pick;
        if (r > 0.0f)
        {
            x0Pick = clamp((max(max(cellLightPhysB.x, 0.001000000047497451305389404296875f), max((0.100000001490116119384765625f * r) / 69.98999786376953125f, 0.20000000298023223876953125f)) * 69.98999786376953125f) / r, 9.9999997473787516355514526367188e-05f, 0.5f);
        }
        else
        {
            x0Pick = 0.5f;
        }
        float x0 = x0Pick;
        float den = 0.89999997615814208984375f - (0.666666686534881591796875f * x0);
        float t = clamp((x - 0.800000011920928955078125f) / 0.20000000298023223876953125f, 0.0f, 1.0f);
        float wPick;
        if ((red & 2) != 0)
        {
            wPick = 1.0f;
        }
        else
        {
            wPick = 1.0f - ((t * t) * (3.0f - (2.0f * t)));
        }
        float w = wPick;
        float m = max(x0, x);
        float cellPhysFalloffRet2;
        if (den > 0.0f)
        {
            cellPhysFalloffRet2 = ((cellLightPhys.w / den) * w) / (m * m);
        }
        else
        {
            cellPhysFalloffRet2 = 0.0f;
        }
        return cellPhysFalloffRet2;
    }
    float s = cellLightPhys.z;
    float rawPart2 = a;
    float rawPart = s;
    float raw = cellPhysRaw(rawPart2, rawPart);
    float kPick;
    if ((red & 1) != 0)
    {
        kPick = 1.0f;
    }
    else
    {
        float kPick2;
        if (raw > 0.0f)
        {
            kPick2 = vA / raw;
        }
        else
        {
            kPick2 = 0.0f;
        }
        kPick = kPick2;
    }
    float k = kPick;
    float cellPhysFalloffRet;
    if (k > 0.0f)
    {
        float cellPhysFalloffPart2 = x;
        float cellPhysFalloffPart = s;
        cellPhysFalloffRet = k * cellPhysRaw(cellPhysFalloffPart2, cellPhysFalloffPart);
    }
    else
    {
        cellPhysFalloffRet = van;
    }
    return cellPhysFalloffRet;
}
#endif

#if defined(WW_FN_cellRadial) || defined(WW_FN_ALL)
float cellRadial(float d, float r, float3 bse)
{
    float x = clamp(d / max(r, 0.001000000047497451305389404296875f), 0.0f, 1.0f);
    float cPart2;
    if (bse.z > 0.0f)
    {
        cPart2 = pow(x, bse.z);
    }
    else
    {
        cPart2 = 1.0f;
    }
    float xe = cPart2;
    float k = 1.0f - clamp((bse.y * xe) + bse.x, 0.0f, 1.0f);
    if ((cellRed & 1) != 0)
    {
        return k;
    }
    float cPart;
    if (cellLightPhys.x > 0.5f)
    {
        float cond = pow(k, 2.2000000476837158203125f);
        float cond2 = x;
        float condPart = r;
        float3 vAPart2 = bse;
        cPart = cellPhysFalloff(cond, cond2, condPart, vAPart2);
    }
    else
    {
        cPart = pow(k, 2.2000000476837158203125f);
    }
    return cPart;
}
#endif

#if defined(WW_FN_cellShadowF) || defined(WW_FN_ALL)
float cellShadowF(int i, float3 P, float3 N)
{
    float4 t4 = cellLights.Load((i * 8) + 4);
    bool cPart2 = !cellShadowOn;
    bool cPart;
    if (!cPart2)
    {
        cPart = t4.y < 0.5f;
    }
    else
    {
        cPart = cPart2;
    }
    if (cPart)
    {
        return 1.0f;
    }
    float4 t0 = cellLights.Load(i * 8);
    bool cond = t4.y > 1.5f;
    bool cond2;
    if (cond)
    {
        cond2 = t4.y < 2.5f;
    }
    else
    {
        cond2 = cond;
    }
    bool condPart;
    if (cond2)
    {
        condPart = dot(P - t0.xyz, cellLights.Load((i * 8) + 2).xyz) < 0.0f;
    }
    else
    {
        condPart = cond2;
    }
    if (condPart)
    {
        return 0.0f;
    }
    if (t4.x < (-0.5f))
    {
        return 1.0f;
    }
    float3 r = P - t0.xyz;
    float3 rn = r + (N * ((1.5f * length(r)) * cellShadowTexel));
    float d = length(rn);
    float ref = (d - 1.0f) / max(t0.w, 0.001000000047497451305389404296875f);
    float3 a = rn / max(d, 0.001000000047497451305389404296875f).xxx;
    bool3 vAPart2 = (abs(a.z) < 0.89999997615814208984375f).xxx;
    float3 u = normalize(cross(a, float3(vAPart2.x ? float3(0.0f, 0.0f, 1.0f).x : float3(1.0f, 0.0f, 0.0f).x, vAPart2.y ? float3(0.0f, 0.0f, 1.0f).y : float3(1.0f, 0.0f, 0.0f).y, vAPart2.z ? float3(0.0f, 0.0f, 1.0f).z : float3(1.0f, 0.0f, 0.0f).z)));
    float3 v = cross(a, u);
    float s = 0.0f;
    for (int y = -1; y <= 1; y++)
    {
        for (int x = -1; x <= 1; x++)
        {
            s += cellShadow.SampleCmp(_cellShadow_sampler, float4(a + (((u * float(x)) + (v * float(y))) * cellShadowTexel), t4.x), ref);
        }
    }
    float vAPart;
    if (cellShadowFadeOn)
    {
        vAPart = lerp(1.0f, s / 9.0f, cellShadowFade[clamp(int(t4.x + 0.5f), 0, 15)]);
    }
    else
    {
        vAPart = s / 9.0f;
    }
    return vAPart;
}
#endif

#if defined(WW_FN_cellShadowF_V2) || defined(WW_FN_ALL)
float cellShadowF(int i, float3 P, float3 N)
{
    float4 t4 = cellLights.Load((i * 8) + 4);
    bool cPart2 = !cellShadowOn;
    bool cPart;
    if (!cPart2)
    {
        cPart = t4.y < 0.5f;
    }
    else
    {
        cPart = cPart2;
    }
    if (cPart)
    {
        return 1.0f;
    }
    float4 t0 = cellLights.Load(i * 8);
    bool cond = t4.y > 1.5f;
    bool cond2;
    if (cond)
    {
        cond2 = t4.y < 2.5f;
    }
    else
    {
        cond2 = cond;
    }
    bool condPart;
    if (cond2)
    {
        condPart = dot(P - t0.xyz, cellLights.Load((i * 8) + 2).xyz) < 0.0f;
    }
    else
    {
        condPart = cond2;
    }
    if (condPart)
    {
        return 0.0f;
    }
    if (t4.x < (-0.5f))
    {
        return 1.0f;
    }
    float3 r = P - t0.xyz;
    float3 rn = r + (N * ((1.5f * length(r)) * cellShadowTexel));
    float d = length(rn);
    float ref = (d - 1.0f) / max(t0.w, 0.001000000047497451305389404296875f);
    float3 a = rn / max(d, 0.001000000047497451305389404296875f).xxx;
    bool3 vAPart2 = (abs(a.z) < 0.89999997615814208984375f).xxx;
    float3 u = normalize(cross(a, float3(vAPart2.x ? float3(0.0f, 0.0f, 1.0f).x : float3(1.0f, 0.0f, 0.0f).x, vAPart2.y ? float3(0.0f, 0.0f, 1.0f).y : float3(1.0f, 0.0f, 0.0f).y, vAPart2.z ? float3(0.0f, 0.0f, 1.0f).z : float3(1.0f, 0.0f, 0.0f).z)));
    float3 v = cross(a, u);
    float s = 0.0f;
    for (int y = -1; y <= 1; y++)
    {
        for (int x = -1; x <= 1; x++)
        {
            s += cellShadow.SampleCmpLevelZero(_cellShadow_sampler, float4(a + (((u * float(x)) + (v * float(y))) * cellShadowTexel), t4.x), ref);
        }
    }
    float vAPart;
    if (cellShadowFadeOn)
    {
        vAPart = lerp(1.0f, s / 9.0f, cellShadowFade[clamp(int(t4.x + 0.5f), 0, 15)]);
    }
    else
    {
        vAPart = s / 9.0f;
    }
    return vAPart;
}
#endif

#if defined(WW_FN_cellContactLocal) || defined(WW_FN_ALL)
float cellContactLocal(int i, float3 P)
{
    float4 t4 = cellLights.Load((i * 8) + 4);
    bool cPart2 = !cellCtOn;
    bool cPart;
    if (!cPart2)
    {
        bool cond;
        if (cellShadowOn)
        {
            cond = t4.y > 0.5f;
        }
        else
        {
            cond = cellShadowOn;
        }
        bool cond2;
        if (cond)
        {
            cond2 = t4.x > (-0.5f);
        }
        else
        {
            cond2 = cond;
        }
        cPart = cond2;
    }
    else
    {
        cPart = cPart2;
    }
    if (cPart)
    {
        return 1.0f;
    }
    float4 t0 = cellLights.Load(i * 8);
    // world to view: the inverse of cellRow's rotation (orthonormal) and translation
    float3 w = P - float3(cellRow[0].w, cellRow[1].w, cellRow[2].w);
    float3 l = t0.xyz - float3(cellRow[0].w, cellRow[1].w, cellRow[2].w);
    // columns = rows of view-to-world: R = its transpose
    float3x3 R = float3x3(float3(cellRow[0].xyz), float3(cellRow[1].xyz), float3(cellRow[2].xyz));
    float3 pv = mul(w, R);
    float3 lv = mul(l, R);
    float3 toL = lv - pv;
    float dist = length(toL);
    if (dist < 0.001000000047497451305389404296875f)
    {
        return 1.0f;
    }
    float3 dir = (toL / dist.xxx) * ((cellCtRed == 1) ? (-1.0f) : 1.0f);
    float z = max(-pv.z, 8.0f);
    float bias = 0.800000011920928955078125f + (z * 0.01200000010430812835693359375f);
    float thickMin = 0.60000002384185791015625f + (z * 0.008000000379979610443115234375f);
    float thickMax = max(20.0f, z * 0.100000001490116119384765625f);
    float reach = max(min(0.1500000059604644775390625f * t0.w, 12.0f), 4.0f);
    float march = min(dist - bias, reach);
    if (march <= 0.0f)
    {
        return 1.0f;
    }
    float3 S0 = pv + (dir * bias);
    float3 S1 = S0 + (dir * march);
    float2 a = ((cellCtProj.xy * S0.xy) + (cellCtProj.zw * S0.z)) / max(-S0.z, 9.9999997473787516355514526367188e-05f).xx;
    float2 b = ((cellCtProj.xy * S1.xy) + (cellCtProj.zw * S1.z)) / max(-S1.z, 9.9999997473787516355514526367188e-05f).xx;
    float px = length(((b - a) * 0.5f) * cellCtRect.zw);
    int steps = clamp(int(px), 2, 48);
    float jit = frac(52.98291778564453125f * frac(dot(gl_FragCoord.xy, float2(0.067110560834407806396484375f, 0.005837149918079376220703125f))));
    for (int k = 1; k <= 48; k++)
    {
        if (k > steps)
        {
            break;
        }
        float t = (float(k) - jit) / float(steps);
        float3 S = S0 + (dir * (march * t));
        if (S.z >= (-1.0f))
        {
            break;
        }
        float2 ndc = ((cellCtProj.xy * S.xy) + (cellCtProj.zw * S.z)) / (-S.z).xx;
        float2 q = ((ndc * 0.5f) + 0.5f.xx) * cellCtRect.zw;
        bool condPart = any(bool2(q.x < 0.0f.xx.x, q.y < 0.0f.xx.y));
        bool vAPart2;
        if (!condPart)
        {
            vAPart2 = any(bool2(q.x >= cellCtRect.zw.x, q.y >= cellCtRect.zw.y));
        }
        else
        {
            vAPart2 = condPart;
        }
        if (vAPart2)
        {
            break;
        }
        float delta = (-S.z) - cellCtGbuf.Load(int3(int2(q), 0)).w;
        if ((delta > thickMin) && (delta < thickMax))
        {
            return 1.0f - (clamp(1.0f - ((march * t) / reach), 0.0f, 1.0f) * 0.5f);
        }
    }
    return 1.0f;
}
#endif

#if defined(WW_FN_cellShadowProbe) || defined(WW_FN_ALL)
float3 cellShadowProbe(float3 P, float3 N)
{
    float3 o = 0.0f.xxx;
    float3 cPart2 = P;
    int cPart;
    int cond = cellLightsBegin(cPart2, cPart);
    int nL = cPart;
    int bL = cond;
    for (int k = 0; k < nL; k++)
    {
        int cond2 = bL;
        int condPart = k;
        int i = cellLightAt(cond2, condPart);
        float slot = cellLights.Load((i * 8) + 4).x;
        bool wwh0 = (slot < (-0.5f));
        bool wwh1 = (slot > 2.5f);
        if (or(wwh0, wwh1))   // HLSLFIX2 orfix.py: no short-circuit branch (NV took the wrong side)
        {
            continue;
        }
        float4 t0 = cellLights.Load(i * 8);
        float3 Lv = t0.xyz - P;
        float d = length(Lv);
        bool vAPart2 = d >= t0.w;
        bool vAPart;
        if (!vAPart2)
        {
            vAPart = dot(N, Lv / max(d, 0.001000000047497451305389404296875f).xxx) < 0.0500000007450580596923828125f;
        }
        else
        {
            vAPart = vAPart2;
        }
        if (vAPart)
        {
            continue;
        }
        int test = i;
        float3 x0Pick = P;
        float3 wPick = N;
        o[int(slot + 0.5f)] = (2.0f + (cellShadowF(test, x0Pick, wPick) * 253.0f)) / 255.0f;
    }
    return o;
}
#endif

#if defined(WW_FN_cellRoomUnpack) || defined(WW_FN_ALL)
int2 cellRoomUnpack(float v)
{
    int k = int(v + 0.5f);
    return int2((k / 4096) - 1, (k - ((k / 4096) * 4096)) - 1);
}
#endif

#if defined(WW_FN_cellRoomAt) || defined(WW_FN_ALL)
int2 cellRoomAt(float3 q)
{
    float3 c = floor((q - cellRoomsOrigin) / cellRoomsCell.xxx);
    bool cPart2 = any(bool3(c.x < 0.0f.xxx.x, c.y < 0.0f.xxx.y, c.z < 0.0f.xxx.z));
    bool cPart;
    if (!cPart2)
    {
        cPart = any(bool3(c.x >= cellRoomsDims.x, c.y >= cellRoomsDims.y, c.z >= cellRoomsDims.z));
    }
    else
    {
        cPart = cPart2;
    }
    if (cPart)
    {
        return int2(-1, -1);
    }
    float cond = cellRooms.Load(int4(int3(c), 0)).x;
    return cellRoomUnpack(cond);
}
#endif

#if defined(WW_FN_cellGiEdgeFade) || defined(WW_FN_ALL)
float cellGiEdgeFade(float2 g)
{
    if (cellGiPad <= 0.0f)
    {
        return 1.0f;
    }
    float2 e = min(g, cellGiDims.xy - g);
    return clamp(min(e.x, e.y) / cellGiPad, 0.0f, 1.0f);
}
#endif

#if defined(WW_FN_cellGiSample) || defined(WW_FN_ALL)
float4 cellGiSample(float3 P, float3 N)
{
    return 0.0f.xxxx;
}
#endif

#if defined(WW_FN_cellGiE0) || defined(WW_FN_ALL)
float3 cellGiE0(float3 P, float3 N)
{
    float3 cPart2 = P;
    float3 cPart = N;
    float4 s = cellGiSample(cPart2, cPart);
    // with the sky in the grid the sample keeps its valid share (E x share), the other
    // ( 1 - share ) being the weather's own ambient the program leaves in (cellGiSkyK)
    if (cellGiSky && false)
    {
        return max(s.xyz, 0.0f.xxx);
    }
    float3 cond;
    if (s.w > 0.00999999977648258209228515625f)
    {
        cond = max(s.xyz / s.w.xxx, 0.0f.xxx);
    }
    else
    {
        cond = 0.0f.xxx;
    }
    return cond;
}
#endif

#if defined(WW_FN_cellGiE) || defined(WW_FN_ALL)
float3 cellGiE(float3 P, float3 N)
{
    float3 cPart2 = P;
    float3 cPart = N;
    float3 e = cellGiE0(cPart2, cPart);
    // lane PRTOFF1: the AO decals are the PRT relight's
    return e;
}
#endif

#if defined(WW_FN_cellGiSkyK) || defined(WW_FN_ALL)
float cellGiSkyK(float3 P, float3 N)
{
    bool cPart2;
    if (!true)
    {
        cPart2 = !(cellGiSky && false);
    }
    else
    {
        cPart2 = true;
    }
    if (or(cPart2, cellInterior))   // HLSLFIX2 orfix.py: no short-circuit branch (NV took the wrong side)
    {
        return 0.0f;
    }
    float3 cPart = P;
    float3 cond = N;
    return clamp(cellGiSample(cPart, cond).w, 0.0f, 1.0f);
}
#endif

#if defined(WW_FN_cellSpecSky) || defined(WW_FN_ALL)
float cellSpecSky(float3 P, float3 N, float3 R, float rough)
{
    bool cPart2 = (cellSpecOcc == 0) || true;
    bool cPart;
    if (!cPart2)
    {
        cPart = !(cellGiSky && false);
    }
    else
    {
        cPart = cPart2;
    }
    if (or(cPart, cellInterior))   // HLSLFIX2 orfix.py: no short-circuit branch (NV took the wrong side)
    {
        return 1.0f;
    }
    float r = clamp(rough, 0.0f, 1.0f);
    float3 cond;
    if (cellSpecOcc == 2)
    {
        cond = N;
    }
    else
    {
        cond = normalize(lerp(N, R, ((1.0f - r) * (sqrt(1.0f - r) + r)).xxx));
    }
    float3 D_1 = cond;
    float v = 1.0f;
    float cond2;
    if (cellSpecOcc == 2)
    {
        cond2 = lerp(1.0f, v, r);
    }
    else
    {
        cond2 = v;
    }
    return cond2;
}
#endif

#if defined(WW_FN_cellAmbShare) || defined(WW_FN_ALL)
float cellAmbShare(float3 P, float3 N)
{
    if (!cellInterior)
    {
        float3 cPart2 = P;
        float3 cPart = N;
        return cellGiSkyK(cPart2, cPart);
    }
    bool wwh2 = (cellGiAmb != 1);
    if ((true || (!cellHasDalc)) || wwh2)
    {
        return 0.0f;
    }
    float3 cond = P;
    float3 cond2 = N;
    return clamp(cellGiSample(cond, cond2).w, 0.0f, 1.0f);
}
#endif

#if defined(WW_FN_cellGobo) || defined(WW_FN_ALL)
float3 cellGobo(int i, float3 D_1, float3 axis, bool spot)
{
    if (!cellGoboOn)
    {
        return 1.0f.xxx;
    }
    int hf = -1;
    int h;
    float2 uv;
    if (spot)
    {
        float4 t5 = cellLights.Load((i * 8) + 5);
        if (t5.w < 0.5f)
        {
            return 1.0f.xxx;
        }
        float4 t6 = cellLights.Load((i * 8) + 6);
        h = int(t5.w + 0.5f) - 1;
        uv = (float2(dot(D_1, t6.xyz), dot(D_1, t5.xyz)) / ((2.0f * max(dot(D_1, axis), 0.001000000047497451305389404296875f)) * max(t6.w, 9.9999997473787516355514526367188e-05f)).xx) + 0.5f.xx;
    }
    else
    {
        if (cellGoboTbl <= 0)
        {
            return 1.0f.xxx;
        }
        int r = int(cellLights.Load(cellGoboTbl + (i / 4))[i % 4] + 0.5f) - 1;
        if (r < 0)
        {
            return 1.0f.xxx;
        }
        float4 a0 = cellLights.Load(r);
        h = int(a0.w + 0.5f) - 1;
        float3 cPart2;
        if ((cellGoboMap & 4) != 0)
        {
            cPart2 = D_1;
        }
        else
        {
            cPart2 = float3(dot(D_1, a0.xyz), dot(D_1, cellLights.Load(r + 1).xyz), dot(D_1, cellLights.Load(r + 2).xyz));
        }
        float3 l = cPart2;
        if ((cellGoboMap & 8) != 0)
        {
            float3 cPart;
            if ((cellGoboMap & 2) != 0)
            {
                cPart = l.yzx;
            }
            else
            {
                cPart = l;
            }
            float3 d = normalize(cPart);
            hf = ((d.z >= 0.0f) == ((cellGoboMap & 1) == 0)) ? 0 : 1;
            uv = ((d.xy / (1.0f + abs(d.z)).xx) * 0.5f) + 0.5f.xx;
        }
        else
        {
            float3 d_1 = normalize(l);
            hf = int(l.x < (-1.0f));
            float2 cond;
            if ((cellGoboMap & 2) != 0)
            {
                cond = d_1.yz;
            }
            else
            {
                cond = d_1.zy;
            }
            float2 e = cond;
            bool cond2 = hf == 1;
            bool condPart;
            if (cond2)
            {
                condPart = (cellGoboMap & 1) == 0;
            }
            else
            {
                condPart = cond2;
            }
            if (condPart)
            {
                e.x = -e.x;
            }
            float vAPart2;
            if (hf == 1)
            {
                vAPart2 = 1.0f - d_1.x;
            }
            else
            {
                vAPart2 = 1.0f + d_1.x;
            }
            uv = ((e / max(vAPart2, 9.9999997473787516355514526367188e-05f).xx) * 0.5f) + 0.5f.xx;
        }
    }
    if (cellGoboFlat)
    {
        float3 vAPart;
        if (cellGoboGam)
        {
            vAPart = pow(cellLights.Load(h + 1).xyz, 2.2000000476837158203125f.xxx);
        }
        else
        {
            vAPart = cellLights.Load(h + 1).xyz;
        }
        return vAPart;
    }
    if ((cellGoboRot & 4) != 0)
    {
        uv = uv.yx;
    }
    if ((cellGoboRot & 1) != 0)
    {
        uv.x = 1.0f - uv.x;
    }
    if ((cellGoboRot & 2) != 0)
    {
        uv.y = 1.0f - uv.y;
    }
    int2 sz = int2(cellLights.Load(h).xy + 0.5f.xx);
    int2 test;
    if (hf < 0)
    {
        test = sz;
    }
    else
    {
        test = int2(sz.x, sz.y / 2);
    }
    int2 pz = test;
    float2 p = (clamp(uv, 0.0f.xx, 1.0f.xx) * float2(pz)) - 0.5f.xx;
    int2 q = int2(floor(p));
    float2 f = p - float2(q);
    int2 q0 = clamp(q, int2(0, 0), pz - int2(1, 1));
    int2 q1 = clamp(q + int2(1, 1), int2(0, 0), pz - int2(1, 1));
    int b = (h + 2) + ((max(hf, 0) * pz.y) * sz.x);
    float3 c0 = lerp(cellLights.Load((b + (q0.y * sz.x)) + q0.x).xyz, cellLights.Load((b + (q0.y * sz.x)) + q1.x).xyz, f.x.xxx);
    float3 c1 = lerp(cellLights.Load((b + (q1.y * sz.x)) + q0.x).xyz, cellLights.Load((b + (q1.y * sz.x)) + q1.x).xyz, f.x.xxx);
    float3 x0Pick;
    if (cellGoboGam)
    {
        x0Pick = pow(lerp(c0, c1, f.y.xxx), 2.2000000476837158203125f.xxx);
    }
    else
    {
        x0Pick = lerp(c0, c1, f.y.xxx);
    }
    return x0Pick;
}
#endif

#if defined(WW_FN_cellGobo_V2) || defined(WW_FN_ALL)
float3 cellGobo(int i, float3 D, float3 axis, bool spot)
{
    if (!cellGoboOn)
    {
        return 1.0f.xxx;
    }
    int hf = -1;
    int h;
    float2 uv;
    if (spot)
    {
        float4 t5 = cellLights.Load((i * 8) + 5);
        if (t5.w < 0.5f)
        {
            return 1.0f.xxx;
        }
        float4 t6 = cellLights.Load((i * 8) + 6);
        h = int(t5.w + 0.5f) - 1;
        uv = (float2(dot(D, t6.xyz), dot(D, t5.xyz)) / ((2.0f * max(dot(D, axis), 0.001000000047497451305389404296875f)) * max(t6.w, 9.9999997473787516355514526367188e-05f)).xx) + 0.5f.xx;
    }
    else
    {
        if (cellGoboTbl <= 0)
        {
            return 1.0f.xxx;
        }
        int r = int(cellLights.Load(cellGoboTbl + (i / 4))[i % 4] + 0.5f) - 1;
        if (r < 0)
        {
            return 1.0f.xxx;
        }
        float4 a0 = cellLights.Load(r);
        h = int(a0.w + 0.5f) - 1;
        float3 cPart2;
        if ((cellGoboMap & 4) != 0)
        {
            cPart2 = D;
        }
        else
        {
            cPart2 = float3(dot(D, a0.xyz), dot(D, cellLights.Load(r + 1).xyz), dot(D, cellLights.Load(r + 2).xyz));
        }
        float3 l = cPart2;
        if ((cellGoboMap & 8) != 0)
        {
            float3 cPart;
            if ((cellGoboMap & 2) != 0)
            {
                cPart = l.yzx;
            }
            else
            {
                cPart = l;
            }
            float3 d = normalize(cPart);
            hf = ((d.z >= 0.0f) == ((cellGoboMap & 1) == 0)) ? 0 : 1;
            uv = ((d.xy / (1.0f + abs(d.z)).xx) * 0.5f) + 0.5f.xx;
        }
        else
        {
            float3 d_1 = normalize(l);
            hf = int(l.x < (-1.0f));
            float2 cond;
            if ((cellGoboMap & 2) != 0)
            {
                cond = d_1.yz;
            }
            else
            {
                cond = d_1.zy;
            }
            float2 e = cond;
            bool cond2 = hf == 1;
            bool condPart;
            if (cond2)
            {
                condPart = (cellGoboMap & 1) == 0;
            }
            else
            {
                condPart = cond2;
            }
            if (condPart)
            {
                e.x = -e.x;
            }
            float vAPart2;
            if (hf == 1)
            {
                vAPart2 = 1.0f - d_1.x;
            }
            else
            {
                vAPart2 = 1.0f + d_1.x;
            }
            uv = ((e / max(vAPart2, 9.9999997473787516355514526367188e-05f).xx) * 0.5f) + 0.5f.xx;
        }
    }
    if (cellGoboFlat)
    {
        float3 vAPart;
        if (cellGoboGam)
        {
            vAPart = pow(cellLights.Load(h + 1).xyz, 2.2000000476837158203125f.xxx);
        }
        else
        {
            vAPart = cellLights.Load(h + 1).xyz;
        }
        return vAPart;
    }
    if ((cellGoboRot & 4) != 0)
    {
        uv = uv.yx;
    }
    if ((cellGoboRot & 1) != 0)
    {
        uv.x = 1.0f - uv.x;
    }
    if ((cellGoboRot & 2) != 0)
    {
        uv.y = 1.0f - uv.y;
    }
    int2 sz = int2(cellLights.Load(h).xy + 0.5f.xx);
    int2 test;
    if (hf < 0)
    {
        test = sz;
    }
    else
    {
        test = int2(sz.x, sz.y / 2);
    }
    int2 pz = test;
    float2 p = (clamp(uv, 0.0f.xx, 1.0f.xx) * float2(pz)) - 0.5f.xx;
    int2 q = int2(floor(p));
    float2 f = p - float2(q);
    int2 q0 = clamp(q, int2(0, 0), pz - int2(1, 1));
    int2 q1 = clamp(q + int2(1, 1), int2(0, 0), pz - int2(1, 1));
    int b = (h + 2) + ((max(hf, 0) * pz.y) * sz.x);
    float3 c0 = lerp(cellLights.Load((b + (q0.y * sz.x)) + q0.x).xyz, cellLights.Load((b + (q0.y * sz.x)) + q1.x).xyz, f.x.xxx);
    float3 c1 = lerp(cellLights.Load((b + (q1.y * sz.x)) + q0.x).xyz, cellLights.Load((b + (q1.y * sz.x)) + q1.x).xyz, f.x.xxx);
    float3 x0Pick;
    if (cellGoboGam)
    {
        x0Pick = pow(lerp(c0, c1, f.y.xxx), 2.2000000476837158203125f.xxx);
    }
    else
    {
        x0Pick = lerp(c0, c1, f.y.xxx);
    }
    return x0Pick;
}
#endif

#if defined(WW_FN_cellShapeIn) || defined(WW_FN_ALL)
bool cellShapeIn(int i, float3 P, float3 Lpos, float shape)
{
    if (shape > (-2.5f))
    {
        return true;
    }
    if (shape > (-3.5f))
    {
        return dot(P - Lpos, cellLights.Load((i * 8) + 2).xyz) >= 0.0f;
    }
    float4 b0 = cellLights.Load((i * 8) + 5);
    float4 b1 = cellLights.Load((i * 8) + 6);
    float4 b2 = cellLights.Load((i * 8) + 7);
    float3 k = float3(dot(b0.xyz, P) + b0.w, dot(b1.xyz, P) + b1.w, dot(b2.xyz, P) + b2.w);
    float3 cPart2 = abs(k);
    return all(bool3(cPart2.x <= 1.0f.xxx.x, cPart2.y <= 1.0f.xxx.y, cPart2.z <= 1.0f.xxx.z));
}
#endif

#if defined(WW_FN_cellLightE) || defined(WW_FN_ALL)
float3 cellLightE(int i, float3 P, float3 N, inout float3 L, out bool noSpec)
{
    float4 t0 = cellLights.Load(i * 8);
    float3 Lv = t0.xyz - P;
    float d = length(Lv);
    L = Lv / max(d, 0.001000000047497451305389404296875f).xxx;
    noSpec = true;
    bool cPart2 = d >= t0.w;
    bool cPart;
    if (!cPart2)
    {
        cPart = dot(N, L) <= 0.0f;
    }
    else
    {
        cPart = cPart2;
    }
    if (cPart)
    {
        return 0.0f.xxx;
    }
    float4 t1 = cellLights.Load((i * 8) + 1);
    int cond = i;
    float3 cond2 = P;
    float3 condPart = t0.xyz;
    float vAPart2 = t1.w;
    if (!cellShapeIn(cond, cond2, condPart, vAPart2))
    {
        return 0.0f.xxx;
    }
    float4 t3 = cellLights.Load((i * 8) + 3);
    float vAPart = d;
    float test = t0.w;
    float3 x0Pick = t3.xyz;
    float a = cellRadial(vAPart, test, x0Pick);
    // lane LIGHTS5 (GOBO1)
    float3 g = 1.0f.xxx;
    float3 gAx = 0.0f.xxx;
    if (t1.w > (-1.5f))
    {
        float4 t2 = cellLights.Load((i * 8) + 2);
        float base = clamp(1.0f - ((1.0f - dot(-L, t2.xyz)) / max(1.0f - t1.w, 9.9999997473787516355514526367188e-05f)), 0.0f, 1.0f);
        a *= min(pow(base, max(t2.w, 0.001000000047497451305389404296875f)), 1.0f);
        gAx = t2.xyz;
    }
    if (a > 0.0f)
    {
        int wPick = i;
        float3 cellPhysFalloffRet2 = -Lv;
        float3 rawPart2 = gAx;
        bool rawPart = t1.w > (-1.5f);
        // one call for every shape
        g = cellGobo(wPick, cellPhysFalloffRet2, rawPart2, rawPart);
    }
    if (a > 0.0f)
    {
        int kPick = i;
        float3 kPick2 = P;
        float3 cellPhysFalloffRet = N;
        a *= cellShadowF(kPick, kPick2, cellPhysFalloffRet);
    }
    noSpec = (int(t3.w + 0.5f) & 1) != 0;
    return (t1.xyz * g) * a;
}
#endif

#if defined(WW_FN_cellAreaSpec) || defined(WW_FN_ALL)
void cellAreaSpec(int i, float3 P, float3 N, float3 V, out float3 Ls, inout float rS)
{
    float4 t0 = cellLights.Load(i * 8);
    float3 Lv = t0.xyz - P;
    float d = length(Lv);
    float R = clamp(min(cellLightPhysB.y, t0.w * cellLightPhys.z), 2.0f, 32.0f);
    float3 r = reflect(-V, N);
    float3 c = (r * dot(Lv, r)) - Lv;
    float3 p = Lv + (c * clamp(R / max(length(c), 9.9999997473787516355514526367188e-05f), 0.0f, 1.0f));
    Ls = normalize(p);
    if ((int(cellLightPhysB.z + 0.5f) & 4) == 0)
    {
        rS = sqrt(clamp(min(max(rS * rS, 9.9999997473787516355514526367188e-05f) + (R / max(2.0f * d, 9.9999997473787516355514526367188e-05f)), 0.99989998340606689453125f), 0.0f, 1.0f));
    }
}
#endif

#if defined(WW_FN_cellFarW) || defined(WW_FN_ALL)
float cellFarW(float3 P)
{
    if (!cellFarOn)
    {
        return 0.0f;
    }
    float d = length(P - float3(cellRow[0].w, cellRow[1].w, cellRow[2].w));
    if ((cellFarRed & 2) != 0)
    {
        return step(0.5f * (cellFarBand.x + cellFarBand.y), d);
    }
    return smoothstep(cellFarBand.x, cellFarBand.y, d);
}
#endif

#if defined(WW_FN_cellFarHash) || defined(WW_FN_ALL)
uint cellFarHash(int3 k, int side)
{
    return (((uint(k.x) * 73856093u) ^ (uint(k.y) * 19349663u)) ^ (uint(k.z) * 83492791u)) ^ (uint(side) * 2654435761u);
}
#endif

#if defined(WW_FN_cellFarF) || defined(WW_FN_ALL)
float3 cellFarF(float3 P, float3 N)
{
    float3 a = abs(N);
    int ax = 0;
    if (a.y > a.x)
    {
        ax = 1;
    }
    if (a.z > a[ax])
    {
        ax = 2;
    }
    int side = (2 * ax) + ((N[ax] >= 0.0f) ? 0 : 1);
    float3 nudge = 0.0f.xxx;
    nudge[ax] = ((side & 1) != 0) ? (-0.5f) : 0.5f;
    int3 k0 = int3(floor((P - nudge) / cellFarCell.xxx));
    if ((cellFarRed & 4) != 0)
    {
        side ^= 1;
    }
    uint mask = (1u << uint(cellFarBits)) - 1u;
    float R2 = (2.25f * cellFarCell) * cellFarCell;
    float3 s = 0.0f.xxx;
    float ws = 0.0f;
    for (int dz = -1; dz <= 1; dz++)
    {
        for (int dy = -1; dy <= 1; dy++)
        {
            for (int dx = -1; dx <= 1; dx++)
            {
                int3 k = k0 + int3(dx, dy, dz);
                int3 cPart2 = k;
                int cPart = side;
                uint h = cellFarHash(cPart2, cPart) & mask;
                float3 key = float3(float(k.x), float(k.y), float((k.z * 8) + side));
                for (int j = 0; (j < cellFarProbe) && (j < 64); j++)
                {
                    cellFarIters++;
                    float4 t = cellFarSlots.Load(int(h));
                    if (t.w == 0.0f)
                    {
                        break;
                    }
                    if (all(bool3(t.xyz.x == key.x, t.xyz.y == key.y, t.xyz.z == key.z)))
                    {
                        int r = int(t.w) - 1;
                        float3 d = P - cellFarRecs.Load(r * 2).xyz;
                        float q = 1.0f - (dot(d, d) / R2);
                        if (q > 0.0f)
                        {
                            q *= q;
                            s += (cellFarRecs.Load((r * 2) + 1).xyz * q);
                            ws += q;
                        }
                    }
                    h = (h + 1u) & mask;
                }
            }
        }
    }
    float3 cond;
    if (ws > 0.0f)
    {
        cond = s / ws.xxx;
    }
    else
    {
        cond = 0.0f.xxx;
    }
    return cond;
}
#endif

#if defined(WW_FN_cellFarPlacedE) || defined(WW_FN_ALL)
float3 cellFarPlacedE(float3 P, float3 N)
{
    if (!cellFarPlacedOn)
    {
        return 0.0f.xxx;
    }
    float3 g = ((P + (N * (0.5f * cellFarPlacedVoxel))) - cellFarPlacedOrigin) / cellFarPlacedVoxel.xxx;
    float2 xy = g.xy / cellFarPlacedDims.xy;
    float z = clamp(g.z, 0.5f, cellFarPlacedDims.z - 0.5f);
    float depth = 6.0f * cellFarPlacedDims.z;
    float3 n2 = N * N;
    float4 s = ((cellFarPlaced.Sample(_cellFarPlaced_sampler, float3(xy, (z + (((N.x >= 0.0f) ? 0.0f : 1.0f) * cellFarPlacedDims.z)) / depth)) * n2.x) + (cellFarPlaced.Sample(_cellFarPlaced_sampler, float3(xy, (z + (((N.y >= 0.0f) ? 2.0f : 3.0f) * cellFarPlacedDims.z)) / depth)) * n2.y)) + (cellFarPlaced.Sample(_cellFarPlaced_sampler, float3(xy, (z + (((N.z >= 0.0f) ? 4.0f : 5.0f) * cellFarPlacedDims.z)) / depth)) * n2.z);
    if (cellGiSky && false)
    {
        return max(s.xyz, 0.0f.xxx);
    }
    float3 cPart2;
    if (s.w > 0.00999999977648258209228515625f)
    {
        cPart2 = max(s.xyz / s.w.xxx, 0.0f.xxx);
    }
    else
    {
        cPart2 = 0.0f.xxx;
    }
    return cPart2;
}
#endif

#if defined(WW_FN_cellFarTerms) || defined(WW_FN_ALL)
float cellFarTerms(float3 P, float3 N, out float3 F, out float3 gP)
{
    F = 0.0f.xxx;
    gP = 0.0f.xxx;
    float3 cPart2 = P;
    float w = cellFarW(cPart2);
    if (w <= 0.0f)
    {
        return 0.0f;
    }
    if ((cellFarRed & 1) == 0)
    {
        float3 cPart = P;
        float3 cond = N;
        float3 cond2 = cellFarF(cPart, cond);
        F = cond2;
    }
    float3 condPart = P;
    float3 vAPart2 = N;
    gP = cellFarPlacedE(condPart, vAPart2) * 0.31830990314483642578125f;
    return w;
}
#endif

#if defined(WW_FN_cellFarProbeOut) || defined(WW_FN_ALL)
float3 cellFarProbeOut(float3 P, float3 N)
{
    float3 cPart2 = P;
    float w = cellFarW(cPart2);
    if (cellProbe == 93)
    {
        return w.xxx;
    }
    if (cellProbe == 91)
    {
        float3 cPart;
        if (cellFarOn)
        {
            float3 cond = P;
            float3 cond2 = N;
            float3 condPart = cellFarF(cond, cond2);
            cPart = condPart;
        }
        else
        {
            cPart = 0.0f.xxx;
        }
        return sqrt(clamp(cPart * 0.25f, 0.0f.xxx, 1.0f.xxx));
    }
    if (cellProbe == 94)
    {
        bool vAPart2 = w > 0.0f;
        bool vAPart;
        if (vAPart2)
        {
            vAPart = (cellFarRed & 1) == 0;
        }
        else
        {
            vAPart = vAPart2;
        }
        if (vAPart)
        {
            float3 test = P;
            float3 x0Pick = N;
            float3 wPick = cellFarF(test, x0Pick);
        }
        float3 cellPhysFalloffRet2 = P;
        int rawPart2;
        int rawPart = cellLightsBegin(cellPhysFalloffRet2, rawPart2);
        int nW = rawPart2;
        return (float(((w >= 1.0f) ? 0 : nW) + cellFarIters) / 256.0f).xxx;
    }
    float3 E = 0.0f.xxx;
    float3 kPick = P;
    int kPick2;
    int cellPhysFalloffRet = cellLightsBegin(kPick, kPick2);
    int nL = kPick2;
    int bL = cellPhysFalloffRet;
    float3 cellPhysFalloffPart2;
    bool cellPhysFalloffPart;
    for (int k = 0; k < nL; k++)
    {
        int iPart2 = bL;
        int iPart = k;
        int i = cellLightAt(iPart2, iPart);
        int cellLightEPart3 = i;
        float3 cellLightEPart2 = P;
        float3 cellLightEPart = N;
        float3 cellLightERet = cellLightE(cellLightEPart3, cellLightEPart2, cellLightEPart, cellPhysFalloffPart2, cellPhysFalloffPart);
        float3 L = cellPhysFalloffPart2;
        bool ns = cellPhysFalloffPart;
        float3 c = cellLightERet;
        E += (c * max(dot(N, L), 0.0f));
    }
    float3 EPart2 = P;
    float3 EPart = N;
    E += (cellFarPlacedE(EPart2, EPart) * 0.31830990314483642578125f);
    return sqrt(clamp(E * 0.25f, 0.0f.xxx, 1.0f.xxx));
}
#endif

#if defined(WW_FN_cellSsgiE) || defined(WW_FN_ALL)
float3 cellSsgiE(float3 eProbe)
{
    if (!cellSsgiOn)
    {
        return eProbe;
    }
    float4 s = cellSsgi.Sample(_cellSsgi_sampler, (gl_FragCoord.xy - cellSsgiRect.xy) * cellSsgiRect.zw);
    float cPart2;
    if ((cellSsgiRed & 2) != 0)
    {
        cPart2 = 1.0f;
    }
    else
    {
        cPart2 = lerp(1.0f, clamp(s.w, 0.0f, 1.0f), cellSsgiK.x);
    }
    float keep = cPart2;
    return (eProbe * keep) + (max(s.xyz, 0.0f.xxx) * cellSsgiK.y);
}
#endif

#if defined(WW_FN_cellProbeRaw) || defined(WW_FN_ALL)
float3 cellProbeRaw(float3 P, float3 N)
{
    if ((cellProbe >= 91) && (cellProbe <= 94))
    {
        float3 cPart2 = P;
        float3 cPart = N;
        float3 cond = cellFarProbeOut(cPart2, cPart);
        return cond;
    }
    // the lit effects' probes: a surface writes black
    if ((cellProbe >= 70) && (cellProbe <= 74))
    {
        return 0.0f.xxx;
    }
    // lane SSGI2: E_probe x the open share + E_screen when SSGI runs (the double-count gate)
    if (cellProbe == 5)
    {
        float3 cond2;
        if (false)
        {
            float3 condPart = P;
            float3 vAPart2 = N;
            cond2 = cellGiE(condPart, vAPart2);
        }
        else
        {
            cond2 = 0.0f.xxx;
        }
        float3 vAPart = cond2;
        return clamp(cellSsgiE(vAPart) * 0.31830990314483642578125f, 0.0f.xxx, 1.0f.xxx);
    }
    // the share of the weather's ambient the grid's sky replaced (indoors the DALC's)
    if (cellProbe == 90)
    {
        float3 test = P;
        float3 x0Pick = N;
        return cellAmbShare(test, x0Pick).xxx;
    }
    // r = the branch cellGiSample took / 16, g = its .a, b = its E luminance / pi (sqrt)
    if (cellProbe == 95)
    {
        float4 wPick;
        if (false)
        {
            float3 cellPhysFalloffRet2 = P;
            float3 rawPart2 = N;
            wPick = cellGiSample(cellPhysFalloffRet2, rawPart2);
        }
        else
        {
            wPick = 0.0f.xxxx;
        }
        float4 s = wPick;
        return float3(float(cellGiPath) * 0.0625f, clamp(s.w, 0.0f, 1.0f), sqrt(clamp(dot(max(s.xyz, 0.0f.xxx), float3(0.2125999927520751953125f, 0.715200006961822509765625f, 0.072200000286102294921875f)) * 0.31830990314483642578125f, 0.0f, 1.0f)));
    }
    if (cellProbe == 1)
    {
        float3 E = 0.0f.xxx;
        float3 rawPart = P;
        int kPick;
        int kPick2 = cellLightsBegin(rawPart, kPick);
        int nL = kPick;
        int bL = kPick2;
        float3 cellPhysFalloffRet;
        bool cellPhysFalloffPart2;
        for (int k = 0; k < nL; k++)
        {
            int cellPhysFalloffPart = bL;
            int iPart2 = k;
            int i = cellLightAt(cellPhysFalloffPart, iPart2);
            int iPart = i;
            float3 cellLightEPart3 = P;
            float3 cellLightEPart2 = N;
            float3 cellLightEPart = cellLightE(iPart, cellLightEPart3, cellLightEPart2, cellPhysFalloffRet, cellPhysFalloffPart2);
            float3 L = cellPhysFalloffRet;
            bool ns = cellPhysFalloffPart2;
            float3 c = cellLightEPart;
            E += (c * max(dot(N, L), 0.0f));
        }
        return clamp(E * 0.25f, 0.0f.xxx, 1.0f.xxx);
    }
    if (cellProbe == 7)
    {
        float3 cellLightERet = P;
        float3 EPart2 = N;
        return cellShadowProbe(cellLightERet, EPart2);
    }
    float3 q = clamp(floor((P - cellCenter) + 32768.0f.xxx), 0.0f.xxx, 65535.0f.xxx);
    if (cellProbe == 2)
    {
        return floor(q / 256.0f.xxx) / 255.0f.xxx;
    }
    if (cellProbe == 3)
    {
        return mod(q, 256.0f.xxx) / 255.0f.xxx;
    }
    return (N * 0.5f) + 0.5f.xxx;
}
#endif

#if defined(WW_FN_cellPassOut) || defined(WW_FN_ALL)
float3 cellPassOut(float3 P, float3 N)
{
    bool wwh3 = ((cellPass >= 3) && (cellPass != 6));
    if (or(wwh3, true))   // HLSLFIX2 orfix.py: no short-circuit branch (NV took the wrong side)
    {
        bool3 cPart2 = (cellPass == 6).xxx;
        return float3(cPart2.x ? float3(1.0f, 0.0f, 1.0f).x : float3(0.0f, 1.0f, 1.0f).x, cPart2.y ? float3(1.0f, 0.0f, 1.0f).y : float3(0.0f, 1.0f, 1.0f).y, cPart2.z ? float3(1.0f, 0.0f, 1.0f).z : float3(0.0f, 1.0f, 1.0f).z);
    }
    bool cPart = cellPass != 6;
    bool cond;
    if (cPart)
    {
        cond = (cellPassRed & 2) != 0;
    }
    else
    {
        cond = cPart;
    }
    bool3 cond2 = cond.xxx;
    float3 condPart = P;
    float3 vAPart2 = float3(cond2.x ? float3(0.0f, 0.0f, 1.0f).x : N.x, cond2.y ? float3(0.0f, 0.0f, 1.0f).y : N.y, cond2.z ? float3(0.0f, 0.0f, 1.0f).z : N.z);
    float4 s = cellGiSample(condPart, vAPart2);
    if (cellPass == 6)
    {
        float3 vAPart;
        if (s.w <= 0.00999999977648258209228515625f)
        {
            vAPart = float3(1.0f, 0.0f, 1.0f);
        }
        else
        {
            vAPart = (0.25f + (0.5f * clamp(s.w, 0.0f, 1.0f))).xxx;
        }
        return vAPart;
    }
    if (s.w <= 0.00999999977648258209228515625f)
    {
        return float3(0.0f, 1.0f, 1.0f);
    }
    float3 v = max(s.xyz / s.w.xxx, 0.0f.xxx);
    if (cellPass == 1)
    {
        v *= 0.31830990314483642578125f;
        // debug "direct": the cell's lights leak into the pass
        if ((cellPassRed & 1) != 0)
        {
            float3 test = P;
            int x0Pick;
            int wPick = cellLightsBegin(test, x0Pick);
            int nL = x0Pick;
            int bL = wPick;
            float3 cellPhysFalloffRet2;
            bool rawPart2;
            for (int k = 0; k < nL; k++)
            {
                int rawPart = bL;
                int kPick = k;
                int i = cellLightAt(rawPart, kPick);
                int kPick2 = i;
                float3 cellPhysFalloffRet = P;
                float3 cellPhysFalloffPart2 = N;
                float3 cellPhysFalloffPart = cellLightE(kPick2, cellPhysFalloffRet, cellPhysFalloffPart2, cellPhysFalloffRet2, rawPart2);
                float3 L = cellPhysFalloffRet2;
                bool ns = rawPart2;
                float3 c = cellPhysFalloffPart;
                v += (c * max(dot(N, L), 0.0f));
            }
        }
    }
    return pow(clamp(v, 0.0f.xxx, 1.0f.xxx), 0.4545454680919647216796875f.xxx);
}
#endif

#if defined(WW_FN_cellSpecGame) || defined(WW_FN_ALL)
float cellSpecGame(float3 N, float3 L, float3 V, float gloss)
{
    float n = exp2((gloss * 10.0f) + 1.0f);
    float3 H = normalize(V + L);
    float NdotL = clamp(dot(N, L), 0.0f, 1.0f);
    float NdotV = clamp(dot(N, V), 0.0f, 1.0f);
    float VdotH = clamp(dot(V, H), 0.0f, 1.0f);
    float NdotH = clamp(dot(N, H), 0.0f, 1.0f);
    float D_1 = (pow(NdotH, n) * (n + 2.0f)) * 0.15915499627590179443359375f;
    float m = min(NdotL, NdotV);
    float cPart2;
    if (VdotH >= ((2.0f * NdotH) * m))
    {
        float cPart;
        if (NdotV == m)
        {
            cPart = 1.0f;
        }
        else
        {
            cPart = NdotL / max(NdotV, 9.9999997473787516355514526367188e-05f);
        }
        cPart2 = ((2.0f * NdotH) * cPart) / max(VdotH, 9.9999997473787516355514526367188e-05f);
    }
    else
    {
        cPart2 = 1.0f / max(NdotV, 9.9999997473787516355514526367188e-05f);
    }
    float G = cPart2;
    float f1 = 1.0f - VdotH;
    float f4 = ((f1 * f1) * f1) * f1;
    float F = min(((1.0f - (f1 * f4)) * 0.20000000298023223876953125f) + (f4 * f1), 1.0f);
    return min(((D_1 * G) * F) * 0.25f, 15.0f) * 3.14159297943115234375f;
}
#endif

#if defined(WW_FN_cellSpecGame_V2) || defined(WW_FN_ALL)
float cellSpecGame(float3 N, float3 L, float3 V, float gloss)
{
    float n = exp2((gloss * 10.0f) + 1.0f);
    float3 H = normalize(V + L);
    float NdotL = clamp(dot(N, L), 0.0f, 1.0f);
    float NdotV = clamp(dot(N, V), 0.0f, 1.0f);
    float VdotH = clamp(dot(V, H), 0.0f, 1.0f);
    float NdotH = clamp(dot(N, H), 0.0f, 1.0f);
    float D = (pow(NdotH, n) * (n + 2.0f)) * 0.15915499627590179443359375f;
    float m = min(NdotL, NdotV);
    float cPart2;
    if (VdotH >= ((2.0f * NdotH) * m))
    {
        float cPart;
        if (NdotV == m)
        {
            cPart = 1.0f;
        }
        else
        {
            cPart = NdotL / max(NdotV, 9.9999997473787516355514526367188e-05f);
        }
        cPart2 = ((2.0f * NdotH) * cPart) / max(VdotH, 9.9999997473787516355514526367188e-05f);
    }
    else
    {
        cPart2 = 1.0f / max(NdotV, 9.9999997473787516355514526367188e-05f);
    }
    float G = cPart2;
    float f1 = 1.0f - VdotH;
    float f4 = ((f1 * f1) * f1) * f1;
    float F = min(((1.0f - (f1 * f4)) * 0.20000000298023223876953125f) + (f4 * f1), 1.0f);
    return min(((D * G) * F) * 0.25f, 15.0f) * 3.14159297943115234375f;
}
#endif

#if defined(WW_FN_cellOren) || defined(WW_FN_ALL)
float cellOren(float3 N, float3 L, float3 V, float NdotL, float gloss)
{
    if ((cellRed & 8) != 0)
    {
        return 1.0f;
    }
    float s2 = (1.0f - gloss) * (1.0f - gloss);
    float A_1 = 1.0f - ((0.5f * s2) / (s2 + 0.569999992847442626953125f));
    float B = (0.449999988079071044921875f * s2) / (s2 + 0.0900000035762786865234375f);
    float NdotV = dot(N, V);
    float3 tV = V - (N * NdotV);
    float3 tL = L - (N * NdotL);
    float cosPhi = dot(tV, tL);
    if ((cellRed & 16) != 0)
    {
        cosPhi /= max(length(tV) * length(tL), 9.9999999747524270787835121154785e-07f);
    }
    float geom = sqrt(clamp((1.0f - (NdotL * NdotL)) * (1.0f - (NdotV * NdotV)), 0.0f, 1.0f)) / max(max(NdotL, NdotV), 9.9999997473787516355514526367188e-05f);
    return ((max(cosPhi, 0.0f) * B) * geom) + A_1;
}
#endif

#if defined(WW_FN_cellOren_V2) || defined(WW_FN_ALL)
float cellOren(float3 N, float3 L, float3 V, float NdotL, float gloss)
{
    if ((cellRed & 8) != 0)
    {
        return 1.0f;
    }
    float s2 = (1.0f - gloss) * (1.0f - gloss);
    float A = 1.0f - ((0.5f * s2) / (s2 + 0.569999992847442626953125f));
    float B = (0.449999988079071044921875f * s2) / (s2 + 0.0900000035762786865234375f);
    float NdotV = dot(N, V);
    float3 tV = V - (N * NdotV);
    float3 tL = L - (N * NdotL);
    float cosPhi = dot(tV, tL);
    if ((cellRed & 16) != 0)
    {
        cosPhi /= max(length(tV) * length(tL), 9.9999999747524270787835121154785e-07f);
    }
    float geom = sqrt(clamp((1.0f - (NdotL * NdotL)) * (1.0f - (NdotV * NdotV)), 0.0f, 1.0f)) / max(max(NdotL, NdotV), 9.9999997473787516355514526367188e-05f);
    return ((max(cosPhi, 0.0f) * B) * geom) + A;
}
#endif

#if defined(WW_FN_cellRim) || defined(WW_FN_ALL)
float cellRim(float3 N, float3 L, float3 V, float gloss)
{
    if ((cellRed & 40) != 0)
    {
        return 0.0f;
    }
    float edge = pow(clamp(1.0f - dot(N, V), 0.0f, 1.0f), 0.00999999977648258209228515625f);
    return (clamp(dot(V, -L), 0.0f, 1.0f) * edge) * (1.0f - gloss);
}
#endif

#if defined(WW_FN_cellSumLights) || defined(WW_FN_ALL)
void cellSumLights(float3 P, float3 N, float3 Vw, float gloss, inout float3 diff, inout float3 diffOn, inout float3 spec)
{
    diff = 0.0f.xxx;
    diffOn = 0.0f.xxx;
    cellRimSum = 0.0f.xxx;
    spec = 0.0f.xxx;
    float cPart2;
    if (cellFarFade)
    {
        float3 cPart = P;
        cPart2 = cellFarW(cPart);
    }
    else
    {
        cPart2 = 0.0f;
    }
    // the real lights fade out over the far band
    float fw = cPart2;
    if (fw >= 1.0f)
    {
        return;
    }
    float3 cond = P;
    int cond2;
    int condPart = cellLightsBegin(cond, cond2);
    int nL = cond2;
    int bL = condPart;
    float vAPart2;
    float vAPart;
    for (int k = 0; k < nL; k++)
    {
        int test = bL;
        int x0Pick = k;
        int i = cellLightAt(test, x0Pick);
        float4 t0 = cellLights.Load(i * 8);
        float3 Lv = t0.xyz - P;
        float d = length(Lv);
        if (d >= t0.w)
        {
            continue;
        }
        float3 L = Lv / max(d, 0.001000000047497451305389404296875f).xxx;
        float NdotL = dot(N, L);
        if (NdotL <= 0.0f)
        {
            continue;
        }
        float4 t1 = cellLights.Load((i * 8) + 1);
        int wPick = i;
        float3 cellPhysFalloffRet2 = P;
        float3 rawPart2 = t0.xyz;
        float rawPart = t1.w;
        if (!cellShapeIn(wPick, cellPhysFalloffRet2, rawPart2, rawPart))
        {
            continue;
        }
        float4 t3 = cellLights.Load((i * 8) + 3);
        float kPick = d;
        float kPick2 = t0.w;
        float3 cellPhysFalloffRet = t3.xyz;
        float a = cellRadial(kPick, kPick2, cellPhysFalloffRet);
        // lane LIGHTS5 (GOBO1)
        float3 g = 1.0f.xxx;
        float3 gAx = 0.0f.xxx;
        if (t1.w > (-1.5f))
        {
            // PRTP2 section 2: base = saturate(1 - (1 - dot(-L, dir)) / (1 - cosOuter)), cone = min(base^falloff, 1)
            float4 t2 = cellLights.Load((i * 8) + 2);
            float base = clamp(1.0f - ((1.0f - dot(-L, t2.xyz)) / max(1.0f - t1.w, 9.9999997473787516355514526367188e-05f)), 0.0f, 1.0f);
            a *= min(pow(base, max(t2.w, 0.001000000047497451305389404296875f)), 1.0f);
            gAx = t2.xyz;
        }
        if (a > 0.0f)
        {
            int cellPhysFalloffPart2 = i;
            float3 cellPhysFalloffPart = -Lv;
            float3 iPart2 = gAx;
            bool iPart = t1.w > (-1.5f);
            // GOBO2
            g = cellGobo(cellPhysFalloffPart2, cellPhysFalloffPart, iPart2, iPart);
        }
        if (a > 0.0f)
        {
            int cellLightEPart3 = i;
            float3 cellLightEPart2 = P;
            float3 cellLightEPart = N;
            int cellLightERet = i;
            float3 EPart2 = P;
            // lane SHADOW1; GAPLOOK1 (C8)
            a *= (cellShadowF(cellLightEPart3, cellLightEPart2, cellLightEPart) * cellContactLocal(cellLightERet, EPart2));
        }
        float3 E = ((t1.xyz * g) * a) * NdotL;
        diff += E;
        // No Rim Lighting (2) drops the back-light; Ignore Roughness (4) also takes Lambert's diffuse
        // WW_CELL_LIT_RED=rimflags
        int fl = int(t3.w + 0.5f) & (((cellRed & 64) != 0) ? 1 : 7);
        if ((fl & 6) != 0)
        {
            vAPart2 = 0.0f;
        }
        else
        {
            float3 EPart = N;
            float3 vAPart10 = L;
            float3 vAPart9 = Vw;
            float vAPart8 = gloss;
            vAPart2 = cellRim(EPart, vAPart10, vAPart9, vAPart8);
        }
        float rim = vAPart2;
        if ((fl & 4) != 0)
        {
            vAPart = 1.0f;
        }
        else
        {
            float3 vAPart7 = N;
            float3 vAPart6 = L;
            float3 vAPart5 = Vw;
            float vAPart4 = NdotL;
            float vAPart3 = gloss;
            vAPart = cellOren(vAPart7, vAPart6, vAPart5, vAPart4, vAPart3);
        }
        diffOn += (E * (vAPart + rim));
        cellRimSum += (E * rim);
        bool cond4 = (fl & 1) == 0;
        bool cond3;
        if (!cond4)
        {
            cond3 = (cellRed & 65536) != 0;
        }
        else
        {
            cond3 = cond4;
        }
        if (cond3)
        {
            float3 specPart4 = N;
            float3 specPart3 = L;
            float3 specPart2 = Vw;
            float specPart = gloss;
            spec += (E * cellSpecGame(specPart4, specPart3, specPart2, specPart));
        }
    }
    if (fw > 0.0f)
    {
        diff *= (1.0f - fw);
        diffOn *= (1.0f - fw);
        cellRimSum *= (1.0f - fw);
        spec *= (1.0f - fw);
    }
}
#endif

#if defined(WW_FN_cellAmboScale) || defined(WW_FN_ALL)
float3 cellAmboScale(float3 P)
{
    for (int i = 0; i < cellAmboCount; i++)
    {
        // linked to a box, it fills the box (not the sphere)
        if (cellAmboK[i].w > 0.5f)
        {
            float4 b0 = cellAmboBox[i * 3];
            float4 b1 = cellAmboBox[(i * 3) + 1];
            float4 b2 = cellAmboBox[(i * 3) + 2];
            float3 k = float3(dot(b0.xyz, P) + b0.w, dot(b1.xyz, P) + b1.w, dot(b2.xyz, P) + b2.w);
            float3 cPart2 = abs(k);
            if (all(bool3(cPart2.x < 1.0f.xxx.x, cPart2.y < 1.0f.xxx.y, cPart2.z < 1.0f.xxx.z)))
            {
                return cellAmboK[i].xyz;
            }
            continue;
        }
        float3 d = P - cellAmbo[i].xyz;
        if (dot(d, d) < (cellAmbo[i].w * cellAmbo[i].w))
        {
            return cellAmboK[i].xyz;
        }
    }
    return 1.0f.xxx;
}
#endif

#if defined(WW_FN_cellAmbientSum) || defined(WW_FN_ALL)
float3 cellAmbientSum(float3 N, float3 P)
{
    float4 n1 = float4(N, 1.0f);
    float3 cPart2 = P;
    return float3(dot(cellDalc[0], n1), dot(cellDalc[1], n1), dot(cellDalc[2], n1)) * cellAmboScale(cPart2);
}
#endif

#if defined(WW_FN_cellAmbient) || defined(WW_FN_ALL)
float3 cellAmbient(float3 N, float3 P)
{
    float3 cPart2 = N;
    float3 cPart = P;
    return pow(max(cellAmbientSum(cPart2, cPart), 0.0f.xxx), 2.2000000476837158203125f.xxx);
}
#endif

#if defined(WW_FN_cellAmbient_V2) || defined(WW_FN_ALL)
float3 cellAmbient(float3 N, float3 P)
{
    float3 cPart2 = N;
    float3 cPart = P;
    return pow(max(cellAmbientSum(cPart2, cPart), 0.0f.xxx), 2.2000000476837158203125f.xxx) * fxAmbShare;
}
#endif

#if defined(WW_FN_cellAmbGi) || defined(WW_FN_ALL)
float3 cellAmbGi(float3 amb, float3 gi, float3 P, float3 N)
{
    if (cellGiAmb == 2)
    {
        return 0.0f.xxx;
    }
    bool wwh4 = (cellGiAmb == 0);
    if (or(wwh4, true))   // HLSLFIX2 orfix.py: no short-circuit branch (NV took the wrong side)
    {
        return amb;
    }
    if (cellGiAmb == 1)
    {
        float3 cPart2 = P;
        float3 cPart = N;
        return amb * (1.0f - clamp(cellGiSample(cPart2, cPart).w, 0.0f, 1.0f));
    }
    if (cellGiAmb == 3)
    {
        return max(amb - gi, 0.0f.xxx);
    }
    return gi;
}
#endif

#if defined(WW_FN_cellCubeGameOn) || defined(WW_FN_ALL)
bool cellCubeGameOn()
{
    bool cPart2 = abs(cellCubeMat.z - 1.0f) < 0.5f;
    bool cPart;
    if (!cPart2)
    {
        cPart = abs(cellCubeMat.z - 3.0f) < 0.5f;
    }
    else
    {
        cPart = cPart2;
    }
    return cPart;
}
#endif

#if defined(WW_FN_cellCubeGame) || defined(WW_FN_ALL)
float3 cellCubeGame(TextureCube<float4> cube, SamplerState _cube_sampler, float3 dir, float gloss, float spec, float envScale, float3 posView)
{
    if (!cellCubeGameOn())
    {
        return 0.0f.xxx;
    }
    float g = clamp(gloss, 0.0f, 1.0f);
    // view units -> game units
    float depth = abs(posView.z) * length(cellRow[0].xyz);
    float lod = (((1.0f - g) * 6.0f) + (depth * 0.00195299996994435787200927734375f)) + log2(float(int2(spvTextureSize(cube, uint(0), dummy_parameter)).x) / 128.0f);
    float3 c = cube.SampleLevel(_cube_sampler, dir, max(lod, 0.0f)).xyz;
    // an untagged cube: the game's sampler decodes sRGB before the filter, this after
    if (cellCubeMat.z < 2.0f)
    {
        c = lerp(c / 12.9200000762939453125f.xxx, pow((c + 0.054999999701976776123046875f.xxx) / 1.05499994754791259765625f.xxx, 2.400000095367431640625f.xxx), step(0.040449999272823333740234375f.xxx, c));
    }
    float k = ((3.0f * clamp(spec, 0.0f, 1.0f)) * min(sqrt(clamp(g - 0.300000011920928955078125f, 0.0f, 1.0f)), 1.0f)) * clamp(envScale, 0.0f, 50.0f);
    return c * k;
}
#endif

#if defined(WW_FN_cellCubeProbe) || defined(WW_FN_ALL)
float3 cellCubeProbe(float2 uv, float3 normalView)
{
    float2 f = clamp(floor((frac(uv) * 65535.0f) + 0.5f.xx), 0.0f.xx, 65535.0f.xx);
    float tag = cellCubeMat.w / 255.0f;
    if (cellProbe == 51)
    {
        return float3(floor(f.x / 256.0f) / 255.0f, mod(f.x, 256.0f) / 255.0f, tag);
    }
    if (cellProbe == 52)
    {
        return float3(floor(f.y / 256.0f) / 255.0f, mod(f.y, 256.0f) / 255.0f, tag);
    }
    float3 cPart2 = normalView;
    float3 n = ((cellWorldDir(cPart2) * 0.5f) + 0.5f.xxx) * 255.0f;
    return (n - floor(n + 0.5f.xxx)) + 0.5f.xxx;
}
#endif

#if defined(WW_FN_cellLit) || defined(WW_FN_ALL)
float3 cellLit(float3 color, float3 albedo, float3 normalView, float3 posView, float3 Vview, float specMask, float3 specCol, float alphaR, float kSmith, float3 emissive, float3 cubeK)
{
    float3 cPart2 = posView;
    float3 P = cellWorldPos(cPart2);
    float3 cPart = normalView;
    float3 N = cellWorldDir(cPart);
    float3 cond = Vview;
    float3 Vw = cellWorldDir(cond);
    float gloss = 1.0f - sqrt(alphaR);
    float3 cond2 = P;
    float3 condPart = N;
    float3 vAPart2 = Vw;
    float vAPart = gloss;
    float3 test;
    float3 x0Pick;
    float3 wPick;
    cellSumLights(cond2, condPart, vAPart2, vAPart, test, x0Pick, wPick);
    float3 diff = test;
    float3 diffOn = x0Pick;
    float3 spec = wPick;
    float3 alb = albedo * albedo;
    float3 cellPhysFalloffRet2;
    if (false)
    {
        float3 rawPart2 = P;
        float3 rawPart = N;
        cellPhysFalloffRet2 = cellGiE(rawPart2, rawPart);
    }
    else
    {
        cellPhysFalloffRet2 = 0.0f.xxx;
    }
    float3 kPick = cellPhysFalloffRet2;
    float3 gi = cellSsgiE(kPick) * 0.31830990314483642578125f;
    float3 kPick2 = P;
    float3 cellPhysFalloffRet = N;
    float3 cellPhysFalloffPart2;
    float3 cellPhysFalloffPart;
    float iPart2 = cellFarTerms(kPick2, cellPhysFalloffRet, cellPhysFalloffPart2, cellPhysFalloffPart);
    float3 farF = cellPhysFalloffPart2;
    float3 farGP = cellPhysFalloffPart;
    float farW = iPart2;
    if (farW > 0.0f)
    {
        gi = max(gi - (farGP * farW), 0.0f.xxx);
        diff += (farF * farW);
        diffOn += (farF * farW);
    }
    float3 add = (alb * (diffOn + gi)) + ((spec * specMask) * specCol);
    if ((!cellInterior) && (!cellHasDir))
    {
        return sqrt(((color * color) + add) + cellReflAdd);
    }
    if (!cellInterior)
    {
        float3 Ld = normalize(cellDirTo);
        float nl = dot(N, Ld);
        float3 dir = (cellDirColor * max(nl, 0.0f)) * cellDirVis;
        float3 airDif = cellDirColor * (cellAirDif * (0.5f + (0.5f * N.z)));
        float3 Ex = (((diff + gi) + dir) + airDif) + cellExtAmb;
        float3 iPart = N;
        float3 cellLightEPart3 = Ld;
        float3 cellLightEPart2 = Vw;
        float cellLightEPart = nl;
        float cellLightERet = gloss;
        float3 EPart2 = N;
        float3 EPart = Ld;
        float3 vAPart10 = Vw;
        float vAPart9 = gloss;
        float3 Edx = (((diffOn + gi) + airDif) + (dir * (cellOren(iPart, cellLightEPart3, cellLightEPart2, cellLightEPart, cellLightERet) + cellRim(EPart2, EPart, vAPart10, vAPart9)))) + (cellDirColor * (cellBackK * cellDirVis));
        float3 ambDim = 1.0f.xxx;
        float specDim = 1.0f;
        float3 vAPart8;
        if ((cellRed & 512) != 0)
        {
            vAPart8 = Ex;
        }
        else
        {
            vAPart8 = Edx + cellExtAmb;
        }
        float3 linx = ((((((color * color) * ambDim) + (alb * Edx)) + ((spec * specMask) * specCol)) + cellExtSpec) + ((cubeK * specDim) * vAPart8)) + cellReflAdd;
        return sqrt(max(linx, 0.0f.xxx)) + emissive;
    }
    float3 E = diff + gi;
    float3 Ed = diffOn + gi;
    if (cellHasDalc)
    {
        float3 vAPart7 = N;
        float3 vAPart6 = P;
        float3 amb = cellAmbient(vAPart7, vAPart6);
        if (cellGiAmb != 0)
        {
            float3 vAPart5 = amb;
            float3 vAPart4 = gi;
            float3 vAPart3 = P;
            float3 cond4 = N;
            amb = cellAmbGi(vAPart5, vAPart4, vAPart3, cond4);
        }
        E += amb;
        Ed += amb;
    }
    if (cellHasDir)
    {
        float3 Ld_1 = normalize(cellDirTo);
        float nl_1 = dot(N, Ld_1);
        float3 dir_1 = cellDirColor * max(nl_1, 0.0f);
        E += dir_1;
        float3 cond3 = N;
        float3 specPart4 = Ld_1;
        float3 specPart3 = Vw;
        float specPart2 = nl_1;
        float specPart = gloss;
        float3 EdPart4 = N;
        float3 EdPart3 = Ld_1;
        float3 EdPart2 = Vw;
        float EdPart = gloss;
        Ed += ((dir_1 * (cellOren(cond3, specPart4, specPart3, specPart2, specPart) + cellRim(EdPart4, EdPart3, EdPart2, EdPart))) + (cellDirColor * cellBackK));
    }
    bool3 linPart = ((cellRed & 512) != 0).xxx;
    float3 lin = (((alb * Ed) + ((spec * specMask) * specCol)) + (cubeK * float3(linPart.x ? E.x : Ed.x, linPart.y ? E.y : Ed.y, linPart.z ? E.z : Ed.z))) + cellReflAdd;
    return sqrt(max(lin, 0.0f.xxx)) + emissive;
}
#endif

#if defined(WW_FN_cellLit_V2) || defined(WW_FN_ALL)
float3 cellLit(float3 color, float3 albedo, float3 normalView, float3 posView, float3 Vview, float specMask, float3 specCol, float alphaR, float kSmith, float3 emissive, float3 cubeK)
{
    float3 cPart2 = posView;
    float3 P = cellWorldPos(cPart2);
    float3 cPart = normalView;
    float3 N = cellWorldDir(cPart);
    float3 cond = Vview;
    float3 Vw = cellWorldDir(cond);
    float gloss = 1.0f - sqrt(alphaR);
    float3 cond2 = P;
    float3 condPart = N;
    float3 vAPart2 = Vw;
    float vAPart = gloss;
    float3 test;
    float3 x0Pick;
    float3 wPick;
    cellSumLights(cond2, condPart, vAPart2, vAPart, test, x0Pick, wPick);
    float3 diff = test;
    float3 diffOn = x0Pick;
    float3 spec = wPick;
    float3 alb = albedo * albedo;
    float3 cellPhysFalloffRet2;
    if (false)
    {
        float3 rawPart2 = P;
        float3 rawPart = N;
        cellPhysFalloffRet2 = cellGiE(rawPart2, rawPart);
    }
    else
    {
        cellPhysFalloffRet2 = 0.0f.xxx;
    }
    float3 kPick = cellPhysFalloffRet2;
    float3 gi = cellSsgiE(kPick) * 0.31830990314483642578125f;
    float3 kPick2 = P;
    float3 cellPhysFalloffRet = N;
    float3 cellPhysFalloffPart2;
    float3 cellPhysFalloffPart;
    float iPart2 = cellFarTerms(kPick2, cellPhysFalloffRet, cellPhysFalloffPart2, cellPhysFalloffPart);
    float3 farF = cellPhysFalloffPart2;
    float3 farGP = cellPhysFalloffPart;
    float farW = iPart2;
    if (farW > 0.0f)
    {
        gi = max(gi - (farGP * farW), 0.0f.xxx);
        diff += (farF * farW);
        diffOn += (farF * farW);
    }
    float3 add = (alb * (diffOn + gi)) + ((spec * specMask) * specCol);
    if ((!cellInterior) && (!cellHasDir))
    {
        return sqrt(((color * color) + add) + cellReflAdd);
    }
    if (!cellInterior)
    {
        float3 Ld = normalize(cellDirTo);
        float nl = dot(N, Ld);
        float3 dir = (cellDirColor * max(nl, 0.0f)) * cellDirVis;
        float3 airDif = cellDirColor * (cellAirDif * (0.5f + (0.5f * N.z)));
        float3 Ex = (((diff + gi) + dir) + airDif) + cellExtAmb;
        float3 iPart = N;
        float3 cellLightEPart3 = Ld;
        float3 cellLightEPart2 = Vw;
        float cellLightEPart = nl;
        float cellLightERet = gloss;
        float3 EPart2 = N;
        float3 EPart = Ld;
        float3 vAPart10 = Vw;
        float vAPart9 = gloss;
        float3 Edx = (((diffOn + gi) + airDif) + (dir * (cellOren(iPart, cellLightEPart3, cellLightEPart2, cellLightEPart, cellLightERet) + cellRim(EPart2, EPart, vAPart10, vAPart9)))) + (cellDirColor * (cellBackK * cellDirVis));
        float3 ambDim = 1.0f.xxx;
        float specDim = 1.0f;
        float3 vAPart8;
        float vAPart7;
        ussTerrainDim(vAPart8, vAPart7);
        ambDim = vAPart8;
        specDim = vAPart7;
        float3 vAPart6;
        if ((cellRed & 512) != 0)
        {
            vAPart6 = Ex;
        }
        else
        {
            vAPart6 = Edx + cellExtAmb;
        }
        float3 linx = ((((((color * color) * ambDim) + (alb * Edx)) + ((spec * specMask) * specCol)) + cellExtSpec) + ((cubeK * specDim) * vAPart6)) + cellReflAdd;
        return sqrt(max(linx, 0.0f.xxx)) + emissive;
    }
    float3 E = diff + gi;
    float3 Ed = diffOn + gi;
    if (cellHasDalc)
    {
        float3 vAPart5 = N;
        float3 vAPart4 = P;
        float3 amb = cellAmbient(vAPart5, vAPart4);
        if (cellGiAmb != 0)
        {
            float3 vAPart3 = amb;
            float3 cond4 = gi;
            float3 cond3 = P;
            float3 specPart4 = N;
            amb = cellAmbGi(vAPart3, cond4, cond3, specPart4);
        }
        E += amb;
        Ed += amb;
    }
    if (cellHasDir)
    {
        float3 Ld_1 = normalize(cellDirTo);
        float nl_1 = dot(N, Ld_1);
        float3 dir_1 = cellDirColor * max(nl_1, 0.0f);
        E += dir_1;
        float3 specPart3 = N;
        float3 specPart2 = Ld_1;
        float3 specPart = Vw;
        float EdPart4 = nl_1;
        float EdPart3 = gloss;
        float3 EdPart2 = N;
        float3 EdPart = Ld_1;
        float3 linPart = Vw;
        float EdPart5 = gloss;
        Ed += ((dir_1 * (cellOren(specPart3, specPart2, specPart, EdPart4, EdPart3) + cellRim(EdPart2, EdPart, linPart, EdPart5))) + (cellDirColor * cellBackK));
    }
    bool3 linPart2 = ((cellRed & 512) != 0).xxx;
    float3 lin = (((alb * Ed) + ((spec * specMask) * specCol)) + (cubeK * float3(linPart2.x ? E.x : Ed.x, linPart2.y ? E.y : Ed.y, linPart2.z ? E.z : Ed.z))) + cellReflAdd;
    return sqrt(max(lin, 0.0f.xxx)) + emissive;
}
#endif

#if defined(WW_FN_cellProbeOut) || defined(WW_FN_ALL)
float3 cellProbeOut(float3 normalView, float3 posView, float alphaR, float kSmith)
{
    float3 cPart2 = posView;
    float3 P = cellWorldPos(cPart2);
    float3 cPart = normalView;
    float3 N = cellWorldDir(cPart);
    // the lit effects' probes: a surface writes black
    if ((cellProbe >= 70) && (cellProbe <= 74))
    {
        return 0.0f.xxx;
    }
    // lane SSGI2: as cellProbeRaw's
    if (cellProbe == 5)
    {
        float3 cond;
        if (false)
        {
            float3 cond2 = P;
            float3 condPart = N;
            cond = cellGiE(cond2, condPart);
        }
        else
        {
            cond = 0.0f.xxx;
        }
        float3 vAPart2 = cond;
        return clamp(cellSsgiE(vAPart2) * 0.31830990314483642578125f, 0.0f.xxx, 1.0f.xxx);
    }
    // the share of the weather's ambient the grid's sky replaced (indoors the DALC's)
    if (cellProbe == 90)
    {
        float3 vAPart = P;
        float3 test = N;
        return cellAmbShare(vAPart, test).xxx;
    }
    // the branch cellGiSample took, its .a and its E (cellProbeRaw's)
    if (cellProbe == 95)
    {
        float3 x0Pick = P;
        float3 wPick = N;
        float3 cellPhysFalloffRet2 = cellProbeRaw(x0Pick, wPick);
        return cellPhysFalloffRet2;
    }
    if ((cellProbe >= 91) && (cellProbe <= 94))
    {
        float3 rawPart2 = P;
        float3 rawPart = N;
        float3 kPick = cellFarProbeOut(rawPart2, rawPart);
        return kPick;
    }
    bool wwh5 = ((((cellProbe == 1) || (cellProbe == 8)) || (cellProbe == 10)) || (cellProbe == 30));
    bool wwh6 = (cellProbe == 31);
    if (or(wwh5, wwh6))   // HLSLFIX2 orfix.py: no short-circuit branch (NV took the wrong side)
    {
        float3 kPick2;
        if (cellProbe == 1)
        {
            kPick2 = N;
        }
        else
        {
            float3 cellPhysFalloffRet = -posView;
            kPick2 = cellWorldDir(cellPhysFalloffRet);
        }
        float3 cellPhysFalloffPart2 = P;
        float3 cellPhysFalloffPart = N;
        float3 iPart2 = kPick2;
        float iPart = 1.0f - sqrt(alphaR);
        float3 cellLightEPart3;
        float3 cellLightEPart2;
        float3 cellLightEPart;
        cellSumLights(cellPhysFalloffPart2, cellPhysFalloffPart, iPart2, iPart, cellLightEPart3, cellLightEPart2, cellLightEPart);
        float3 diff = cellLightEPart3;
        float3 diffOn = cellLightEPart2;
        float3 spec = cellLightEPart;
        if (cellProbe == 10)
        {
            return clamp(cellRimSum * 4.0f, 0.0f.xxx, 1.0f.xxx);
        }
        // the placed lights' specular / 4, before the mask
        if (cellProbe == 30)
        {
            return clamp(spec * 0.25f, 0.0f.xxx, 1.0f.xxx);
        }
        // lane GAPFIX1: the same after the colour cellLit puts on it (deferred = white)
        if (cellProbe == 31)
        {
            return clamp((spec * cellSpecTint) * 0.25f, 0.0f.xxx, 1.0f.xxx);
        }
        bool3 cellLightERet = (cellProbe == 8).xxx;
        return clamp(float3(cellLightERet.x ? diffOn.x : diff.x, cellLightERet.y ? diffOn.y : diff.y, cellLightERet.z ? diffOn.z : diff.z) * 0.25f, 0.0f.xxx, 1.0f.xxx);
    }
    if (cellProbe == 9)
    {
        // the gloss the diffuse used
        return float3(1.0f - sqrt(alphaR), 0.0f, 0.0f);
    }
    // the interior ambient's affine sum before its 2.2, x 8 (it is dim)
    if (cellProbe == 11)
    {
        float3 EPart2;
        if (cellHasDalc)
        {
            float3 EPart = N;
            float3 vAPart10 = P;
            EPart2 = clamp(cellAmbientSum(EPart, vAPart10) * 8.0f, 0.0f.xxx, 1.0f.xxx);
        }
        else
        {
            EPart2 = 0.0f.xxx;
        }
        return EPart2;
    }
    if (cellProbe == 7)
    {
        float3 vAPart9 = P;
        float3 vAPart8 = N;
        return cellShadowProbe(vAPart9, vAPart8);
    }
    // the position, 16 bits an axis over the 65536-unit box around the center
    float3 q = clamp(floor((P - cellCenter) + 32768.0f.xxx), 0.0f.xxx, 65535.0f.xxx);
    if (cellProbe == 2)
    {
        return floor(q / 256.0f.xxx) / 255.0f.xxx;
    }
    if (cellProbe == 3)
    {
        return mod(q, 256.0f.xxx) / 255.0f.xxx;
    }
    return (N * 0.5f) + 0.5f.xxx;
}
#endif
#endif	// WW_HLSL_SRC
