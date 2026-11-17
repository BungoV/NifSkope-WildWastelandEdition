/* Lane VKFAR3 (2026-11-14, plan row G3): the far ground's ring-0 terrain on the present device. fo4_cellcsmfgl (Show > Far
 * ground: Live, shader decals, the default) through VKFAR2's twinRun: fargnd_view_vs (fo4_default's twin) + fargnd_view_ps
 * (fo4_cellcsmfgl's readable source, FOGLEAK + FRAME_UBO), one command a shape. Included after vkwater.inc (its TwinSpec,
 * WaterDraw, twinRun). Gate: tests/vkframe/fargrounddraw.sh. BATCH1114: on the far LOD rings' copy of them (namespaces
 * farlod, farlodrun), as the water has its own (lanes VKWATER3, VKWATER4). */
#include "features/vkframe/fargnd_view_vs.spv.h"  // vkframe_spirv.sh's fxt / fxg lines
#include "features/vkframe/fargnd_view_ps.spv.h"
#include "features/vkframe/fargndp_view_ps.spv.h"  // lane VKGROUND3: mode 2, fo4_cellcsmfgp's twin

namespace
{
namespace farlod
{
// the pixel stage's images (t n -> 64 + n, samplers at + 64) and the vertex stage's farStream (t 0 -> 200), reflected names
const WaterImgName kFargndImg[] = { { 64, "cellLights" }, { 68, "cellIsLut" }, { 69, "cellIsBloom" }, { 70, "cellShadow" },
	{ 72, "cellFarSlots" }, { 73, "cellFarRecs" }, { 74, "cellFarPlaced" }, { 77, "cellCtGbuf" }, { 80, "cellSsgi" }, { 81, "fgGrid" },
	{ 82, "fgLtex" }, { 83, "fgDecal" }, { 84, "fgAtlas" }, { 89, "atApTex" }, { 90, "volTex" }, { 91, "volTile" }, { 92, "volAirTerr" },
	{ 93, "volAirRainMap" }, { 94, "volAirNoise" }, { 95, "csmMap" }, { 96, "ussTerrMap" }, { 97, "ussContactMap" },
	{ 98, "ussCloudMap" }, { 99, "waterInfoIndex" }, { 100, "waterInfoAtlas" }, { 101, "waterInfoBodies" }, { 106, "cellAo" },
	{ 107, "cellSsr" }, { 109, "cellCluster" }, { 110, "rainCover" }, { 111, "landShoreIndex" }, { 112, "landShoreAtlas" },
	{ 114, "BaseMap" }, { 115, "NormalMap" }, { 116, "GlowMap" }, { 117, "BacklightMap" }, { 118, "SpecularMap" },
	{ 119, "GreyscaleMap" }, { 120, "EnvironmentMap" }, { 121, "CubeMap" }, { 122, "RefractionSrc" }, { 123, "LandLodDiff" },
	{ 124, "LandLodOuter" }, { 125, "LandLodNoise" }, { 126, "LandLodDetail" }, { 127, "farSkyMap" }, { 65, "cellGi" },
	{ 66, "cellGiSlots" }, { 67, "cellRooms" }, { 79, "cellAoDecal" }, { 102, "waterFlowIndex" }, { 103, "waterFlowAtlas" },
	{ 200, "farStream" } };
// the integer images (usampler2D): fgGrid (RGBA32UI), the water info, flow and shore lookups (VKFAR14: renumbered to
// the BATCH1114 regen of fargnd_view_ps.spv.h, shore at 111/112, farSkyMap at 127)
const std::uint32_t kFargndInts[] = { 81, 99, 100, 102, 103, 111, 112 };
// the pixel block (kT_fargndPsBytes 7168 since the BATCH1114 regen; 7104 before) ends at 19968: the constants after it, as the rings'
constexpr VkDeviceSize kFargndConstAt = 19968, kFargndHostBytes = 20224;
static_assert( kWaterPsLooseAt + WwVkPresent::kT_fargndPsBytes <= kFargndConstAt
		&& kFargndConstAt + 16 * WwVkPresent::kWaterAttribs <= kFargndHostBytes && WwVkPresent::kT_fargndPsBytes <= WwVkPresent::kTwinPsMax,
	"the far ground's host buffer layout" );
// rings: GL's polygon offset as the depth bias and the zero integer / colour cube views; the pixel stage's blocks b0 global, b1 shadow
const TwinSpec kFargndSpec { "far ground", vkframe_fargnd_view_vs_spv, sizeof( vkframe_fargnd_view_vs_spv ), vkframe_fargnd_view_ps_spv,
	sizeof( vkframe_fargnd_view_ps_spv ), kFargndImg, sizeof( kFargndImg ) / sizeof( kFargndImg[0] ), kFargndInts,
	sizeof( kFargndInts ) / sizeof( kFargndInts[0] ), WwVkPresent::kT_fargndPsBytes, kFargndConstAt, kFargndHostBytes, true, 9, true };

WaterDraw & farGroundD()
{
	static WaterDraw D;
	return D;
}

// lane VKGROUND3: mode 2 (Live, cached pages). Its pixel stage declares mode 1's images less fgDecal / fgAtlas, plus the
// cell table fgCells (RGBA16UI, an integer image) and the BC1 page pools fgPagesN / fgPagesF (spvh_binds of the header)
const WaterImgName kFargndpImg[] = { { 64, "cellLights" }, { 68, "cellIsLut" }, { 69, "cellIsBloom" }, { 70, "cellShadow" },
	{ 72, "cellFarSlots" }, { 73, "cellFarRecs" }, { 74, "cellFarPlaced" }, { 77, "cellCtGbuf" }, { 80, "cellSsgi" }, { 81, "fgGrid" },
	{ 82, "fgLtex" }, { 86, "fgCells" }, { 87, "fgPagesN" }, { 88, "fgPagesF" }, { 89, "atApTex" }, { 90, "volTex" }, { 91, "volTile" },
	{ 92, "volAirTerr" }, { 93, "volAirRainMap" }, { 94, "volAirNoise" }, { 95, "csmMap" }, { 96, "ussTerrMap" }, { 97, "ussContactMap" },
	{ 98, "ussCloudMap" }, { 99, "waterInfoIndex" }, { 100, "waterInfoAtlas" }, { 101, "waterInfoBodies" }, { 106, "cellAo" },
	{ 107, "cellSsr" }, { 109, "cellCluster" }, { 110, "rainCover" }, { 111, "landShoreIndex" }, { 112, "landShoreAtlas" },
	{ 114, "BaseMap" }, { 115, "NormalMap" }, { 116, "GlowMap" }, { 117, "BacklightMap" }, { 118, "SpecularMap" },
	{ 119, "GreyscaleMap" }, { 120, "EnvironmentMap" }, { 121, "CubeMap" }, { 122, "RefractionSrc" }, { 123, "LandLodDiff" },
	{ 124, "LandLodOuter" }, { 125, "LandLodNoise" }, { 126, "LandLodDetail" }, { 127, "farSkyMap" }, { 65, "cellGi" },
	{ 66, "cellGiSlots" }, { 67, "cellRooms" }, { 79, "cellAoDecal" }, { 102, "waterFlowIndex" }, { 103, "waterFlowAtlas" },
	{ 200, "farStream" } };
const std::uint32_t kFargndpInts[] = { 81, 86, 99, 100, 102, 103, 111, 112 };
static_assert( kWaterPsLooseAt + WwVkPresent::kT_fargndpPsBytes <= kFargndConstAt && WwVkPresent::kT_fargndpPsBytes <= WwVkPresent::kTwinPsMax,
	"mode 2's pixel block in the far ground's host buffer layout" );
const TwinSpec kFargndpSpec { "far ground pages", vkframe_fargnd_view_vs_spv, sizeof( vkframe_fargnd_view_vs_spv ),
	vkframe_fargndp_view_ps_spv, sizeof( vkframe_fargndp_view_ps_spv ), kFargndpImg, sizeof( kFargndpImg ) / sizeof( kFargndpImg[0] ),
	kFargndpInts, sizeof( kFargndpInts ) / sizeof( kFargndpInts[0] ), WwVkPresent::kT_fargndpPsBytes, kFargndConstAt, kFargndHostBytes,
	true, 18, true };  // fxWork kind 18 (VKMERGE1: 12 is vkcloud1's cloud shadow; 0-17 taken, 19 VKCLOUD5, 20 VKAIR12)

WaterDraw & farGroundPD()
{
	static WaterDraw D;
	return D;
}
}  // namespace farlod
}  // namespace

namespace WwVkPresent
{
static WaterStat & farGroundGlS()
{
	static WaterStat s;
	return s;
}

// lane VKFLIGHT1: GL draws by reason since the last line (bungo's flight said only the last draw's reason: windows mixed two)
static std::map<std::string, int> & farGroundGlWhys( int mode )
{
	static std::map<std::string, int> m[2];
	return m[mode & 1];
}

#ifndef WW_VKPRESENT_NO_QT
static QString takeFarGroundGlWhys( int mode )
{
	std::map<std::string, int> & m = farGroundGlWhys( mode );
	QString s;
	for ( const auto & [why, n] : m )
		s += ( s.isEmpty() ? QStringLiteral( "; GL by reason: " ) : QStringLiteral( ", " ) ) + QStringLiteral( "%1 x %2" ).arg( n ).arg( QString::fromStdString( why ) );
	m.clear();
	return s;
}
#endif

void farGroundOnGl( const char * why )
{
	WwVkPresent::glGuard( "farGround", 1 );  // lane VKFLIGHT4: the view GL guard
	WaterStat & s = farGroundGlS();
	s.gl++;
	s.glWhy = why ? why : "";
	farGroundGlWhys( 0 )[s.glWhy]++;
}

WaterStat takeFarGroundGlStat()
{
	WaterStat s = farGroundGlS();
	farGroundGlS() = WaterStat();
	return s;
}

std::string farGroundDrawRefusal()
{
	return farlod::farGroundD().broken ? farlod::farGroundD().brokenWhy : farlodrun::twinDrawRefusal();
}

bool farGroundRun( const WaterRun & r, std::string & why )
{
	return farlodrun::twinRun( farlod::farGroundD(), farlod::kFargndSpec, r, why );
}

// lane VKFAR4 (batching): the frame's queued ring-0 tiles (GL's order) until the flush; off: a batch failed (sticky)
struct FgQueue
{
	std::vector<WaterRun> runs;
	int n = 0, lost = 0;
	int v = 0, lostRt = 0;  // lane VKFLIGHT14: the queued runs' twin (0 base, 1 RT, 2 DG), RT / DG draws lost
	int lostLod = 0;        // lane VKFLIGHT18: v 3 / 4 the far LOD rings' RT / DG twins (vkfarlodrt.inc), their draws lost
	bool off = false;
	std::string offWhy;
};
static FgQueue & fgQ()
{
	static FgQueue q;
	return q;
}

bool farGroundRunN( const WaterRun * const * rs, int n, std::string & why )
{
	farlod::WaterDraw & D = farlod::farGroundD();  // BATCH1114: the rings' copy (namespaces farlod, farlodrun)
	for ( int at = 0; at < n; ) {
		int k = 1;
		while ( at + k < n && k < farlod::kTwinBatchMax && farlodrun::twinSameInputs( *rs[at], *rs[at + k] ) )
			k++;
		if ( !farlodrun::twinRunN( D, farlod::kFargndSpec, rs + at, k, why ) )
			return false;
		at += k;
	}
	return true;
}

bool farGroundFlush( std::string & why )
{
	FgQueue & q = fgQ();
	if ( !q.n )
		return true;
	std::vector<const WaterRun *> rs( size_t( q.n ) );
	for ( int i = 0; i < q.n; i++ )
		rs[size_t( i )] = &q.runs[size_t( i )];
	const int n = q.n, v = q.v;
	q.n = 0;
	if ( v >= 3 ? farLodRtRunN( rs.data(), n, v - 2, why ) : v ? farGroundRtRunN( rs.data(), n, v, why ) : farGroundRunN( rs.data(), n, why ) )
		return true;
	( v >= 3 ? q.lostLod : v ? q.lostRt : q.lost ) += n;
	q.off = true;
	q.offWhy = why;
	return false;
}

bool farGroundQueue( const WaterRun & r, std::string & why )
{
	FgQueue & q = fgQ();
	if ( q.off )
		return farGroundRun( r, why );
	if ( q.n && ( q.v != 0 || q.n >= farlod::kTwinBatchMax || !farlodrun::twinSameInputs( q.runs[0], r ) ) && !farGroundFlush( why ) )
		return false;
	if ( const std::string no = farlodrun::twinReadyN( farlod::farGroundD(), farlod::kFargndSpec, r, q.n == 0 ); !no.empty() ) {
		why = no;
		return false;
	}
	if ( q.runs.size() <= size_t( q.n ) )
		q.runs.resize( size_t( q.n ) + 1 );
	q.runs[size_t( q.n )] = r;
	q.v = 0;
	q.n++;
	return true;
}

int farGroundQueued()
{
	return fgQ().n;
}

TwinBatchStat takeFarGroundBatchStat()
{
	farlod::WaterDraw & D = farlod::farGroundD();
	FgQueue & q = fgQ();
	TwinBatchStat s;
	s.crossings = D.crossN;
	s.draws = D.drawsN;
	s.slips = D.slipN;
	s.lost = q.lost;
	s.off = q.off;
	s.offWhy = q.offWhy;
	D.crossN = D.drawsN = D.slipN = 0;
	q.lost = 0;
	return s;
}

FxStat takeFarGroundDrawStat()
{
	return farlodrun::takeTwinStat( farlod::farGroundD() );
}

// lane VKGROUND3: mode 2 (Live, cached pages), one crossing a shape (no batching yet)
static WaterStat & farGroundLiveGlS()
{
	static WaterStat s;
	return s;
}

void farGroundLiveOnGl( const char * why )
{
	WwVkPresent::glGuard( "farGroundLive", 1 );  // lane VKFLIGHT4: the view GL guard
	WaterStat & s = farGroundLiveGlS();
	s.gl++;
	s.glWhy = why ? why : "";
	farGroundGlWhys( 1 )[s.glWhy]++;
}

WaterStat takeFarGroundLiveGlStat()
{
	WaterStat s = farGroundLiveGlS();
	farGroundLiveGlS() = WaterStat();
	return s;
}

std::string farGroundLiveDrawRefusal()
{
	return farlod::farGroundPD().broken ? farlod::farGroundPD().brokenWhy : farlodrun::twinDrawRefusal();
}

bool farGroundLiveRun( const WaterRun & r, std::string & why )
{
	if ( !farGroundFlush( why ) )	// mode 1's queued tiles draw first (GL's order)
		return false;
	return farlodrun::twinRun( farlod::farGroundPD(), farlod::kFargndpSpec, r, why );
}

FxStat takeFarGroundLiveDrawStat()
{
	return farlodrun::takeTwinStat( farlod::farGroundPD() );
}
}  // namespace WwVkPresent

#ifndef WW_VKPRESENT_NO_QT
namespace WwVkPresent
{
void farGroundSay( bool wanted, const char * glWhy )
{
	static QString said;
	static double at = 0.0;
	const QString head = QStringLiteral( "vk farground: " );
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
	const double winMs = at > 0.0 ? now - at : 0.0;  // lane VKFLIGHT18: the say window (GPU ms over it)
	at = now;
	const WaterStat st = takeFarGroundGlStat();
	const FxStat ds = takeFarGroundDrawStat();
	const TwinBatchStat bs = takeFarGroundBatchStat();
	QString line = head + QStringLiteral( "wanted on Vulkan, %1 draws run (fo4_cellcsmfgl twins, ring-0 terrain, into the tone frame; "
		"%2 ms CPU each, max %3, %4 copies, %5 fails%6); on GL %7 draws" )
		.arg( ds.runs ).arg( ds.runs ? ds.cpuMs / ds.runs : 0.0, 0, 'f', 3 ).arg( ds.maxMs, 0, 'f', 3 ).arg( ds.copies ).arg( ds.fails )
		.arg( ds.failWhy.empty() ? QString() : QStringLiteral( ": " ) + QString::fromStdString( ds.failWhy ) ).arg( st.gl );
	if ( !st.glWhy.empty() )
		line += QStringLiteral( " (refused: %1)" ).arg( QString::fromStdString( st.glWhy ) );
	line += takeFarGroundGlWhys( 0 );
	for ( int v = 1; v <= 2; v++ ) {  // lane VKGROUND11: the RT GI keys' twins (one crossing a tile)
		const FxStat rs = takeFarGroundRtDrawStat( v );
		const TwinBatchStat rb = takeFarGroundRtBatchStat( v );  // lane VKFLIGHT14: the RT / DG draws batched (submits a frame)
		if ( rs.runs || rs.fails )
			line += QStringLiteral( "; %1 twin: %2 draws, %3 ms CPU each, %4 copies, %5 fails%6" ).arg( v == 1 ? "RT" : "DG" ).arg( rs.runs )
				.arg( rs.runs ? rs.cpuMs / rs.runs : 0.0, 0, 'f', 3 ).arg( rs.copies ).arg( rs.fails )
				.arg( rs.failWhy.empty() ? QString() : QStringLiteral( ": " ) + QString::fromStdString( rs.failWhy ) );
		if ( rb.crossings || rb.lost )
			line += QStringLiteral( "; %1 batched: %2 crossings for %3 draws, %4 lost, %5 fence slips" ).arg( v == 1 ? "RT" : "DG" )
				.arg( rb.crossings ).arg( rb.draws ).arg( rb.lost ).arg( rb.slips );
	}
	if ( bs.crossings || bs.lost || bs.off )  // lane VKFAR4: the tiles batched into crossings
		line += QStringLiteral( "; batched: %1 crossings for %2 draws, %3 lost, %4 fence slips%5" ).arg( bs.crossings ).arg( bs.draws )
			.arg( bs.lost ).arg( bs.slips )
			.arg( bs.off ? QStringLiteral( ", off (one crossing a tile): " ) + QString::fromStdString( bs.offWhy ) : QString() );
	static const int kGpuKinds[3] = { 9, 23, 24 };
	static const char * const kGpuNames[3] = { "base", "RT", "DG" };
	line += fxGpuSay( kGpuKinds, kGpuNames, 3, winMs );
	said = line;
	qInfo().noquote() << line;
}

void farGroundLiveSay( bool wanted, const char * glWhy )
{
	static QString said;
	static double at = 0.0;
	const QString head = QStringLiteral( "vk fargroundlive: " );
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
	const WaterStat st = takeFarGroundLiveGlStat();
	const FxStat ds = takeFarGroundLiveDrawStat();
	QString line = head + QStringLiteral( "wanted on Vulkan, %1 draws run (fo4_cellcsmfgp twins, mode 2 cached pages, into the tone "
		"frame; %2 ms CPU each, max %3, %4 copies, %5 fails%6); on GL %7 draws" )
		.arg( ds.runs ).arg( ds.runs ? ds.cpuMs / ds.runs : 0.0, 0, 'f', 3 ).arg( ds.maxMs, 0, 'f', 3 ).arg( ds.copies ).arg( ds.fails )
		.arg( ds.failWhy.empty() ? QString() : QStringLiteral( ": " ) + QString::fromStdString( ds.failWhy ) ).arg( st.gl );
	if ( !st.glWhy.empty() )
		line += QStringLiteral( " (refused: %1)" ).arg( QString::fromStdString( st.glWhy ) );
	line += takeFarGroundGlWhys( 1 );
	for ( int v = 1; v <= 2; v++ ) {  // lane VKGROUND18: the RT GI keys' twins (one crossing a shape)
		const FxStat rs = takeFarGroundLiveRtDrawStat( v );
		if ( rs.runs || rs.fails )
			line += QStringLiteral( "; %1 twin: %2 draws, %3 ms CPU each, %4 copies, %5 fails%6" ).arg( v == 1 ? "RT" : "DG" ).arg( rs.runs )
				.arg( rs.runs ? rs.cpuMs / rs.runs : 0.0, 0, 'f', 3 ).arg( rs.copies ).arg( rs.fails )
				.arg( rs.failWhy.empty() ? QString() : QStringLiteral( ": " ) + QString::fromStdString( rs.failWhy ) );
	}
	said = line;
	qInfo().noquote() << line;
}
}  // namespace WwVkPresent
#endif
