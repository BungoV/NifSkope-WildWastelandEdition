/* celllights_p.h: the cell light law's private state, shared by the hub (gl/celllights.cpp) and the lamps
 * (features/lights/celllamps.cpp). Not an interface: only those two include it, after their own includes. */
#pragma once

namespace WwCellLightsP
{

constexpr int kTextureUnit = WwCellUnit::Lights;		// TexCache allocates from unit 0 upward; 15 is the CSM map
constexpr int kTexelsPerLight = 8;		// lane SHADOW1 added the 5th: shadow slot, kind, near clip, XLIG bias;
										// lane HEMI1 the 6th to 8th: the box rows (cell_lights.glsl CELL_TPL)
constexpr int kGiUnit = WwCellUnit::Gi;			// the bounce grid (sampler3D)
constexpr int kGiSlotUnit = WwCellUnit::GiSlot;		// the grid's slot rooms, the fine rooms (sampler3D, R32F); off
constexpr int kRoomUnit = WwCellUnit::Room;		// on a GPU with fewer than 20 units (the blend before the lane)
constexpr int kFarSlotUnit = WwCellUnit::FarSlot;	// the far table's slots (usamplerBuffer), records (samplerBuffer) and the
constexpr int kFarRecUnit = WwCellUnit::FarRec;		// GI grid's placed share (sampler3D); off on a GPU without FarPlaced (cellunits.h)
constexpr int kFarPlacedUnit = WwCellUnit::FarPlaced;
constexpr int kLutUnit = WwCellUnit::Lut;		// the imagespace LUT (sampler3D)
constexpr int kBloomUnit = WwCellUnit::Bloom;		// the imagespace bloom (sampler2D, a quarter of the view)
constexpr int kShadowUnit = WwCellUnit::Shadow;		// the lights' depth cubes (samplerCubeArrayShadow)
constexpr int kShadowSlots = 16;	// shadowed lights at once, the nearest the camera
constexpr int kShadowFace = 512;	// texels a cube face edge (the game's 2048 paraboloid's centre texel at 1024)

// the budget's distance fade, Godot's Light3D shadow fade (40 to 50 m) at 70 units a metre
constexpr float kShadowFadeBegin = 2800.0f, kShadowFadeEnd = 3500.0f;
constexpr int kAmbientSlots = 16;	// Ambient Only volumes a cell (Fallout4.esm's most in one cell: 5, measured)

//! one bloom per document: the blurred bright pass, rgb floats, w x h (rows bottom-up, as read back)
struct ClBloom
{
	int w = 0, h = 0;
	std::vector<float> rgb;
	int version = 0;
	double peak = 0.0;      // the brightest bloom texel's max channel (the echo)
	int lit = 0;            // texels the bright pass passed (the echo)
	unsigned gpuTex = 0;    // lane PERFFIX1: blurred on the GPU (cellhdr.cpp bloomBlur): sampled as is, rgb not used
};

//! lane NIFXLIGHT1: a document with .nifx fixtures: its lights as built, and whether the shown copy has them applied
struct ClNifx
{
	WwCellLighting base;
	bool applied = false;
	QByteArray key;
	QElapsedTimer clock;
	WwNifxLive live;	// lane NIFXLIGHT3: the folders a placed model's .nifx would sit in, looked at in turn
	bool liveInit = false, ticked = false;
};

struct ClState
{
	bool loaded = false;
	quint64 liveGen = 0;	// lane TESTDRIVER1: the switch generation last read
	bool on = false;
	bool pinned = false;
	int probe = 0;
	int red = 0;            // bit 1 linear, 2 axis, 4 nodalc
	QHash<const void *, WwCellLighting> docs;
	QHash<const void *, ClNifx> nifx;	// lane NIFXLIGHT1
	QHash<const void *, int> nifxStamp;	// lane NIFXLIGHT2: moves on each Mesh lights publish (none: as built)
	int nextNifxStamp = 1;
	QHash<const void *, int> version;
	int nextVersion = 1;
	bool giOn = false;
	QHash<const void *, WwCellGi> gi;
	QHash<const void *, int> giVersion;
	QHash<const void *, int> giFadeVersion;	// lane GIPROBE2: the grid's rgba edited in place (Probe light smoothing)
	bool farOn = false, farPinned = false;
	int dotsLast = -1;	// the dots the last frame drew (-1 none tried)
	int farRed = 0;
	QHash<const void *, WwCellFar> far;
	QHash<const void *, int> farVersion;
	bool isOn = false, isPinned = false, measuring = false;
	int isRed = 0;          // 1 nolut, 2 noexp, 4 nograde, 8 nobloom, 16 nofx
	QHash<const void *, float> adapted;     // the last measure per document
	QHash<const void *, int> adaptedPixels;
	QHash<const void *, ClBloom> bloom;     // the last measure's bloom per document
	int nextBloomVersion = 1;
	bool shadowOn = true;   // WW_CELL_SHADOW=0 turns the maps off
	bool shadowPinned = false;	// the Light shadows row (WW/CellShadow); the pin wins
	bool shadowRed = false; // WW_CELL_SHADOW_RED=noshadow: the maps rendered, every factor read as 1
	QString shadowLast = QStringLiteral( "none yet" );
	QString lampLast = QStringLiteral( "off" );	// lane LAMPSHADOW1: the stored lamp shadows' last frame
	int lampSliced = 0;	// lane PERFFIX2 b: slots whose first store waits (the last frame)
	int fogProbe = 0;       // WW_CELL_FOG_PROBE=6 (alpha, height blend) | 7 (fog color ^ 1/2.2) | 8 (its d, z)
	int pass = 0;           // the PRTP band's Pass (WwCellPass); WW_CELL_PASS pins it
	bool passPinned = false;
	int passRed = 0;        // WW_CELL_PV_RED: 1 direct, 2 nonormal (the shader), 4 open (every sky direction open), 8 noclamp, 16 nosee, 32 borrow, 64 lendamb, 128 strand, 256 firstslot, 512 tiepick, 1024 lonesky, 2048 nofilter, 4096 nolastfill; 8192 WW_CELL_GIWALL_RED=off
	int giAmb = 1;          // WW_CELL_GI_AMB = keep (0) | replace | off | max | asgi (the measure's red); replace by default (his ruling), keep = the gate's red
	bool giFill = true;     // WW_CELL_GI_FILL=0 leaves the GI's gaps black
	// the Light flicker row (WW/CellFlicker, pin WW_CELL_FLICKER), its red and gate pins
	bool flickerOn = false, flickerPinned = false, flickerRed = false;
	float flickerStep = 0.0f;	// > 0: WW_CELL_FLICKER_STEP, seconds a frame
	bool flickerClockRed = false;	// lane PERFFIX2 a: WW_CELL_FLICKER_CLOCK_RED=1, the wall-clock dt per painted frame
	QString flickerDump;
};
ClState & st();

//! one texture buffer per GL context (the renderer), re-uploaded when the published version moves
struct Gpu
{
	GLuint buf = 0, tex = 0;
	bool texAttached = false;	// lane PERF1: the buffer texture keeps its buffer across glBufferData; attached once
	bool bufStored = false;	// lane VKOBJ4: buf is a store (WW_VKOBJ_SHADE: objBlTexelFill; the Vulkan runs read it in place)
	quint64 bufLiveGen = 0;	// lane TESTDRIVER1: a live set (light copies, reds) rebuilds the buffer
	std::uint64_t bufXemiStamp = 0;	// lane LIGHTS4: the XEMI light factors the buffer was built with
	float bufCurve = 1.0f;	// lane LIGHTPHYS1: the intensity curve folded into the colours (menu sets bump no switch generation)
	int bufCurveRed = 0;
	int bufGobo = -1;	// lane LIGHTS5 (GOBO1): the Light patterns row the buffer was built with
	bool goboAny = false;
	int goboTbl = 0;	// lane GOBO2: the texel of the per-light table of omni/box pattern records (0 none)
	int bulbBase = 0, bulbs = 0;	// lane NIFXLIGHT2: the bulb-tier lights' first index (after the lights) and count
	// lane LIGHTS5 (FOGSCALE1): each light's fog.scale (empty = every light 1), re-read at most every half second
	std::vector<float> fogVals;
	quint64 fogStamp = 0, bufFogStamp = 0, fogRecxGen = ~0ULL, fogSwitchGen = ~0ULL;
	int fogBase = -1, fogVersion = -1;
	const void * fogDoc = nullptr;
	QElapsedTimer fogClock;
	const void * doc = nullptr;
	int version = 0;
	int count = 0;
	GLuint giTex = 0;
	const void * giDoc = nullptr;
	int giVersion = 0;
	int giFadeVersion = 0;	// lane GIPROBE2: the in-place rgba edit last uploaded
	GLuint slotTex = 0, roomTex = 0;	// uploaded with the GI grid (the same document and version)
	const void * roomDoc = nullptr;
	int roomVersion = 0, units = -1;
	GLuint skyTex = 0;		// the sky grid, bound on the GI unit in the Sky visibility pass
	const void * skyDoc = nullptr;
	int skyVersion = 0;
	bool giSpecSky = false;	// lane SPECOCC1: the GI texture carries the sky grid's slot 0 after its own slabs
	GLuint airTex = 0;		// SKY2 FOGSLICE1: the air volume (WwCellGi::air), uploaded with the GI grid
	const void * airDoc = nullptr;
	int airVersion = 0;
	GLuint lutTex = 0;
	const void * lutDoc = nullptr;
	int lutVersion = 0;
	int lutStamp = -1;	// the exterior imagespace's stamp the LUT was uploaded at
	GLuint bloomTex = 0;
	GLuint farSlotBuf = 0, farSlotTex = 0, farRecBuf = 0, farRecTex = 0, farPlacedTex = 0;
	GLuint farSlotAtt = 0, farRecAtt = 0;	// lane CELLUNI1: the buffer each far texture holds (kept across glBufferData)
	bool farSlotStored = false, farRecStored = false;	// lane VKOBJ4: stores, as bufStored
	const void * farDoc = nullptr;
	int farVersion = 0;
	const void * bloomDoc = nullptr;
	int bloomVersion = 0;
	// the depth cube array, the light each slot holds (-1 free), and a stamp the light buffer re-uploads on
	GLuint shTex = 0, shFbo = 0;
	bool shShared = false;	// lane VKOBJ4: shTex is a shared cube array on the present device (WW_VKOBJ_SHADE, keyed &shTex)
	const void * shDoc = nullptr;
	int shVersion = 0;
	int shSlot[kShadowSlots];
	int shStamp = 1, bufShStamp = 0;
	int shKick = 0;	// lane DOORLIVE1: a moving caster (a live door) re-renders every held slot
	int shPend[kShadowSlots];	// lane PERFFIX2 b: the light a slot holds whose first store waits for a later frame (-1 none)
	int shFace = 0;	// lane LAMPSHADOW1: the face edge shTex was made at (the tier's)
	float shFade[kShadowSlots];	// lane GAPLOOK1: each slot's shadow strength under the budget (1 without it)
	int shBudget = 0;
	float shCut = 0.0f;
	// lane LAMPSHADOW1: the store (a 2D array in blocks of 3 face layers), its maps by light, the movers
	struct LsEntry
	{
		std::vector<int> blocks;	// omni 2 (faces 0-2, 3-5); wall 1 (the front face, then the side halves two a layer)
		int back = -1;			// the face into the wall (-1 omni)
		float dist = 0.0f;
		std::uint64_t used = 0;
	};
	struct LsSig
	{
		float v[20];
		Vector3 c;
		float rad = 0.0f;
	};
	GLuint lsTex = 0, lsReadFbo = 0;
	int lsFace = 0, lsBlocks = 0;
	std::vector<char> lsBlockUsed;
	QHash<int, LsEntry> lsStore;
	QHash<int, WwLampWall> lsWall;
	QHash<int, LsSig> lsHist;
	QHash<int, char> lsMovers;
	std::vector<int> lsSlotMovers[kShadowSlots];
	const void * lsDoc = nullptr;
	int lsVersion = 0;
	std::uint64_t lsFrame = 0;
	std::vector<float> spheres;	// lane VOLAIR1: every buffer light's world centre + radius (copies included), in order
	std::vector<float> texels;		// the light buffer as uploaded, for the cluster lists
	std::uint64_t texelStamp = 0;
	// lane LIGHTFLICKER1: the buffer before the flicker (base) and as patched (live); the flicker stamp last patched
	std::vector<float> flickBase, flickLive;
	std::uint64_t flickStamp = 0;
	std::uint64_t shWrites = 0;	// lane PERFFIX2 a: shadow cube faces written (drawn or blitted), ever
	bool flickOn = false;
	bool dimOn = false;	// lane NIFXLIGHT3: .nifx lights follow their dimmer on the world clock (the bulbs patched too)
	Gpu()
	{
		std::fill( shSlot, shSlot + kShadowSlots, -1 );
		std::fill( shFade, shFade + kShadowSlots, 1.0f );
		std::fill( shPend, shPend + kShadowSlots, -1 );
	}
};
QHash<const void *, Gpu> & gpus();
const WwCellLighting * isLighting( const void * nif, int * stamp = nullptr );

using FlickPhase = WwFlick::Phase;	// lane PERFFIX2 a: the phase, its tables and random moved to gl/cellflickclock.h
struct FlickDoc
{
	int version = -1;
	int mode = 0;					// lane NIFXLIGHT3: 1 flicker, 2 dimmer tracks, 4 the bulbs flicker too
	int flickN = 0, dimN = 0;
	std::vector<int> idx;			// the flickering lights (NIFXLIGHT3: and dimming ones; lights + j = bulb j)
	std::vector<float> val;			// 4 a light (dimmer, world offset), all lights then the bulbs
	QHash<quint32, FlickPhase> phase;	// by ref, so a streamed republish keeps each light's phase
	std::uint64_t stamp = 1;
	int moving = 0;
	qint64 frames = 0;
};
QHash<const void *, FlickDoc> & flickDocs();
bool flickLive( Scene * scene );

// lane LIGHTS4 (XEMI-LIGHT): per document, the lights with XEMI and this frame's factor (3 a light, all lights)
struct XemiDoc
{
	int version = -1;
	std::vector<int> idx;
	std::vector<float> mul;
	std::uint64_t stamp = 1;
	QByteArray echo;
};
QHash<const void *, XemiDoc> & xemiDocs();
std::uint64_t xemiStampOf( const void * doc );

/* lane LIGHTS5 (GOBO1): a spot's pattern, decoded once by path (at most 128 texels across); a map with only its first
 * channel set drives all three. w 0 = unread. GOBO3: the stored value as read (Todd's treat loads it linear, every
 * vanilla pattern is BC1 unorm); the shader's red WW_CELL_GOBO_GAMMA=2.2 takes it ^2.2 as LIGHTS5 did. */
struct GoboPat
{
	int w = 0, h = 0;
	std::vector<float> rgb;
	float mean[3] = { 1, 1, 1 };
};
const GoboPat & goboPattern( const QString & dataRoot, const QString & path );
void fogScaleTick( Gpu & g, const void * doc, int version, const WwCellLighting & L );
void nifxTick( const void * nif );
void physTick( const void * nif );	// lane PHYSUNITS1: Game Light Brightness live on the placed lamps

}	// namespace WwCellLightsP
