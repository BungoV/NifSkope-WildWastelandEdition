/* Lane RTXGI1 M2 (2026-10-29): the ray-traced GI volume in the cell view (src/rtddgicell.h).
 * M3 (2026-10-29): GL reads the device's atlas in place (GL_EXT_memory_object_win32 + GL_EXT_semaphore_win32; red
 * WW_CELL_RTDDGI_RED=readback keeps M2's copy back and upload), and the volume follows the camera by whole probe cells
 * (rtddgimath.h scrollRuns; red WW_CELL_RTDDGI_RED=rebuild builds a new volume instead). */

#include "rtddgicell.h"

#include "harnesswindow.h"   // wwHarnessSettingsSuffix
#include "probeplace.h"      // ProbeSoup
#include "probesky.h"
#include "rtddgi.h"
#include "rtddgimath.h"
#include "rtddgi/rtddgi_gpu.h"
#include "version.h"
#include "wwswitch.h"
#include "gl/celllights.h"
#include "gl/cellunits.h"
#include "gl/cellssr.h"      // lane RESTIRDI1: wwCellAoTargets (the shared opaque pass ReSTIR reads)
#include "gl/glscene.h"
#include "gl/lookdevstage.h"   // lane RTXFIX1: wwLookdevFogUniforms (the probes view in the frame takes the fog)
#include "gl/renderer.h"
#include "gl/scenelighting.h"  // lane RTXFIX1: wwSceneExposureScale

#include <QDir>
#include <QElapsedTimer>
#include <QOpenGLContext>
#include <QOpenGLFunctions>
#include <QSettings>
#include <QStringList>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

namespace {

constexpr int kAtlasTexels = 193;           // a probe's texels in the texture buffer: 64 irradiance + 128 distance + 1 probe
constexpr double kViewReach = 3000.0;       // the debug view draws the probes within this of the camera (the PRTP view's)
constexpr size_t kViewMaxProbes = 20000;    // and at most this many spheres
constexpr int kScrollEcho = 16;             // the census prints the first scrolls, then every 64th

//! WW_CELL_RTDDGI_SPEC (harness): the volume's knobs, "k=v" comma list; the text is part of the build key
struct CellSpec
{
	float spacing = 140.0f;   // GUIDE 4: a probe every 2 m to start; grown to fit `probes`
	float zscale = 1.0f;
	float reach = 2048.0f;    // the window: 2 x reach a side (the probe GI's grid's size where it is smaller), round the camera
	int probes = 4096;        // the cap
	int rays = 128;
	int updates = 4;          // a frame (budget: M3)
	int max = 0;              // stop after this many updates since the build or the last scroll (0: never; pictures that must repeat set it)
	int every = 4;            // RTXCASCADE1: cascade L updates every every^L frames (rtddgimath.h cascadeDue)
	int crays = 0;            // RTXCASCADE2 cost lever: a coarse cascade's rays (0 = `rays`, RTXCASCADE1's; rtddgimath.h cascadeRays)
	int cskip = 0;            // RTXCASCADE2 cost lever: 1 = a coarse cascade skips the probes the finer one covers whole (cascadeCovered)
	QString text;
};

CellSpec cellSpecNow()
{
	CellSpec s;
	s.text = wwEnvStr( "WW_CELL_RTDDGI_SPEC" ).trimmed();
	for ( const QString & kv : s.text.split( QLatin1Char( ',' ), Qt::SkipEmptyParts ) ) {
		const QStringList p = kv.split( QLatin1Char( '=' ) );
		if ( p.size() != 2 )
			continue;
		const QString k = p[0].trimmed();
		bool ok = false;
		const double v = p[1].trimmed().toDouble( &ok );
		if ( !ok )
			continue;
		if ( k == QLatin1String( "spacing" ) && v > 1.0 ) s.spacing = float( v );
		else if ( k == QLatin1String( "zscale" ) && v > 0.05 ) s.zscale = float( v );
		else if ( k == QLatin1String( "reach" ) && v > 100.0 ) s.reach = float( v );
		else if ( k == QLatin1String( "probes" ) && v >= 8.0 ) s.probes = int( std::min( v, 65536.0 ) );
		else if ( k == QLatin1String( "rays" ) && v >= 33.0 ) s.rays = int( std::min( v, 256.0 ) );
		else if ( k == QLatin1String( "updates" ) && v >= 1.0 ) s.updates = int( std::min( v, 16.0 ) );
		else if ( k == QLatin1String( "max" ) && v >= 0.0 ) s.max = int( v );
		else if ( k == QLatin1String( "cevery" ) && v >= 1.0 ) s.every = int( std::min( v, 64.0 ) );
		else if ( k == QLatin1String( "crays" ) && v >= 0.0 ) s.crays = int( std::min( v, 256.0 ) );   // RTXCASCADE2
		else if ( k == QLatin1String( "cskip" ) && v >= 0.0 ) s.cskip = v > 0.5 ? 1 : 0;              // RTXCASCADE2
	}
	return s;
}

constexpr int kPathReadback = 1;   // WW_CELL_RTDDGI_RED=readback: M2's copy back + GL upload, no interop
constexpr int kPathRebuild = 2;    // WW_CELL_RTDDGI_RED=rebuild: a camera move builds a new volume (M2's way), no scroll
/* lane RTXFIX1 (Bryant 10-29, the Fusion Shaders bug "the next update overwrites the atlas while GL still reads it"): ours
 * orders both ways per submit (GL signals glDone before it, the device waits on that, GL waits on vkDone after it).
 * WW_CELL_RTDDGI_RED=nowait drops both: no beforeSubmit (the device then neither waits nor signals: rtddgi_vk.inc), no
 * GL wait; the census's "sync" counts go 0. Live. */
constexpr int kPathNoWait = 4;
/* lane RTXCASCADE2: WW_CELL_RTDDGI_RED=perblas, RTXCASCADE1's cascades: each coarse cascade builds its own BLAS from the
 * same soup and steps in its own submit (the one-BLAS / one-submit red). Live (a set rebuilds the coarse cascades). */
constexpr int kPathPerCascade = 8;

/*! WW_CELL_RTDDGI_RED, live: [0] the read's red bits (rtddgimath.h kSample*, the shader's cellDdgiRed), [1] the path
 *  reds (kPath*). One list, both parts: "trilinear,readback" sets one of each */
const int * cellRedParts()
{
	struct Parts
	{
		int v[2] = { 0, 0 };
	};
	WW_SWITCH_STATIC( Parts, parts, []() {
		Parts b;
		for ( const QString & r : wwEnvStr( "WW_CELL_RTDDGI_RED" ).split( QLatin1Char( ',' ), Qt::SkipEmptyParts ) ) {
			const QString t = r.trimmed();
			if ( t == QLatin1String( "trilinear" ) ) b.v[0] |= int( rtddgi::kSampleTrilinear );
			if ( t == QLatin1String( "nocheb" ) ) b.v[0] |= int( rtddgi::kSampleNoCheb );
			if ( t == QLatin1String( "nobias" ) ) b.v[0] |= int( rtddgi::kSampleNoBias );
			if ( t == QLatin1String( "readback" ) ) b.v[1] |= kPathReadback;
			if ( t == QLatin1String( "rebuild" ) ) b.v[1] |= kPathRebuild;
			if ( t == QLatin1String( "nowait" ) ) b.v[1] |= kPathNoWait;   // lane RTXFIX1
			if ( t == QLatin1String( "perblas" ) ) b.v[1] |= kPathPerCascade;   // lane RTXCASCADE2
		}
		return b;
	}() );
	return parts.v;
}
int cellRedBits() { return cellRedParts()[0]; }
int cellRedPaths() { return cellRedParts()[1]; }

//! one volume being opened on a worker (the soup copy, the BLAS, relocation and classification take seconds)
struct Build
{
	std::mutex m;
	bool done = false, cancel = false, ok = false;
	RtDdgiLive live;
	QString why;
	double ms = 0, msCopy = 0;
	CellSpec spec;
	int paths = 0;            // the path reds it was built under (part of the key)
	QString box;              // the census's words for the box
	int lights = 0;
	QString skyLabel;
	// M3: the window the scroll keeps the camera in (world): the grid it stays inside, its size on each axis
	double gridLo[3] = { 0, 0, 0 }, gridHi[3] = { 0, 0, 0 }, window[3] = { 0, 0, 0 };
};

// M3: GL_EXT_memory_object(_win32) + GL_EXT_semaphore(_win32), loaded for the current context
typedef void ( APIENTRY * PfnCreateMemoryObjects )( GLsizei, GLuint * );
typedef void ( APIENTRY * PfnDeleteMemoryObjects )( GLsizei, const GLuint * );
typedef void ( APIENTRY * PfnMemoryObjectParameteriv )( GLuint, GLenum, const GLint * );
typedef void ( APIENTRY * PfnImportMemoryWin32Handle )( GLuint, std::uint64_t, GLenum, void * );
typedef void ( APIENTRY * PfnBufferStorageMem )( GLenum, GLsizeiptr, GLuint, std::uint64_t );
typedef void ( APIENTRY * PfnGenSemaphores )( GLsizei, GLuint * );
typedef void ( APIENTRY * PfnDeleteSemaphores )( GLsizei, const GLuint * );
typedef void ( APIENTRY * PfnImportSemaphoreWin32Handle )( GLuint, GLenum, void * );
typedef void ( APIENTRY * PfnSemaphoreOp )( GLuint, GLuint, const GLuint *, GLuint, const GLuint *, const GLenum * );
typedef void ( APIENTRY * PfnGetUnsignedBytei )( GLenum, GLuint, GLubyte * );
typedef void ( APIENTRY * PfnGetUnsignedBytev )( GLenum, GLubyte * );
constexpr GLenum kGlDedicatedMemoryObject = 0x9581;   // GL_DEDICATED_MEMORY_OBJECT_EXT
constexpr GLenum kGlHandleOpaqueWin32 = 0x9587;       // GL_HANDLE_TYPE_OPAQUE_WIN32_EXT
constexpr GLenum kGlNumDeviceUuids = 0x9596;          // GL_NUM_DEVICE_UUIDS_EXT
constexpr GLenum kGlDeviceUuid = 0x9597;              // GL_DEVICE_UUID_EXT
constexpr GLenum kGlDriverUuid = 0x9598;              // GL_DRIVER_UUID_EXT

struct GlShare
{
	bool tried = false, ok = false;
	const QOpenGLContext * ctx = nullptr;
	QString why;
	PfnCreateMemoryObjects createMem = nullptr;
	PfnDeleteMemoryObjects deleteMem = nullptr;
	PfnMemoryObjectParameteriv memParam = nullptr;
	PfnImportMemoryWin32Handle importMem = nullptr;
	PfnBufferStorageMem bufferStorageMem = nullptr;
	PfnGenSemaphores genSem = nullptr;
	PfnDeleteSemaphores deleteSem = nullptr;
	PfnImportSemaphoreWin32Handle importSem = nullptr;
	PfnSemaphoreOp waitSem = nullptr, signalSem = nullptr;
	PfnGetUnsignedBytei uuidAt = nullptr;
	PfnGetUnsignedBytev uuid = nullptr;
};

GlShare & glShare()
{
	static GlShare g;
	QOpenGLContext * q = QOpenGLContext::currentContext();
	if ( g.tried && g.ctx == q )
		return g;
	g = GlShare();
	g.tried = true;
	g.ctx = q;
	if ( !q ) {
		g.why = QStringLiteral( "no GL context" );
		return g;
	}
	for ( const char * e : { "GL_EXT_memory_object", "GL_EXT_memory_object_win32", "GL_EXT_semaphore", "GL_EXT_semaphore_win32" } )
		if ( !q->hasExtension( e ) ) {
			g.why = QStringLiteral( "the GL context has no %1" ).arg( QLatin1String( e ) );
			return g;
		}
	g.createMem = reinterpret_cast<PfnCreateMemoryObjects>( q->getProcAddress( "glCreateMemoryObjectsEXT" ) );
	g.deleteMem = reinterpret_cast<PfnDeleteMemoryObjects>( q->getProcAddress( "glDeleteMemoryObjectsEXT" ) );
	g.memParam = reinterpret_cast<PfnMemoryObjectParameteriv>( q->getProcAddress( "glMemoryObjectParameterivEXT" ) );
	g.importMem = reinterpret_cast<PfnImportMemoryWin32Handle>( q->getProcAddress( "glImportMemoryWin32HandleEXT" ) );
	g.bufferStorageMem = reinterpret_cast<PfnBufferStorageMem>( q->getProcAddress( "glBufferStorageMemEXT" ) );
	g.genSem = reinterpret_cast<PfnGenSemaphores>( q->getProcAddress( "glGenSemaphoresEXT" ) );
	g.deleteSem = reinterpret_cast<PfnDeleteSemaphores>( q->getProcAddress( "glDeleteSemaphoresEXT" ) );
	g.importSem = reinterpret_cast<PfnImportSemaphoreWin32Handle>( q->getProcAddress( "glImportSemaphoreWin32HandleEXT" ) );
	g.waitSem = reinterpret_cast<PfnSemaphoreOp>( q->getProcAddress( "glWaitSemaphoreEXT" ) );
	g.signalSem = reinterpret_cast<PfnSemaphoreOp>( q->getProcAddress( "glSignalSemaphoreEXT" ) );
	g.uuidAt = reinterpret_cast<PfnGetUnsignedBytei>( q->getProcAddress( "glGetUnsignedBytei_vEXT" ) );
	g.uuid = reinterpret_cast<PfnGetUnsignedBytev>( q->getProcAddress( "glGetUnsignedBytevEXT" ) );
	g.ok = g.createMem && g.deleteMem && g.memParam && g.importMem && g.bufferStorageMem && g.genSem && g.deleteSem && g.importSem
		&& g.waitSem && g.signalSem && g.uuidAt && g.uuid;
	if ( !g.ok )
		g.why = QStringLiteral( "a GL_EXT_memory_object / semaphore entry point is missing" );
	return g;
}

struct Cell
{
	bool loaded = false, guiOn = false;
	int guiView = 0;
	const void * nif = nullptr;
	quint64 key = 0;
	std::shared_ptr<Build> pending, live;
	QString refused;
	int sets = 0, busy = 0;
	bool stopped = false, dumped = false, censusBuilt = false;
	// GL: one texture buffer (the context of the renderer that made it); interop: its storage is the device's atlas
	const void * glOwner = nullptr;
	GLuint buf = 0, tex = 0;
	GLuint mem = 0, semVk = 0, semGl = 0;   // M3: the imported memory object and the two semaphores
	bool interop = false;                   // M3: buf is the device's atlas (no upload)
	bool glRead = false;                    // M3: GL waited on vkDone since its last glDone (it owes a signal before the next submit)
	QString interopWhy;                     // M3: why this volume reads back (the census)
	size_t bytes = 0;
	bool uploaded = false, attach = false, probesPacked = false;
	double uploadMs = 0, glWaitMs = 0;
	// the debug view's geometry, rebuilt each frame it draws
	std::vector<Vector3> vp;
	std::vector<FloatVector4> vc;
	// lane RTXFIX1: the GL order's counts since the build (the census's "sync"), the in-frame view's GL objects (the
	// context of the renderer that made them), its instance list, and whether it drew this frame (the overlay then not)
	qint64 syncSignals = 0, syncWaits = 0;
	const void * viewOwner = nullptr;
	GLuint viewVao = 0, viewMeshBuf = 0, viewMeshTex = 0, viewListBuf = 0, viewListTex = 0;
	std::vector<float> viewList;
	bool viewDrewInFrame = false;
	QString viewLast;
	// RTXCASCADE1: the cascade this is (0 = M3's volume, cell()), its census prefix ("" for cascade 0: M3's lines),
	// and new tiles in buf since the last copy into the cascades' texture buffer
	int level = 0;
	QString tag;
	bool fresh = false;
	/* RTXCASCADE2: the rows (cell() only): the cascades count and the debug view's cascade; and the frame this volume's
	 * step was last ordered on with its first probe after that step (the next coarser cascade's skip range reads it) */
	int guiCascades = 1, guiViewCascade = 0;
	quint64 orderFrame = 0;
	double orderOrigin[3] = { 0, 0, 0 };
};

Cell & cell()
{
	static Cell * c = new Cell;   // never destroyed: a live volume must not close after the bake device's statics
	return *c;
}

/* lane SKYINT1 (10-29): a Use Sky Lighting interior (WwCellLighting skyLighting) takes the CURRENT Lookdev weather and
 * hour as the volume's sky, as the game's full-mode sky does there, whether or not the baked probe GI relit the kept
 * cell. A change is taken once it has stood still kSkyStillMs (an hour slider drags through many values; each new sky
 * builds the volume again). Red WW_CELL_SKYLIT_RED=sky|fixed: the sky the document's first build took, held. */
constexpr qint64 kSkyStillMs = 400;

struct SkyLit
{
	const void * nif = nullptr;
	ProbeSkyLight first, used, wanted;
	QElapsedTimer still;
	bool waiting = false;
};

SkyLit & skyLit()
{
	static SkyLit s;
	return s;
}

//! the sky to build with for `nif` (the kept relight's in `sky` on entry)
void skyLitSky( const void * nif, ProbeSkyLight & sky )
{
	SkyLit & s = skyLit();
	if ( s.nif != nif ) {
		s = SkyLit();
		s.nif = nif;
		s.first = sky;
		s.used = sky;
		const ProbeSkyLight now = probeSkyLightNow();
		if ( now.on )
			s.used = now;   // the first build takes the hour it opens at, no wait
		s.wanted = s.used;
	}
	if ( wwCellSkyLitRed() & 2 ) {
		s.waiting = false;
		sky = s.first;
		return;
	}
	const ProbeSkyLight now = probeSkyLightNow();
	if ( now.on && !now.same( s.wanted ) ) {
		s.wanted = now;
		s.still.start();
		s.waiting = true;
	}
	if ( s.waiting && s.still.elapsed() >= kSkyStillMs ) {
		s.used = s.wanted;
		s.waiting = false;
	}
	sky = s.used;
	/* the void under an interior cell is ground in the game, never sky: a ray leaving downward (a broken floor, a
	 * probe under the slab) took the weather's ground-facing colour and lit the floor from below (the 10-29 dump: floor
	 * probes 0.2-0.44 sky). The face a surface facing down takes is 0; red WW_CELL_SKYLIT_RED=ground keeps it */
	if ( !( wwCellSkyLitRed() & 4 ) )
		for ( int c = 0; c < 3; c++ )
			sky.amb[5][c] = 0.0f;
}

//! RTXCASCADE1: cascade `level`'s volume (0: cell(), M3's)
Cell & cascCell( int level )
{
	if ( level <= 0 )
		return cell();
	static Cell * cs = []() {
		Cell * a = new Cell[rtddgi::kMaxCascades];   // never destroyed, as cell()
		for ( int l = 1; l < rtddgi::kMaxCascades; l++ ) {
			a[l].level = l;
			a[l].tag = QStringLiteral( "cascade %1: " ).arg( l );
		}
		return a;
	}();
	return cs[std::min( level, rtddgi::kMaxCascades - 1 )];
}

/*! RTXCASCADE1: the cascades' one texture buffer (no new sampler: cellDdgiAtlas reads it). Cascade l's atlas (M3's
 *  layout, its own nP) at texel base[l]; copied on the GPU (glCopyBufferSubData) from its own buffer, the device's
 *  atlas with interop, after the GL side's wait on that cascade's submit. count = the cascades ready from 0 */
struct Combined
{
	const void * owner = nullptr;
	GLuint buf = 0, tex = 0;
	size_t bytes = 0;
	bool attach = false;
	int count = 0;
	int base[rtddgi::kMaxCascades] = {}, nP[rtddgi::kMaxCascades] = {};
	double copyMs = 0;
	qint64 copies = 0;
};

Combined & combined()
{
	static Combined * k = new Combined;
	return *k;
}

//! the GUI's settings file (bakert.cpp's; a harness WW_SETTINGS_SCOPE starts empty, so the rows read off there)
QString rowSettingsApp()
{
	return QStringLiteral( "NifSkope " ) + NifSkopeVersion::rawToMajMin( NIFSKOPE_VERSION ) + wwHarnessSettingsSuffix();
}

void rowWrite( const QString & key, const QVariant & value )
{
	QSettings s( QStringLiteral( "NifTools" ), rowSettingsApp() );
	s.setValue( key, value );
}

void loadRows( Cell & c )
{
	if ( c.loaded )
		return;
	c.loaded = true;
	QSettings s( QStringLiteral( "NifTools" ), rowSettingsApp() );
	c.guiOn = s.value( QStringLiteral( "WW/CellRtDdgi" ), false ).toBool();
	c.guiView = std::clamp( s.value( QStringLiteral( "WW/CellRtDdgiView" ), 0 ).toInt(), 0, 2 );
	// RTXCASCADE2: the cascades row (1 = M3's one volume, the default) and the debug view's cascade (0 = the nearest)
	c.guiCascades = std::clamp( s.value( QStringLiteral( "WW/CellRtDdgiCascades" ), 1 ).toInt(), 1, rtddgi::kMaxCascades );
	c.guiViewCascade = std::clamp( s.value( QStringLiteral( "WW/CellRtDdgiViewCascade" ), 0 ).toInt(), 0, rtddgi::kMaxCascades - 1 );
}

void say( const QString & line )
{
	std::fprintf( stderr, "rtddgi cell: %s\n", qPrintable( line ) );
	std::fflush( stderr );
}

void refuse( Cell & c, const QString & why )
{
	if ( why == c.refused )
		return;
	c.refused = why;
	say( c.tag + QStringLiteral( "refused(%1)" ).arg( why ) );
}

//! the GL side: the texture buffer, and with interop the imported memory and semaphores (before the session closes)
void freeGl( Cell & c, Renderer * r )
{
	if ( r && c.glOwner == r ) {
		if ( c.tex )
			r->fn->glDeleteTextures( 1, &c.tex );
		if ( c.buf )
			r->fn->glDeleteBuffers( 1, &c.buf );
		GlShare & g = glShare();
		if ( g.ok ) {
			if ( c.mem )
				g.deleteMem( 1, &c.mem );
			if ( c.semVk )
				g.deleteSem( 1, &c.semVk );
			if ( c.semGl )
				g.deleteSem( 1, &c.semGl );
		}
	}
	c.buf = c.tex = 0;   // made in another context: left to it (one view draws the cell)
	c.mem = c.semVk = c.semGl = 0;
	c.bytes = 0;
	c.glOwner = nullptr;
	c.uploaded = c.attach = c.probesPacked = false;
	c.interop = c.glRead = false;
	c.fresh = false;
}

//! the volume and its GL objects (GL first: its import holds the device's memory)
void dropLive( Cell & c, Renderer * r )
{
	freeGl( c, r );
	if ( c.pending ) {
		std::lock_guard<std::mutex> lk( c.pending->m );
		c.pending->cancel = true;
		if ( c.pending->done )
			c.pending->live.close();
	}
	c.pending.reset();
	c.live.reset();   // its RtDdgiLive closes the device's session
	c.sets = c.busy = 0;
	c.syncSignals = c.syncWaits = 0;   // lane RTXFIX1
	c.stopped = c.dumped = c.censusBuilt = false;
	c.interopWhy.clear();
}

quint64 fnv( quint64 h, const void * p, size_t n )
{
	const unsigned char * b = static_cast<const unsigned char *>( p );
	for ( size_t i = 0; i < n; i++ )
		h = ( h ^ b[i] ) * 0x100000001B3ULL;
	return h;
}

//! the camera's world position (celllights.cpp's cellRow w: R^T (0 - t) / scale)
void eyeOf( const Transform & vt, float eye[3] )
{
	const float sc = vt.scale != 0.0f ? vt.scale : 1.0f;
	for ( int k = 0; k < 3; k++ ) {
		float w = 0.0f;
		for ( int j = 0; j < 3; j++ )
			w -= vt.rotation( j, k ) * vt.translation[j];
		eye[k] = w / sc;
	}
}

/*! the volume's box: a window of 2 x reach a side (the grid's own size where that is smaller, two spacings at least)
 *  round the camera, kept inside the probe GI's grid (its probes' part: the x/y pad voxels left out; else the soup's
 *  box). M3: the window's size does not depend on where the camera is, so the scroll keeps the same grid */
bool boxFor( const WwCellGi * G, const ProbeSoup & soup, const float eye[3], const CellSpec & sp, float box[6], Build & b )
{
	float lo[3], hi[3];
	QString words;
	if ( G && G->dims[0] > 0 && G->dims[1] > 0 && G->dims[2] > 0 ) {
		for ( int k = 0; k < 3; k++ ) {
			const float pad = k < 2 ? float( G->pad ) * G->voxel : 0.0f;
			lo[k] = G->origin[k] + pad;
			hi[k] = G->origin[k] + float( G->dims[k] ) * G->voxel - pad;
		}
		words = QStringLiteral( "the probe GI's grid" );
	} else {
		for ( int k = 0; k < 3; k++ ) {
			lo[k] = 3.4e38f;
			hi[k] = -3.4e38f;
		}
		for ( size_t i = 0; i < soup.tris.size(); i += 3 )
			for ( int k = 0; k < 3; k++ ) {
				lo[k] = std::min( lo[k], soup.tris[i + size_t( k )] );
				hi[k] = std::max( hi[k], soup.tris[i + size_t( k )] );
			}
		words = QStringLiteral( "the soup (no probe GI grid)" );
	}
	for ( int k = 0; k < 3; k++ ) {
		if ( hi[k] <= lo[k] )
			return false;
		const double minSpan = 2.0 * double( sp.spacing ) * ( k == 2 ? double( sp.zscale ) : 1.0 );   // three probe planes at least
		const double win = std::max( std::min( double( hi[k] - lo[k] ), 2.0 * double( sp.reach ) ), minSpan );
		const double a = rtddgi::scrollWindowLo( double( eye[k] ), double( lo[k] ), double( hi[k] ), win );
		box[k] = float( a );
		box[3 + k] = float( a + win );
		b.gridLo[k] = lo[k];
		b.gridHi[k] = hi[k];
		b.window[k] = win;
	}
	b.box = words + QStringLiteral( " %1 %2 %3 .. %4 %5 %6, a window %7 x %8 x %9 round the camera (%10 %11 %12)" )
		.arg( double( lo[0] ), 0, 'f', 0 ).arg( double( lo[1] ), 0, 'f', 0 ).arg( double( lo[2] ), 0, 'f', 0 )
		.arg( double( hi[0] ), 0, 'f', 0 ).arg( double( hi[1] ), 0, 'f', 0 ).arg( double( hi[2] ), 0, 'f', 0 )
		.arg( b.window[0], 0, 'f', 0 ).arg( b.window[1], 0, 'f', 0 ).arg( b.window[2], 0, 'f', 0 )
		.arg( double( eye[0] ), 0, 'f', 0 ).arg( double( eye[1] ), 0, 'f', 0 ).arg( double( eye[2] ), 0, 'f', 0 );
	return true;
}

//! M3: whole cells from the volume's first probe to the window round the eye (0 0 0: the volume stays)
bool scrollFor( const Build & b, const RtDdgiVolume & v, const float eye[3], int cells[3] )
{
	bool any = false;
	for ( int k = 0; k < 3; k++ ) {
		const double lo = rtddgi::scrollWindowLo( double( eye[k] ), b.gridLo[k], b.gridHi[k], b.window[k] );
		cells[k] = v.step[k] > 0.0f ? rtddgi::scrollCells( double( v.origin[k] ), lo, double( v.step[k] ) ) : 0;
		any = any || cells[k] != 0;
	}
	return any;
}

QString statesOf( const RtDdgiVolume & v )
{
	return QStringLiteral( "probes %1: off %2, asleep %3, vigilant %4, moved %5%6" ).arg( v.probes ).arg( v.off ).arg( v.asleep )
		.arg( v.vigilant ).arg( v.moved ).arg( v.fresh > 0 ? QStringLiteral( ", fresh %1 (classified on the device, not back yet)" ).arg( v.fresh ) : QString() );
}

/*! M3: where the camera sits against the volume (the Memory Den night case: M2's census could not tell a box that missed
 *  the view from a view with no usable probe): inside or outside the probe box, and the probes within 1.5 cells of it */
QString cameraWords( const RtDdgiVolume & v, const float eye[3] )
{
	bool inside = true;
	for ( int k = 0; k < 3; k++ )
		inside = inside && eye[k] >= v.origin[k] && eye[k] <= v.origin[k] + float( v.dims[k] - 1 ) * v.step[k];
	int nearN = 0, vig = 0, offN = 0, asleep = 0;
	const int nP = v.probeCount();
	for ( int p = 0; p < nP && size_t( p ) < v.state.size(); p++ ) {
		float pos[3];
		v.position( p, pos );
		float d = 0.0f;
		for ( int k = 0; k < 3; k++ )
			d = std::max( d, std::fabs( pos[k] - eye[k] ) / std::max( v.step[k], 1e-3f ) );
		if ( d > 1.5f )
			continue;
		nearN++;
		vig += v.state[size_t( p )] == rtddgi::kVigilant ? 1 : 0;
		offN += v.state[size_t( p )] == rtddgi::kOff ? 1 : 0;
		asleep += v.state[size_t( p )] == rtddgi::kAsleep ? 1 : 0;
	}
	return QStringLiteral( "camera %1 %2 %3 %4 the box (first probe %5 %6 %7, last %8 %9 %10); probes within 1.5 cells of it %11: "
		"vigilant %12, asleep %13, off %14" )
		.arg( double( eye[0] ), 0, 'f', 0 ).arg( double( eye[1] ), 0, 'f', 0 ).arg( double( eye[2] ), 0, 'f', 0 )
		.arg( inside ? QStringLiteral( "inside" ) : QStringLiteral( "OUTSIDE" ) )
		.arg( double( v.origin[0] ), 0, 'f', 0 ).arg( double( v.origin[1] ), 0, 'f', 0 ).arg( double( v.origin[2] ), 0, 'f', 0 )
		.arg( double( v.origin[0] + float( v.dims[0] - 1 ) * v.step[0] ), 0, 'f', 0 )
		.arg( double( v.origin[1] + float( v.dims[1] - 1 ) * v.step[1] ), 0, 'f', 0 )
		.arg( double( v.origin[2] + float( v.dims[2] - 1 ) * v.step[2] ), 0, 'f', 0 )
		.arg( nearN ).arg( vig ).arg( asleep ).arg( offN );
}

qint64 atlasBytes( int nP )
{
	return qint64( nP ) * kAtlasTexels * 16;
}

QString msText( double ms )
{
	return ms < 0.0 ? QStringLiteral( "-" ) : QString::number( ms, 'f', 3 );
}

/*! M3: the device's atlas as the GL texture buffer's storage (no upload ever). False + why: the volume reads back.
 *  The same GPU and driver on both sides (the UUIDs GL_EXT_memory_object gives against Vulkan's) */
bool importShare( Cell & c, Renderer * r, RtDdgiLive & L, QString * why )
{
	GlShare & g = glShare();
	if ( !g.ok ) {
		*why = g.why;
		return false;
	}
	bakert::DdgiGpuShare sh;
	if ( !bakert::ddgiGpuShare( L.session(), &sh ) ) {
		*why = QString::fromStdString( sh.why );
		return false;
	}
	auto closeHandles = [&sh]() {
		bakert::ddgiGpuCloseHandle( sh.memory );
		bakert::ddgiGpuCloseHandle( sh.vkDone );
		bakert::ddgiGpuCloseHandle( sh.glDone );
	};
	auto fn = r->fn;
	GLint nDev = 0;
	fn->glGetIntegerv( kGlNumDeviceUuids, &nDev );
	bool sameDevice = false;
	for ( GLint i = 0; i < nDev && !sameDevice; i++ ) {
		GLubyte u[16] = {};
		g.uuidAt( kGlDeviceUuid, GLuint( i ), u );
		sameDevice = std::memcmp( u, sh.deviceUuid, 16 ) == 0;
	}
	GLubyte drv[16] = {};
	g.uuid( kGlDriverUuid, drv );
	if ( !sameDevice || std::memcmp( drv, sh.driverUuid, 16 ) != 0 ) {
		closeHandles();
		*why = QStringLiteral( "the GL context runs on another GPU or driver than the bake device (%1 GL devices)" ).arg( nDev );
		return false;
	}
	for ( int i = 0; i < 16 && fn->glGetError() != GL_NO_ERROR; i++ ) {}   // older errors are not this import's
	freeGl( c, r );
	c.glOwner = r;
	g.createMem( 1, &c.mem );
	const GLint dedicated = sh.dedicated ? 1 : 0;
	g.memParam( c.mem, kGlDedicatedMemoryObject, &dedicated );
	g.importMem( c.mem, sh.memorySize, kGlHandleOpaqueWin32, sh.memory );
	fn->glGenBuffers( 1, &c.buf );
	fn->glBindBuffer( GL_TEXTURE_BUFFER, c.buf );
	g.bufferStorageMem( GL_TEXTURE_BUFFER, GLsizeiptr( sh.atlasBytes ), c.mem, 0 );
	fn->glBindBuffer( GL_TEXTURE_BUFFER, 0 );
	fn->glGenTextures( 1, &c.tex );
	g.genSem( 1, &c.semVk );
	g.importSem( c.semVk, kGlHandleOpaqueWin32, sh.vkDone );
	g.genSem( 1, &c.semGl );
	g.importSem( c.semGl, kGlHandleOpaqueWin32, sh.glDone );
	const GLenum err = fn->glGetError();
	closeHandles();   // the imports keep their own references (NT handles)
	if ( err != GL_NO_ERROR ) {
		freeGl( c, r );
		*why = QStringLiteral( "the GL import failed (GL error 0x%1)" ).arg( qulonglong( err ), 0, 16 );
		return false;
	}
	c.glOwner = r;
	c.bytes = size_t( sh.atlasBytes );
	c.attach = true;
	c.interop = true;
	c.glRead = false;
	return true;
}

//! the readback path (M2's, the red): the tiles of the last step into the texture buffer, the probes' texels when moved
bool upload( Cell & c, Renderer * r, const RtDdgiLive & L )
{
	auto fn = r->fn;
	const RtDdgiVolume & v = L.volume();
	const int nP = v.probeCount();
	const float * irr = L.irr(), * dist = L.dist();
	const quint32 * age = L.age();
	if ( !irr || !dist || !age || nP <= 0 )
		return false;
	if ( c.glOwner != r ) {
		freeGl( c, nullptr );
		c.glOwner = r;
	}
	if ( !c.buf ) {
		fn->glGenBuffers( 1, &c.buf );
		fn->glGenTextures( 1, &c.tex );
	}
	QElapsedTimer t;
	t.start();
	const size_t bytes = size_t( atlasBytes( nP ) );
	fn->glBindBuffer( GL_TEXTURE_BUFFER, c.buf );
	if ( c.bytes != bytes ) {
		fn->glBufferData( GL_TEXTURE_BUFFER, GLsizeiptr( bytes ), nullptr, GL_DYNAMIC_DRAW );
		c.bytes = bytes;
		c.attach = true;
		c.probesPacked = false;
	}
	fn->glBufferSubData( GL_TEXTURE_BUFFER, 0, GLsizeiptr( size_t( nP ) * 64 * 16 ), irr );
	fn->glBufferSubData( GL_TEXTURE_BUFFER, GLintptr( size_t( nP ) * 64 * 16 ), GLsizeiptr( size_t( nP ) * 128 * 16 ), dist );
	if ( !c.probesPacked ) {   // the states change only at a scroll; usable needs one blend (age > 0)
		std::vector<float> pt( size_t( nP ) * 4 );
		for ( int p = 0; p < nP; p++ ) {
			for ( int k = 0; k < 3; k++ )
				pt[size_t( p ) * 4 + size_t( k )] = v.offset[size_t( p ) * 3 + size_t( k )];
			const int st = v.state[size_t( p )];
			pt[size_t( p ) * 4 + 3] = float( rtddgi::probeUsable( st, age[p] ) ? st : int( rtddgi::kOff ) );
		}
		fn->glBufferSubData( GL_TEXTURE_BUFFER, GLintptr( size_t( nP ) * 192 * 16 ), GLsizeiptr( pt.size() * 4 ), pt.data() );
		c.probesPacked = true;
	}
	fn->glBindBuffer( GL_TEXTURE_BUFFER, 0 );
	c.uploadMs = double( t.nsecsElapsed() ) / 1e6;
	c.uploaded = true;
	return true;
}

/* RTXCASCADE2: `from` = cascade 0's built volume for a coarse cascade (it opens on that BLAS: one BLAS for every cascade;
 * the worker holds it until the open returns, so a drop of cascade 0 meanwhile cannot free the scene under it) */
void startBuild( Cell & c, const ProbeSoup & soup, const WwCellLighting & lights, const ProbeSkyLight & sky, const WwCellGi * G,
	const float eye[3], const CellSpec & sp, int paths, std::shared_ptr<Build> from = nullptr )
{
	auto b = std::make_shared<Build>();
	b->spec = sp;
	b->paths = paths;
	b->lights = int( lights.lights.size() );
	b->skyLabel = sky.on ? sky.label : QStringLiteral( "no sky (interior)" );
	RtDdgiSpec spec;
	spec.spacing = sp.spacing;
	spec.zScale = sp.zscale;
	spec.rays = sp.rays;
	spec.maxProbes = sp.probes;
	spec.fitProbes = true;
	spec.place = false;
	spec.gpu = 1;
	if ( !boxFor( G, soup, eye, sp, spec.box, *b ) ) {
		std::lock_guard<std::mutex> lk( b->m );
		b->done = true;
		b->why = QStringLiteral( "no box: the grid or the soup is empty" );
		c.pending = b;
		return;
	}
	const bool share = !( paths & kPathReadback );
	QElapsedTimer t;
	t.start();
	auto soupCopy = std::make_shared<ProbeSoup>( soup );   // the kept soup may be replaced while the worker reads
	b->msCopy = double( t.nsecsElapsed() ) / 1e6;
	c.pending = b;
	std::thread( [b, soupCopy, lights, sky, spec, share, from]() {
		QElapsedTimer tm;
		tm.start();
		const bool ok = b->live.open( *soupCopy, lights, sky, spec, share, from ? &from->live : nullptr );
		std::lock_guard<std::mutex> lk( b->m );
		b->ok = ok;
		b->why = b->live.why();
		b->ms = double( tm.nsecsElapsed() ) / 1e6;
		b->done = true;
		if ( b->cancel )
			b->live.close();
	} ).detach();
}

void censusBuilt( const Cell & c, const Build & b )
{
	const RtDdgiVolume & v = b.live.volume();
	const RtDdgiSpec & s = b.live.spec();
	const int nP = v.probeCount();
	const bool grown = std::fabs( v.step[0] - s.spacing ) > 1e-3f;
	// RTXCASCADE2: whose BLAS a coarse cascade traces (cascade 0's: one BLAS for all; its own: the perblas red, or why)
	const QString blas = c.level == 0 ? QString() : b.live.builtBlas()
		? QStringLiteral( "; BLAS: its own%1" ).arg( b.live.sceneWhy().isEmpty() ? QString() : QStringLiteral( " (%1)" ).arg( b.live.sceneWhy() ) )
		: QStringLiteral( "; BLAS: cascade 0's (one BLAS)" );
	say( c.tag + QStringLiteral( "built %1 in %2 ms (soup copy %3 ms); grid %4 x %5 x %6, spacing %7 x %8 x %9%10; box %11; first probe %12 %13 "
		"%14; %15; relocate+classify %16 ms; BLAS grown %17 u; rays %18 a probe, %19 updates a frame%20; atlas bytes %21 (irr %22, "
		"dist %23, probes %24); lights %25; sky %26; %27; %28" )
		.arg( v.path ).arg( b.ms, 0, 'f', 1 ).arg( b.msCopy, 0, 'f', 1 ).arg( v.dims[0] ).arg( v.dims[1] ).arg( v.dims[2] )
		.arg( double( v.step[0] ), 0, 'f', 1 ).arg( double( v.step[1] ), 0, 'f', 1 ).arg( double( v.step[2] ), 0, 'f', 1 )
		.arg( grown ? QStringLiteral( " (asked %1, grown to fit the cap %2)" ).arg( double( s.spacing ), 0, 'f', 1 ).arg( s.maxProbes ) : QString() )
		.arg( b.box ).arg( double( v.origin[0] ), 0, 'f', 0 ).arg( double( v.origin[1] ), 0, 'f', 0 ).arg( double( v.origin[2] ), 0, 'f', 0 )
		.arg( statesOf( v ) ).arg( v.msReloc, 0, 'f', 1 ).arg( v.blasGrow, 0, 'f', 4 ).arg( s.rays ).arg( b.spec.updates )
		.arg( b.spec.max > 0 ? QStringLiteral( " (stop at %1 after the build or a scroll)" ).arg( b.spec.max ) : QString() )
		.arg( atlasBytes( nP ) ).arg( qint64( nP ) * 64 * 16 ).arg( qint64( nP ) * 128 * 16 ).arg( qint64( nP ) * 16 )
		.arg( b.lights ).arg( b.skyLabel )
		.arg( c.interop ? QStringLiteral( "interop (GL reads the device's atlas in place)" )
			: QStringLiteral( "readback (%1)" ).arg( c.interopWhy ) )
		.arg( ( b.paths & kPathRebuild ) ? QStringLiteral( "a camera move rebuilds (the rebuild red)" )
			: QStringLiteral( "scrolls with the camera by whole cells" ) ) + blas );
}

void censusSet( const Cell & c, const RtDdgiLive & L, int k, const QString & tail )
{
	const double * ms = L.lastMs();
	QString rb;
	if ( c.interop )   // nothing comes back: the census's readback is 0 (the brief's key), the GL wait is the GPU-side order
		rb = QStringLiteral( "readback ms 0.000 (interop: no copy, no upload; GL waits on the device %1 ms on the CPU); sync glDone %2 "
			"vkDone waits %3%4" ).arg( c.glWaitMs, 0, 'f', 3 ).arg( c.syncSignals ).arg( c.syncWaits )
			.arg( ( cellRedPaths() & kPathNoWait ) ? QStringLiteral( " (the nowait red: no order)" ) : QString() );   // lane RTXFIX1
	else
		rb = QStringLiteral( "readback ms %1 (device copy %2 + GL upload %3; interop owed when over 0.2: %4)" )
			.arg( msText( ms[3] < 0.0 ? -1.0 : ms[3] + c.uploadMs ) ).arg( msText( ms[3] ) ).arg( c.uploadMs, 0, 'f', 3 ).arg( c.interopWhy );
	say( c.tag + QStringLiteral( "set %1, updates %2 (+%3); gpu ms trace %4 blend %5 border %6 scroll %7 (the last finished set's); %8; wall %9 ms; "
		"rays %10; %11; atlas bytes %12; busy frames %13%14" )
		.arg( c.sets ).arg( L.updates() ).arg( k ).arg( msText( ms[0] ), msText( ms[1] ), msText( ms[2] ), msText( ms[4] ) ).arg( rb )
		.arg( L.lastWallMs(), 0, 'f', 3 ).arg( L.lastRays() ).arg( statesOf( L.volume() ) ).arg( atlasBytes( L.volume().probeCount() ) )
		.arg( c.busy ).arg( tail ) );
}

void dumpLive( Cell & c, RtDdgiLive & L )
{
	QString dir = QString::fromLocal8Bit( wwEnv( "WW_CELL_RTDDGI_DUMP" ) ).trimmed();
	if ( dir.isEmpty() || c.dumped )
		return;
	if ( c.level > 0 )   // RTXCASCADE1: a coarse cascade's dump in a folder of its own under the key's
		dir += QStringLiteral( "/cascade%1" ).arg( c.level );
	c.dumped = true;
	L.finish();   // the step that copied the tiles back, done
	RtDdgiVolume v;
	L.snapshot( &v );
	QString err;
	if ( rtddgiDump( v, L.spec(), dir, &err ) )
		say( c.tag + QStringLiteral( "dumped %1 updates to %2" ).arg( L.updates() ).arg( QDir::toNativeSeparators( dir ) ) );
	else
		say( c.tag + QStringLiteral( "dump refused(%1)" ).arg( err ) );
}

// a unit sphere for the debug view: an octahedron split once (32 triangles), vertices on the sphere
const std::vector<Vector3> & sphereTris()
{
	static std::vector<Vector3> t;
	if ( !t.empty() )
		return t;
	const Vector3 ax[6] = { Vector3( 1, 0, 0 ), Vector3( -1, 0, 0 ), Vector3( 0, 1, 0 ), Vector3( 0, -1, 0 ), Vector3( 0, 0, 1 ), Vector3( 0, 0, -1 ) };
	auto unit = []( const Vector3 & v ) {
		const float l = std::sqrt( v[0] * v[0] + v[1] * v[1] + v[2] * v[2] );
		return Vector3( v[0] / l, v[1] / l, v[2] / l );
	};
	for ( int sx = 0; sx < 2; sx++ )
		for ( int sy = 2; sy < 4; sy++ )
			for ( int sz = 4; sz < 6; sz++ ) {
				const Vector3 a = ax[sx], b = ax[sy], cz = ax[sz];
				const Vector3 ab = unit( a + b ), bc = unit( b + cz ), ca = unit( cz + a );
				for ( const Vector3 & v : { a, ab, ca, ab, b, bc, ca, bc, cz, ab, bc, ca } )
					t.push_back( v );
			}
	return t;
}

/* lane RTXFIX1 (bungo 10-29 "Could our probes have a rounder shape?" + "fog"): WW_CELL_RTDDGI_VIEW_FOG, live:
 * unset / 1 = the probes view drawn INSIDE the linear frame (rtddgiCellViewDrawLinear: rounder spheres, the fog and the
 * exposure on them as on any surface); nofog = in the frame without the fog; 0 = the old overlay (after the tone map,
 * unfogged, 32 triangles a sphere, the tiles copied to the host) */
int viewFogMode()
{
	const QString v = wwEnvStr( "WW_CELL_RTDDGI_VIEW_FOG" ).trimmed().toLower();
	if ( v == QLatin1String( "0" ) || v == QLatin1String( "overlay" ) )
		return 0;
	return v == QLatin1String( "nofog" ) ? 2 : 1;
}
bool viewOverlay() { return viewFogMode() == 0; }

constexpr int kViewLevelNear = 3;           // lane RTXFIX1: 512 triangles a sphere within kViewNear of the camera
constexpr int kViewLevelFar = 2;            // and 128 beyond (the old view: 32)
constexpr double kViewNear = 800.0;         // view units

//! lane RTXFIX1: a unit sphere, the octahedron split `level` times (8 x 4^level triangles), corners on the sphere
std::vector<Vector3> sphereLevel( int level )
{
	auto unit = []( const Vector3 & v ) {
		const float l = std::sqrt( v[0] * v[0] + v[1] * v[1] + v[2] * v[2] );
		return Vector3( v[0] / l, v[1] / l, v[2] / l );
	};
	std::vector<Vector3> t;
	const Vector3 ax[6] = { Vector3( 1, 0, 0 ), Vector3( -1, 0, 0 ), Vector3( 0, 1, 0 ), Vector3( 0, -1, 0 ), Vector3( 0, 0, 1 ), Vector3( 0, 0, -1 ) };
	for ( int sx = 0; sx < 2; sx++ )
		for ( int sy = 2; sy < 4; sy++ )
			for ( int sz = 4; sz < 6; sz++ )
				for ( const Vector3 & v : { ax[sx], ax[sy], ax[sz] } )
					t.push_back( v );
	for ( int l = 0; l < level; l++ ) {
		std::vector<Vector3> n;
		n.reserve( t.size() * 4 );
		for ( size_t i = 0; i + 2 < t.size(); i += 3 ) {
			const Vector3 a = t[i], b = t[i + 1], c = t[i + 2];
			const Vector3 ab = unit( a + b ), bc = unit( b + c ), ca = unit( c + a );
			for ( const Vector3 & v : { a, ab, ca, ab, b, bc, ca, bc, c, ab, bc, ca } )
				n.push_back( v );
		}
		t.swap( n );
	}
	return t;
}

} // namespace

// ------------------------------------------------------------------ lane RESTIRDI1 (2026-10-30): the RT quality preset
/* bungo's ruling (10-29): Low = the ray-traced GI volume only (today), Medium = + ReSTIR DI (ray-traced shadows for the
 * shadow-casting lights in place of the 16 depth cubes), High = + ReSTIR GI, Ultra = + ReSTIR PT. High and Ultra are
 * Medium until those lanes land (the row's tooltip says so). WW_CELL_RT_PRESET (low|medium|high|ultra or 0..3) pins it,
 * else the Show menu row (WW/CellRtPreset, harness-scoped settings: a harness starts Low). Live: the row reloads the
 * shaders (WW_CELL_RESTIR is a loader define) and the next frame's pass imports or frees its buffers. */
namespace {

int presetParse( const QByteArray & v0 )
{
	const QByteArray v = v0.trimmed().toLower();
	if ( v == "medium" || v == "1" )
		return 1;
	if ( v == "high" || v == "2" )
		return 2;
	if ( v == "ultra" || v == "3" )
		return 3;
	return 0;
}

struct PresetRow
{
	bool loaded = false;
	int gui = 0;
};

PresetRow & presetRow()
{
	static PresetRow p;
	if ( !p.loaded ) {
		p.loaded = true;
		QSettings s( QStringLiteral( "NifTools" ), rowSettingsApp() );
		p.gui = std::clamp( s.value( QStringLiteral( "WW/CellRtPreset" ), 0 ).toInt(), 0, 3 );
	}
	return p;
}

//! WW_CELL_RESTIR_RED (live, read every frame): novis, nopdf, notemporal, noclamp, nohole (restir_common.glsl RESTIR_BIT_*)
int restirRedBits()
{
	int b = 0;
	for ( const QString & r : wwEnvStr( "WW_CELL_RESTIR_RED" ).split( QLatin1Char( ',' ), Qt::SkipEmptyParts ) ) {
		const QString t = r.trimmed();
		b |= t == QLatin1String( "novis" ) ? 1 : t == QLatin1String( "nopdf" ) ? 2 : t == QLatin1String( "notemporal" ) ? 4
			: t == QLatin1String( "noclamp" ) ? 8 : t == QLatin1String( "nohole" ) ? 16 : 0;
	}
	return b;
}

//! the pass's GL side: the shared g-buffer copy (a pixel-pack buffer) and the ratio (a texture buffer, RG32F)
struct RestirGl
{
	const void * owner = nullptr;
	const bakert::DdgiGpuSession * session = nullptr;
	int W = 0, H = 0;
	GLuint mem[2] = { 0, 0 }, buf[2] = { 0, 0 }, tex = 0, semVk = 0, semGl = 0;
	bool ready = false, history = false;
	quint32 frame = 0;
	qint64 runs = 0, busy = 0;
	float prows[12] = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0 }, pproj[4] = { 1, 1, 0, 0 };
	QString last;
};

RestirGl & restirGl()
{
	static RestirGl * g = new RestirGl;   // never destroyed (as cell())
	return *g;
}

void restirSay( RestirGl & G, const QString & line )
{
	if ( line == G.last )
		return;
	G.last = line;
	std::fprintf( stderr, "restir cell: %s\n", qPrintable( line ) );
	std::fflush( stderr );
}

void restirDropGl( RestirGl & G, Renderer * r )
{
	if ( r && G.owner == r ) {
		auto fn = r->fn;
		GlShare & sh = glShare();
		if ( G.tex )
			fn->glDeleteTextures( 1, &G.tex );
		for ( GLuint & b : G.buf )
			if ( b )
				fn->glDeleteBuffers( 1, &b );
		if ( sh.ok ) {
			for ( GLuint & m : G.mem )
				if ( m )
					sh.deleteMem( 1, &m );
			for ( GLuint * s : { &G.semVk, &G.semGl } )
				if ( *s )
					sh.deleteSem( 1, s );
		}
	}
	const QString last = G.last;
	G = RestirGl();
	G.last = last;
}

bool restirImport( RestirGl & G, Renderer * r, bakert::DdgiGpuSession * s, int W, int H, QString * why )
{
	GlShare & g = glShare();
	if ( !g.ok ) {
		*why = g.why;
		return false;
	}
	bakert::RestirGpuShare sh;
	if ( !bakert::restirGpuShare( s, W, H, &sh ) ) {
		*why = QString::fromStdString( sh.why );
		return false;
	}
	auto fn = r->fn;
	for ( int i = 0; i < 16 && fn->glGetError() != GL_NO_ERROR; i++ ) {}
	G.owner = r;
	const GLint dedicated = 1;
	void * mems[2] = { sh.gbufMemory, sh.outMemory };
	const quint64 sizes[2] = { sh.gbufSize, sh.outSize };
	const GLsizeiptr bytes[2] = { GLsizeiptr( W ) * H * 16, GLsizeiptr( W ) * H * 8 };
	const GLenum targets[2] = { GL_PIXEL_PACK_BUFFER, GL_TEXTURE_BUFFER };
	for ( int k = 0; k < 2; k++ ) {
		g.createMem( 1, &G.mem[k] );
		g.memParam( G.mem[k], kGlDedicatedMemoryObject, &dedicated );
		g.importMem( G.mem[k], sizes[k], kGlHandleOpaqueWin32, mems[k] );
		fn->glGenBuffers( 1, &G.buf[k] );
		fn->glBindBuffer( targets[k], G.buf[k] );
		g.bufferStorageMem( targets[k], bytes[k], G.mem[k], 0 );
		fn->glBindBuffer( targets[k], 0 );
	}
	g.genSem( 1, &G.semVk );
	g.importSem( G.semVk, kGlHandleOpaqueWin32, sh.vkDone );
	g.genSem( 1, &G.semGl );
	g.importSem( G.semGl, kGlHandleOpaqueWin32, sh.glDone );
	GLint prevActive = 0;
	fn->glGetIntegerv( GL_ACTIVE_TEXTURE, &prevActive );
	fn->glActiveTexture( GLenum( GL_TEXTURE0 + WwCellUnit::Restir ) );
	fn->glGenTextures( 1, &G.tex );
	fn->glBindTexture( GL_TEXTURE_BUFFER, G.tex );
	fn->glTexBuffer( GL_TEXTURE_BUFFER, GL_RG32F, G.buf[1] );
	fn->glBindTexture( GL_TEXTURE_BUFFER, 0 );
	fn->glActiveTexture( GLenum( prevActive ) );
	const GLenum err = fn->glGetError();
	for ( void * h : { sh.gbufMemory, sh.outMemory, sh.vkDone, sh.glDone } )
		bakert::ddgiGpuCloseHandle( h );
	if ( err != GL_NO_ERROR ) {
		*why = QStringLiteral( "the GL import failed (GL error 0x%1)" ).arg( qulonglong( err ), 0, 16 );
		return false;
	}
	G.session = s;
	G.W = W;
	G.H = H;
	return true;
}

//! the ratio's texture buffer and size on a program compiled under WW_CELL_RESTIR (cell_lights.glsl cellRestirAt)
void restirUniforms( Renderer * r, NifSkopeOpenGLContext::Program * prog )
{
	const int loc = prog->uniLocation( "cellRestirOn" );
	if ( loc < 0 )
		return;
	RestirGl & G = restirGl();
	const bool on = G.ready && G.owner == r && G.tex;
	auto fn = r->fn;
	GLint prevActive = 0;
	fn->glGetIntegerv( GL_ACTIVE_TEXTURE, &prevActive );
	fn->glActiveTexture( GLenum( GL_TEXTURE0 + WwCellUnit::Restir ) );
	fn->glBindTexture( GL_TEXTURE_BUFFER, on ? G.tex : 0 );
	fn->glActiveTexture( GLenum( prevActive ) );
	prog->uni1i( "cellRestir", WwCellUnit::Restir );
	prog->uni1b_l( loc, on );
	prog->uni4f( "cellRestirSize", FloatVector4( float( G.W ), float( G.H ), 0.0f, 0.0f ) );
}

} // namespace

bool rtddgiRtPresetPinned()
{
	return wwEnvIsSet( "WW_CELL_RT_PRESET" );
}

int rtddgiRtPreset()
{
	if ( wwEnvIsSet( "WW_CELL_RT_PRESET" ) )
		return presetParse( wwEnv( "WW_CELL_RT_PRESET" ) );
	return presetRow().gui;
}

void rtddgiRtPresetSet( int preset )
{
	PresetRow & p = presetRow();
	p.gui = std::clamp( preset, 0, 3 );
	rowWrite( QStringLiteral( "WW/CellRtPreset" ), p.gui );
}

bool rtddgiRestirShaderDefine()
{
	return rtddgiRtPreset() >= 1 && rtddgiCellShaderDefine();
}

void rtddgiRestirPass( Scene * scene )
{
	RestirGl & G = restirGl();
	G.ready = false;
	Renderer * r = scene ? scene->renderer : nullptr;
	if ( !r || scene->selecting || !scene->nifModel )
		return;
	if ( !rtddgiRestirShaderDefine() ) {
		if ( G.owner ) {
			restirDropGl( G, r );
			restirSay( G, QStringLiteral( "freed (the RT quality is Low)" ) );
		}
		return;
	}
	Cell & c = cell();
	if ( !c.live || !c.live->live.isOpen() || c.nif != scene->nifModel || c.glOwner != r ) {
		restirSay( G, QStringLiteral( "waiting for the ray-traced GI volume (ReSTIR traces its BVH); the shadow lights are unshadowed meanwhile" ) );
		return;
	}
	if ( !c.interop ) {
		restirSay( G, QStringLiteral( "refused(the volume has no GL interop: %1)" ).arg( c.interopWhy ) );
		return;
	}
	WwCellAoTargets t;
	if ( !wwCellAoTargets( scene, t ) || !t.gbuf || t.w <= 0 || t.h <= 0 ) {
		restirSay( G, QStringLiteral( "refused(no shared opaque pass this frame)" ) );
		return;
	}
	bakert::DdgiGpuSession * s = c.live->live.session();
	if ( G.owner != r || G.session != s || G.W != t.w || G.H != t.h ) {
		restirDropGl( G, r );
		QString why;
		if ( !restirImport( G, r, s, t.w, t.h, &why ) ) {
			restirDropGl( G, r );
			restirSay( G, QStringLiteral( "refused(%1)" ).arg( why ) );
			return;
		}
	}
	auto fn = r->fn;
	GlShare & sh = glShare();
	// the shared g-buffer into the device's buffer (a GPU copy through the pixel-pack binding)
	GLint prevActive = 0, prevPack = 0;
	fn->glGetIntegerv( GL_ACTIVE_TEXTURE, &prevActive );
	fn->glGetIntegerv( GL_PIXEL_PACK_BUFFER_BINDING, &prevPack );
	fn->glActiveTexture( GLenum( GL_TEXTURE0 + WwCellUnit::Restir ) );
	fn->glBindTexture( GL_TEXTURE_2D, t.gbuf );
	fn->glBindBuffer( GL_PIXEL_PACK_BUFFER, G.buf[0] );
	fn->glPixelStorei( GL_PACK_ALIGNMENT, 4 );
	fn->glGetTexImage( GL_TEXTURE_2D, 0, GL_RGBA, GL_FLOAT, nullptr );
	fn->glBindBuffer( GL_PIXEL_PACK_BUFFER, GLuint( prevPack ) );
	fn->glBindTexture( GL_TEXTURE_2D, 0 );
	fn->glActiveTexture( GLenum( prevActive ) );
	// the camera: view -> world rows (celllights.cpp cellRow), moved into the soup's local frame; the frame's projection
	bakert::RestirGpuIn in;
	const Transform & vt = scene->view;
	const float sc = vt.scale != 0.0f ? vt.scale : 1.0f;
	for ( int k = 0; k < 3; k++ ) {
		float w = 0.0f;
		for ( int j = 0; j < 3; j++ )
			w -= vt.rotation( j, k ) * vt.translation[j];
		for ( int j = 0; j < 3; j++ )
			in.rows[k * 4 + j] = vt.rotation( j, k ) / sc;
		in.rows[k * 4 + 3] = w / sc;
	}
	in.rows[3] = float( double( in.rows[3] ) - c.live->live.localX() );
	in.rows[7] = float( double( in.rows[7] ) - c.live->live.localY() );
	const auto & pm = r->globalUniforms->projectionMatrix;
	in.proj[0] = pm[0][0];
	in.proj[1] = pm[1][1];
	in.proj[2] = pm[2][0];
	in.proj[3] = pm[2][1];
	std::copy( G.prows, G.prows + 12, in.prows );
	std::copy( G.pproj, G.pproj + 4, in.pproj );
	in.history = G.history;
	in.W = t.w;
	in.H = t.h;
	in.frame = G.frame++;
	in.red = restirRedBits();
	const std::vector<float> & pl = c.live->live.packedLights();
	for ( size_t i = 0; i + 31 < pl.size(); i += 32 )
		if ( pl[i + 30] > 0.5f )
			in.cand.push_back( int( i / 32 ) );
	in.beforeSubmit = [&G, &sh, fn]() {
		sh.signalSem( G.semGl, 2, G.buf, 0, nullptr, nullptr );
		fn->glFlush();
		return true;
	};
	bakert::RestirGpuOut out;
	if ( !bakert::restirGpuStep( s, in, &out ) ) {
		G.history = false;
		G.busy += out.busy ? 1 : 0;
		restirSay( G, out.busy ? QStringLiteral( "busy (a bake holds the device): this frame unshadowed" )
			: QStringLiteral( "refused(%1)" ).arg( QString::fromStdString( out.why ) ) );
		return;
	}
	sh.waitSem( G.semVk, 2, G.buf, 0, nullptr, nullptr );
	G.ready = true;
	G.history = true;
	std::copy( in.rows, in.rows + 12, G.prows );
	std::copy( in.proj, in.proj + 4, G.pproj );
	if ( ( G.runs++ % 64 ) == 0 )
		restirSay( G, QStringLiteral( "on %1 x %2, %3 shadow lights of %4 (candidates %5, neighbours %6), frame %7, device wall %8 ms, red %9" )
			.arg( t.w ).arg( t.h ).arg( in.cand.size() ).arg( pl.size() / 32 ).arg( in.candidates ).arg( in.spatial ).arg( in.frame )
			.arg( out.msWall, 0, 'f', 3 ).arg( in.red ) );
}

bool rtddgiCellPinned()
{
	return wwEnvIsSet( "WW_CELL_RTDDGI" );
}

bool rtddgiCellOn()
{
	if ( wwEnvIsSet( "WW_CELL_RTDDGI" ) ) {
		const QByteArray v = wwEnv( "WW_CELL_RTDDGI" ).trimmed();
		return !v.isEmpty() && v != "0";
	}
	Cell & c = cell();
	loadRows( c );
	return c.guiOn || rtddgiRtPreset() >= 1;   // lane RESTIRDI1: Medium and up trace on the volume's BVH
}

void rtddgiCellSetOn( bool on )
{
	Cell & c = cell();
	loadRows( c );
	c.guiOn = on;
	rowWrite( QStringLiteral( "WW/CellRtDdgi" ), on );
}

bool rtddgiCellShaderDefine()
{
	return rtddgiCellOn() && wwCellGiVis() == 0;
}

bool rtddgiCellViewPinned()
{
	return wwEnvIsSet( "WW_CELL_RTDDGI_VIEW" );
}

int rtddgiCellView()
{
	if ( wwEnvIsSet( "WW_CELL_RTDDGI_VIEW" ) ) {
		const QByteArray v = wwEnv( "WW_CELL_RTDDGI_VIEW" ).trimmed().toLower();
		return v == "states" || v == "1" ? 1 : v == "irradiance" || v == "2" ? 2 : 0;
	}
	Cell & c = cell();
	loadRows( c );
	return c.guiView;
}

void rtddgiCellSetView( int mode )
{
	Cell & c = cell();
	loadRows( c );
	c.guiView = std::clamp( mode, 0, 2 );
	rowWrite( QStringLiteral( "WW/CellRtDdgiView" ), c.guiView );
}

/* RTXCASCADE2: one volume's ordered step this frame. Run at once (runOne: M3's one volume, the perblas red, the stop's copy
 * steps), or collected and submitted with the other cascades' in ONE submit (groupStep), then finished alike (stepDone). */
struct Deferred
{
	Cell * c = nullptr;
	RtDdgiStep o;
	int k = 0, max = 0;
	bool atMax = false, moved = false;
	int cells[3] = { 0, 0, 0 };
	float eye[3] = { 0, 0, 0 };
};
static quint64 s_cellFrame = 0;   // RTXCASCADE2: rtddgiCellFrame's count (the skip range reads the finer cascade's order of this frame)
static bool s_jump = false;       // RTXCASCADE2: cascade 0 jumped this frame (rtddgimath.h cascadeJump)

//! RTXCASCADE2: the cascade the debug views draw (the row's pick, within the cascades there are)
static int viewLevel()
{
	return std::max( 0, std::min( rtddgiCellViewCascade(), rtddgiCellCascades() - 1 ) );
}

/* with interop the submit is ordered against GL both ways (GL signals glDone before it, so the device writes only after
 * GL's reads so far; GL waits on vkDone after it, before any read of this frame). True = this step orders itself */
template <class F>
static bool stepGlBefore( Cell & c, RtDdgiStep & o, F * fn )
{
	GlShare & g = glShare();
	const bool gl = c.interop && g.ok && !( cellRedPaths() & kPathNoWait );   // lane RTXFIX1: the red drops the order
	if ( gl ) {
		Cell * cp = &c;
		GlShare * gp = &g;
		o.beforeSubmit = [cp, gp, fn]() {
			if ( !cp->glRead )
				return false;
			gp->signalSem( cp->semGl, 1, &cp->buf, 0, nullptr, nullptr );
			cp->glRead = false;
			cp->syncSignals++;
			fn->glFlush();   // the signal reaches the GPU before the device's submit waits on it
			return true;
		};
	}
	return gl;
}

static void stepGlAfter( Cell & c, const RtDdgiLive & L, bool ok, bool gl )
{
	GlShare & g = glShare();
	if ( ok && gl && L.submitted() ) {   // every such submit signalled vkDone: GL waits on it exactly once
		QElapsedTimer t;
		t.start();
		g.waitSem( c.semVk, 1, &c.buf, 0, nullptr, nullptr );
		c.glWaitMs = double( t.nsecsElapsed() ) / 1e6;
		c.glRead = true;
		c.syncWaits++;
		c.uploaded = true;
		c.fresh = true;
	} else if ( ok && c.interop && !gl && L.submitted() ) {
		c.uploaded = true;   // the nowait red: GL reads the device's atlas with no order at all
		c.fresh = true;
	}
}

//! the rest of a frame's step once it ran (or was refused): the upload, the census, the stop and the dump (M3's tail)
static void stepDone( Renderer * r, Deferred & j, bool ok )
{
	Cell & c = *j.c;
	if ( !c.live )
		return;
	RtDdgiLive & L = c.live->live;
	if ( !ok ) {
		if ( L.busy() ) {
			c.busy++;
			return;
		}
		refuse( c, QStringLiteral( "an update failed: %1" ).arg( L.why() ) );
		dropLive( c, r );   // the key stays: no rebuild until an input moves
		return;
	}
	c.sets++;
	if ( !c.interop ) {
		if ( j.moved )
			c.probesPacked = false;   // the states moved with the scroll (and came back with it: the step waited)
		if ( !upload( c, r, L ) ) {
			refuse( c, QStringLiteral( "no tiles came back" ) );
			return;
		}
		c.fresh = true;
	}
	if ( j.moved && ( L.scrolls() <= kScrollEcho || L.scrolls() % 64 == 0 ) ) {
		const RtDdgiVolume & v = L.volume();
		say( c.tag + QStringLiteral( "scroll %1: scrolled %2 cells (%3 %4 %5), reset %6 probes (%7 in all); first probe %8 %9 %10; camera %11 %12 %13%14" )
			.arg( L.scrolls() ).arg( L.lastScrollCells() ).arg( j.cells[0] ).arg( j.cells[1] ).arg( j.cells[2] ).arg( L.lastScrollFresh() )
			.arg( L.freshTotal() ).arg( double( v.origin[0] ), 0, 'f', 0 ).arg( double( v.origin[1] ), 0, 'f', 0 )
			.arg( double( v.origin[2] ), 0, 'f', 0 ).arg( double( j.eye[0] ), 0, 'f', 0 ).arg( double( j.eye[1] ), 0, 'f', 0 )
			.arg( double( j.eye[2] ), 0, 'f', 0 )
			.arg( j.o.resetAll ? QStringLiteral( " (a camera jump: every plane reset)" ) : QString() ) );   // RTXCASCADE2
	}
	const bool pow2 = ( c.sets & ( c.sets - 1 ) ) == 0;
	if ( pow2 || j.atMax || c.sets % 256 == 0 ) {
		QString tail = j.atMax ? QStringLiteral( "; stopped at the spec's max (%1 updates since the build or the last scroll); %2" )
			.arg( j.max ).arg( cameraWords( L.volume(), j.eye ) ) : QString();
		if ( L.lastSkipped() > 0 )   // RTXCASCADE2: the covered-probe skip (WW_CELL_RTDDGI_SPEC cskip=1)
			tail += QStringLiteral( "; skips %1 probes the finer cascade covers" ).arg( L.lastSkipped() );
		censusSet( c, L, j.k, tail );
	}
	if ( j.atMax ) {
		c.stopped = true;
		dumpLive( c, L );
	} else if ( j.max <= 0 && c.sets == 64 ) {
		dumpLive( c, L );
	}
}

static void runOne( Renderer * r, Deferred & j )
{
	Cell & c = *j.c;
	RtDdgiLive & L = c.live->live;
	const bool gl = stepGlBefore( c, j.o, r->fn );
	const bool ok = L.step( j.o );
	stepGlAfter( c, L, ok, gl );
	stepDone( r, j, ok );
}

/* RTXCASCADE2: every cascade due this frame in one submit (RtDdgiLive::stepGroup: one command buffer, densest first);
 * volumes not on one BLAS (a coarse one that could not take cascade 0's) step one by one as before */
static void groupStep( Renderer * r, std::vector<Deferred> & J )
{
	const size_t n = J.size();
	if ( n == 0 )
		return;
	bool one = n > 1;
	for ( size_t i = 1; i < n && one; i++ )
		one = J[i].c->live->live.sameScene( J[0].c->live->live );
	if ( !one ) {
		for ( Deferred & j : J )
			runOne( r, j );
		J.clear();
		return;
	}
	std::vector<RtDdgiLive *> lives( n );
	std::vector<RtDdgiStep> orders( n );
	std::vector<char> gls( n, 0 );
	for ( size_t i = 0; i < n; i++ ) {
		gls[i] = stepGlBefore( *J[i].c, J[i].o, r->fn ) ? 1 : 0;
		lives[i] = &J[i].c->live->live;
		orders[i] = J[i].o;
	}
	const bool ok = RtDdgiLive::stepGroup( lives.data(), orders.data(), int( n ) );
	for ( size_t i = 0; i < n; i++ ) {
		stepGlAfter( *J[i].c, *lives[i], ok, gls[i] != 0 );
		stepDone( r, J[i], ok );
	}
	J.clear();
}

/*! one volume's frame (M3's rtddgiCellFrame body): cascade 0 = cell() with the spec of the key and due every frame;
 *  RTXCASCADE1: a coarse cascade with its own spec (spIn) steps only on its due frames (a build is harvested on any) */
static void volumeFrame( Scene * scene, Cell & c, const CellSpec * spIn, bool due, bool resetAll = false,
	std::vector<Deferred> * defer = nullptr )
{
	if ( !scene || !scene->nifModel || !scene->renderer )
		return;
	const void * nif = scene->nifModel;
	Renderer * r = scene->renderer;
	if ( !rtddgiCellShaderDefine() ) {   // off (or the march red): nothing kept, nothing bound
		if ( c.pending || c.live || c.buf ) {
			dropLive( c, r );
			c.key = 0;
			c.nif = nullptr;
			say( QStringLiteral( "freed (the switch is off)" ) );
		}
		c.refused.clear();
		return;
	}
	ProbeSkyLight sky;
	WwCellLighting lights;
	const ProbeSoup * soup = probeSkyKeptSoup( nif, &sky, &lights );
	if ( !soup ) {
		if ( c.nif == nif && ( c.pending || c.live ) ) {
			dropLive( c, r );
			c.key = 0;
		}
		refuse( c, QStringLiteral( "no relit cell in this view: the volume traces the soup the Precomputed Radiance Transfer relight keeps" ) );
		return;
	}
	if ( lights.interior && lights.skyLighting && sky.on )
		skyLitSky( nif, sky );   // lane SKYINT1: the current weather and hour
	const WwCellGi * G = wwCellGiFor( nif );
	const CellSpec sp = spIn ? *spIn : cellSpecNow();
	const int paths = cellRedPaths();
	float eye[3];
	eyeOf( scene->view, eye );
	// what the volume is built from; the camera is not in it (M3: the volume scrolls; the rebuild red rebuilds below)
	quint64 key = 0xCBF29CE484222325ULL;
	key = fnv( key, &nif, sizeof nif );
	key = fnv( key, &soup, sizeof soup );
	const size_t nTris = soup->tris.size();
	key = fnv( key, &nTris, sizeof nTris );
	if ( nTris >= 9 ) {
		key = fnv( key, soup->tris.data(), 36 );
		key = fnv( key, soup->tris.data() + nTris - 9, 36 );
	}
	const int skyOn = sky.on ? 1 : 0;
	key = fnv( key, &skyOn, sizeof skyOn );
	key = fnv( key, sky.amb, sizeof sky.amb );
	key = fnv( key, sky.sun, sizeof sky.sun );
	key = fnv( key, sky.sunTo, sizeof sky.sunTo );
	const quint64 lk = rtddgiLightsKey( lights );
	key = fnv( key, &lk, sizeof lk );
	const QByteArray st = sp.text.toUtf8();
	key = fnv( key, st.constData(), size_t( st.size() ) );
	key = fnv( key, &paths, sizeof paths );
	if ( G ) {
		key = fnv( key, G->origin, sizeof G->origin );
		key = fnv( key, G->dims, sizeof G->dims );
		key = fnv( key, &G->voxel, sizeof G->voxel );
		key = fnv( key, &G->pad, sizeof G->pad );
	}
	QString rebuildWhy = QStringLiteral( "the soup, lights, sky, spec, reds or GI grid moved" );
	int cells[3] = { 0, 0, 0 };
	bool moved = false;
	if ( due && key == c.key && c.nif == nif && c.live )
		moved = scrollFor( *c.live, c.live->live.volume(), eye, cells );
	// RTXCASCADE2: a coarse cascade on a camera-jump frame resets every plane (a scroll that keeps none), moved or not
	if ( resetAll && due && key == c.key && c.nif == nif && c.live && !( c.live->paths & kPathRebuild ) )
		moved = true;
	if ( moved && ( c.live->paths & kPathRebuild ) ) {   // the red: M2's volume built again round the camera
		rebuildWhy = QStringLiteral( "the camera moved %1 %2 %3 cells (the rebuild red)" ).arg( cells[0] ).arg( cells[1] ).arg( cells[2] );
		c.key = 0;
		moved = false;
	}
	if ( key != c.key || c.nif != nif ) {
		const bool had = c.pending || c.live;
		dropLive( c, r );
		c.key = key;
		c.nif = nif;
		c.refused.clear();
		startBuild( c, *soup, lights, sky, G, eye, sp, paths,
			c.level > 0 && !( paths & kPathPerCascade ) ? cell().live : nullptr );   // RTXCASCADE2: one BLAS
		say( c.tag + QStringLiteral( "%1 (%2 triangles)" ).arg( had ? QStringLiteral( "rebuilding: " ) + rebuildWhy : QStringLiteral( "building" ) )
			.arg( qint64( nTris / 9 ) ) );
	}
	if ( c.pending ) {
		std::shared_ptr<Build> b = c.pending;
		std::lock_guard<std::mutex> lk2( b->m );
		if ( b->done ) {
			c.pending.reset();
			if ( b->ok ) {
				c.live = b;
				c.interopWhy.clear();
				if ( !( b->paths & kPathReadback ) ) {
					QString why;
					if ( !importShare( c, r, b->live, &why ) )
						c.interopWhy = QStringLiteral( "interop refused: " ) + why;
				} else {
					c.interopWhy = QStringLiteral( "WW_CELL_RTDDGI_RED=readback" );
				}
				censusBuilt( c, *b );
				// the camera may have moved during the build: a scroll now (the rebuild red builds again next frame)
				moved = !( b->paths & kPathRebuild ) && scrollFor( *b, b->live.volume(), eye, cells );
			} else {
				refuse( c, b->why );
			}
		}
	}
	if ( c.level == 0 && moved && c.live )   // RTXCASCADE2: every plane of cascade 0 reset = the camera jumped
		s_jump = rtddgi::cascadeJump( cells, c.live->live.volume().dims );
	if ( !c.live || !due )
		return;
	RtDdgiLive & L = c.live->live;
	const int view = c.level == viewLevel() ? rtddgiCellView() : 0;   // RTXCASCADE2: the debug views' cascade (the row)
	const bool wantDump = !c.dumped && !wwEnvStr( "WW_CELL_RTDDGI_DUMP" ).trimmed().isEmpty();
	if ( moved )
		c.stopped = false;   // a scroll re-arms the spec's max: the new planes converge too
	if ( c.stopped ) {
		/* the irradiance view and a dump need the tiles on the host: one copy step (k = 0). A dump asked for after the
		 * stop is written now (M2 wrote it only at the stop: a row that set the key on a volume an earlier row had
		 * already stopped, with nothing to rebuild it, never dumped: M2's mem_rt_dump) */
		if ( ( view == 2 && viewOverlay() && !L.irr() ) || ( wantDump && !L.irr() ) ) {   // lane RTXFIX1: the in-frame view reads the atlas
			RtDdgiStep o;
			o.k = 0;
			const bool gl = stepGlBefore( c, o, r->fn );
			stepGlAfter( c, L, L.step( o ), gl );
		}
		if ( wantDump && L.irr() )
			dumpLive( c, L );
		return;
	}
	const int max = c.live->spec.max;
	int k = c.live->spec.updates;
	if ( max > 0 && !moved )
		k = std::min( k, max - L.updatesSinceScroll() );
	if ( k <= 0 ) {
		c.stopped = true;
		return;
	}
	const bool atMax = max > 0 && ( moved ? 0 : L.updatesSinceScroll() ) + k >= max;
	Deferred j;
	j.c = &c;
	j.k = k;
	j.max = max;
	j.atMax = atMax;
	j.moved = moved;
	RtDdgiStep & o = j.o;
	o.k = k;
	for ( int a = 0; a < 3; a++ ) {
		o.scroll[a] = moved ? cells[a] : 0;
		j.cells[a] = cells[a];
		j.eye[a] = eye[a];
	}
	o.resetAll = moved && resetAll;   // RTXCASCADE2: a camera jump keeps no plane of a coarse cascade
	// interop: no host copy unless the debug view or a dump at this step needs one; the step then waits for it
	// lane RTXFIX1: the in-frame irradiance view reads the atlas on the GPU; only the overlay (WW_CELL_RTDDGI_VIEW_FOG=0) needs the tiles here
	o.host = !c.interop || ( view == 2 && viewOverlay() ) || ( wantDump && ( atMax || ( max <= 0 && c.sets + 1 == 64 ) ) );
	o.wait = !c.interop || o.host;
	/* RTXCASCADE2 cost lever (cskip=1): skip the probes the next finer cascade covers whole, on its grid as it stands after
	 * its own step this frame (it was ordered first: densest first) */
	const RtDdgiVolume & vNow = L.volume();
	if ( c.level > 0 && sp.cskip ) {
		const Cell & f = cascCell( c.level - 1 );
		if ( f.live && f.nif == nif ) {
			const RtDdgiVolume & fv = f.live->live.volume();
			o.skip = true;
			for ( int a = 0; a < 3; a++ ) {
				o.fineOrigin[a] = f.orderFrame == s_cellFrame ? f.orderOrigin[a] : double( fv.origin[a] );
				o.fineStep[a] = double( fv.step[a] );
				o.fineDims[a] = fv.dims[a];
			}
		}
	}
	c.orderFrame = s_cellFrame;
	for ( int a = 0; a < 3; a++ )
		c.orderOrigin[a] = double( vNow.origin[a] ) + double( o.scroll[a] ) * double( vNow.step[a] );
	if ( defer ) {   // RTXCASCADE2: submitted with the other cascades' steps (groupStep)
		defer->push_back( j );
		return;
	}
	runOne( r, j );
}

int rtddgiCellCascades()
{
	if ( !wwEnvIsSet( "WW_CELL_RTDDGI_CASCADES" ) ) {   // RTXCASCADE2: the row (default 1: M3's one volume)
		Cell & c = cell();
		loadRows( c );
		return c.guiCascades;
	}
	const QByteArray v = wwEnv( "WW_CELL_RTDDGI_CASCADES" ).trimmed();
	bool ok = false;
	const int n = v.toInt( &ok );
	return ok ? std::clamp( n, 1, rtddgi::kMaxCascades ) : 1;   // unset, 0, 1 or not a number: M3's one volume
}

bool rtddgiCellCascadesPinned()
{
	return wwEnvIsSet( "WW_CELL_RTDDGI_CASCADES" );
}

void rtddgiCellSetCascades( int n )
{
	Cell & c = cell();
	loadRows( c );
	c.guiCascades = std::clamp( n, 1, rtddgi::kMaxCascades );
	rowWrite( QStringLiteral( "WW/CellRtDdgiCascades" ), c.guiCascades );
}

int rtddgiCellViewCascade()
{
	if ( wwEnvIsSet( "WW_CELL_RTDDGI_VIEW_CASCADE" ) ) {   // RTXCASCADE2: pins the debug views' cascade (0 = the nearest)
		bool ok = false;
		const int l = wwEnv( "WW_CELL_RTDDGI_VIEW_CASCADE" ).trimmed().toInt( &ok );
		return ok ? std::clamp( l, 0, rtddgi::kMaxCascades - 1 ) : 0;
	}
	Cell & c = cell();
	loadRows( c );
	return c.guiViewCascade;
}

bool rtddgiCellViewCascadePinned()
{
	return wwEnvIsSet( "WW_CELL_RTDDGI_VIEW_CASCADE" );
}

void rtddgiCellSetViewCascade( int level )
{
	Cell & c = cell();
	loadRows( c );
	c.guiViewCascade = std::clamp( level, 0, rtddgi::kMaxCascades - 1 );
	rowWrite( QStringLiteral( "WW/CellRtDdgiViewCascade" ), c.guiViewCascade );
}

//! RTXCASCADE1: the cascades' texture buffer, freed (its own context only, as freeGl)
static void freeCombined( Renderer * r )
{
	Combined & K = combined();
	if ( r && K.owner == r ) {
		if ( K.tex )
			r->fn->glDeleteTextures( 1, &K.tex );
		if ( K.buf )
			r->fn->glDeleteBuffers( 1, &K.buf );
	}
	K = Combined();
}

/*! RTXCASCADE1: the ready cascades (from 0, each live with tiles in its own buffer in this context) copied into the one
 *  texture buffer: a cascade whose buffer has new tiles (fresh), or all of them when the layout moved. GL commands
 *  only, after each cascade's wait on its submit (runStep): the copies read the finished atlas */
static void combinedUpdate( Renderer * r, int n, const void * nif )
{
	Combined & K = combined();
	if ( K.owner != r )
		freeCombined( nullptr );
	int m = 0, nP[rtddgi::kMaxCascades] = {};
	for ( int l = 0; l < n; l++ ) {
		const Cell & c = cascCell( l );
		if ( !c.live || !c.uploaded || !c.buf || c.glOwner != r || c.nif != nif )
			break;
		nP[l] = c.live->live.volume().probeCount();
		m++;
	}
	K.count = 0;   // nothing reads it until the copies below are in
	if ( !m )
		return;
	auto fn = r->fn;
	bool relayout = false;
	size_t texels = 0;
	for ( int l = 0; l < m; l++ ) {
		relayout = relayout || K.base[l] != int( texels ) || K.nP[l] != nP[l];
		K.base[l] = int( texels );
		K.nP[l] = nP[l];
		texels += size_t( nP[l] ) * kAtlasTexels;
	}
	const size_t bytes = texels * 16;
	if ( !K.buf ) {
		fn->glGenBuffers( 1, &K.buf );
		fn->glGenTextures( 1, &K.tex );
		K.owner = r;
	}
	QElapsedTimer t;
	t.start();
	if ( bytes > K.bytes ) {   // grows only: a smaller layout reads the front of it
		fn->glBindBuffer( GL_COPY_WRITE_BUFFER, K.buf );
		fn->glBufferData( GL_COPY_WRITE_BUFFER, GLsizeiptr( bytes ), nullptr, GL_DYNAMIC_COPY );
		fn->glBindBuffer( GL_COPY_WRITE_BUFFER, 0 );
		K.bytes = bytes;
		K.attach = true;
		relayout = true;
	}
	for ( int l = 0; l < m; l++ ) {
		Cell & c = cascCell( l );
		if ( !relayout && !c.fresh )
			continue;
		fn->glBindBuffer( GL_COPY_READ_BUFFER, c.buf );
		fn->glBindBuffer( GL_COPY_WRITE_BUFFER, K.buf );
		fn->glCopyBufferSubData( GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER, 0, GLintptr( size_t( K.base[l] ) * 16 ),
			GLsizeiptr( size_t( nP[l] ) * kAtlasTexels * 16 ) );
		c.fresh = false;
		K.copies++;
	}
	fn->glBindBuffer( GL_COPY_READ_BUFFER, 0 );
	fn->glBindBuffer( GL_COPY_WRITE_BUFFER, 0 );
	K.copyMs = double( t.nsecsElapsed() ) / 1e6;
	K.count = m;
}

/*! RTXCASCADE1: cascades 1..n-1 after cascade 0's frame. Cascade L = cascade 0's built step x 2^L (rtddgimath.h
 *  cascadeScale), its window x 2^L; built only while cascade 0 is up (its step sizes them) */
static void cascadesFrame( Scene * scene, std::vector<Deferred> * defer )
{
	if ( !scene || !scene->nifModel || !scene->renderer )
		return;
	Renderer * r = scene->renderer;
	const int n = rtddgiCellShaderDefine() ? rtddgiCellCascades() : 1;
	for ( int l = std::max( n, 1 ); l < rtddgi::kMaxCascades; l++ ) {   // past the count: freed
		Cell & c = cascCell( l );
		if ( c.pending || c.live || c.buf ) {
			dropLive( c, r );
			c.key = 0;
			c.nif = nullptr;
			say( c.tag + QStringLiteral( "freed (WW_CELL_RTDDGI_CASCADES %1)" ).arg( n ) );
		}
		c.refused.clear();
	}
	if ( n <= 1 ) {
		if ( combined().buf )
			freeCombined( r );
		return;
	}
	static quint64 frame = 0;
	frame++;
	const CellSpec sp = cellSpecNow();
	const Cell & c0 = cell();
	for ( int l = 1; l < n; l++ ) {
		Cell & c = cascCell( l );
		if ( !c0.live || c0.nif != scene->nifModel ) {   // cascade 0 building or refused: no step to size this one from
			if ( c.pending || c.live ) {
				dropLive( c, r );
				c.key = 0;
			}
			continue;
		}
		const RtDdgiVolume & v0 = c0.live->live.volume();
		const double k = rtddgi::cascadeScale( l );
		CellSpec q = sp;
		q.spacing = float( double( v0.step[0] ) * k );
		q.zscale = v0.step[0] > 0.0f ? v0.step[2] / v0.step[0] : sp.zscale;
		q.reach = float( double( sp.reach ) * k );
		q.text = sp.text + QStringLiteral( "|cascade %1 of step %2 x %3" ).arg( l ).arg( double( v0.step[0] ), 0, 'f', 3 )
			.arg( double( v0.step[2] ), 0, 'f', 3 );
		q.rays = rtddgi::cascadeRays( sp.rays, sp.crays, l );   // RTXCASCADE2 cost lever (crays; 0 = cascade 0's)
		/* with a spec max (pictures): every frame, so a camera move scrolls it at once and rtddgiCellPending holds the
		 * picture until it has its updates again (a stopped volume that did not move steps nothing); running: every
		 * cevery^L frames (the cost lever). RTXCASCADE2: a camera jump (cascade 0 kept no plane) steps every cascade
		 * this frame and resets every plane (rtddgimath.h cascadeStepPlan) */
		bool due = false, resetAll = false;
		rtddgi::cascadeStepPlan( l, frame, sp.every, s_jump, due, resetAll );
		due = due || sp.max > 0;
		// the census prefix carries the period: device ms a frame = sum over cascades of a set's ms / its period
		c.tag = QStringLiteral( "cascade %1 (every %2 frames): " ).arg( l ).arg( qlonglong( std::llround( std::pow( double( std::max( sp.every, 1 ) ), l ) ) ) );
		// RTXCASCADE2: cascade 0 was built again on a new BLAS (its own key moved): open this one again on it (one BLAS)
		if ( c.live && !c.pending && !( cellRedPaths() & kPathPerCascade ) && !c.live->live.sameScene( c0.live->live )
			&& c.live->live.sceneWhy().isEmpty() )
			c.key = 0;
		volumeFrame( scene, c, &q, due, resetAll, defer );
	}
	if ( defer )
		groupStep( r, *defer );   // RTXCASCADE2: every cascade due this frame in one submit, before the copies read them
	combinedUpdate( r, n, scene->nifModel );
}

void rtddgiCellFrame( Scene * scene )
{
	s_cellFrame++;
	s_jump = false;
	/* RTXCASCADE2: with cascades (and not the perblas red) the cascades' steps are collected and go in one submit at the
	 * end of cascadesFrame; one volume (WW_CELL_RTDDGI_CASCADES 1, the default) steps at once as M3's did */
	const int n = rtddgiCellShaderDefine() ? rtddgiCellCascades() : 1;
	std::vector<Deferred> jobs;
	std::vector<Deferred> * defer = n > 1 && !( cellRedPaths() & kPathPerCascade ) ? &jobs : nullptr;
	volumeFrame( scene, cell(), nullptr, true, false, defer );   // M3's volume, cascade 0
	cascadesFrame( scene, defer );                               // RTXCASCADE1: nothing while WW_CELL_RTDDGI_CASCADES is 1
	if ( !jobs.empty() && scene && scene->renderer )
		groupStep( scene->renderer, jobs );   // cascade 0's, when cascadesFrame left early
}

bool rtddgiCellPending()
{
	if ( !rtddgiCellShaderDefine() )
		return false;
	const Cell & c0 = cell();
	if ( c0.nif && skyLit().nif == c0.nif && skyLit().waiting )
		return true;   // lane SKYINT1: a new sky waits to stand still, then builds
	const int n = rtddgiCellCascades();
	for ( int l = 0; l < n; l++ ) {   // RTXCASCADE1: every cascade (n = 1: M3's test exactly)
		const Cell & c = cascCell( l );
		if ( c.pending )
			return true;   // the worker's build (a failed one is harvested by the next frame, then nothing is pending)
		if ( c.live && c.live->spec.max > 0 && !c.stopped )
			return true;
		if ( l > 0 && !c.live && cell().live && c.refused.isEmpty() )
			return true;   // a coarse cascade not started yet (it starts on the frame after cascade 0 is up)
	}
	return false;
}

/* the one-volume read's uniforms and texture buffer from volume `c` (M3's; RTXCASCADE2: the probes view passes the
 * picked cascade's own volume, in M3's layout) */
static void bindOne( Scene * scene, Renderer * r, NifSkopeOpenGLContext::Program * prog, int locOn, Cell & c )
{
	const bool ready = c.live && c.uploaded && c.tex && c.glOwner == r && c.nif == scene->nifModel;
	auto fn = r->fn;
	GLint prevActive = 0;
	fn->glGetIntegerv( GL_ACTIVE_TEXTURE, &prevActive );
	fn->glActiveTexture( GLenum( GL_TEXTURE0 + WwCellUnit::DdgiAtlas ) );
	fn->glBindTexture( GL_TEXTURE_BUFFER, ready ? c.tex : 0 );
	if ( ready && c.attach ) {
		fn->glTexBuffer( GL_TEXTURE_BUFFER, GL_RGBA32F, c.buf );
		c.attach = false;
	}
	fn->glActiveTexture( GLenum( prevActive ) );
	prog->uni1i( "cellDdgiAtlas", WwCellUnit::DdgiAtlas );
	prog->uni1b_l( locOn, ready );
	if ( !ready )
		return;
	const RtDdgiVolume & v = c.live->live.volume();
	const float D = std::min( v.step[0], std::min( v.step[1], v.step[2] ) );
	prog->uni4f( "cellDdgiOrigin", FloatVector4( v.origin[0], v.origin[1], v.origin[2], D ) );
	prog->uni4f( "cellDdgiStep", FloatVector4( v.step[0], v.step[1], v.step[2], c.live->live.spec().bias ) );
	const int locDims = prog->uniLocation( "cellDdgiDims" );
	if ( locDims >= 0 )
		fn->glUniform4i( locDims, v.dims[0], v.dims[1], v.dims[2], v.probeCount() );
	prog->uni1i( "cellDdgiRed", cellRedBits() );
}

void rtddgiCellUniforms( Scene * scene )
{
	Renderer * r = scene ? scene->renderer : nullptr;
	NifSkopeOpenGLContext::Program * prog = r ? r->getCurrentProgram() : nullptr;
	if ( !prog )
		return;
	restirUniforms( r, prog );   // lane RESTIRDI1: a program compiled under WW_CELL_RESTIR (else no location: nothing)
	const int locOn = prog->uniLocation( "cellDdgiOn" );
	if ( locOn < 0 )
		return;   // a program without the read: with the switch off, every program
	Cell & c = cell();
	// RTXCASCADE1: a program compiled under WW_CELL_RTDDGI_CASCADES reads the cascades' one texture buffer
	if ( const int locCount = prog->uniLocation( "cellDdgiCount" ); locCount >= 0 ) {
		Combined & K = combined();
		int cnt = 0;   // the cascades still live (the copy's count was taken this frame, after every drop)
		while ( cnt < K.count && cascCell( cnt ).live )
			cnt++;
		const bool on = cnt > 0 && K.owner == r && K.tex && c.nif == scene->nifModel;
		auto fn = r->fn;
		GLint prevActive = 0;
		fn->glGetIntegerv( GL_ACTIVE_TEXTURE, &prevActive );
		fn->glActiveTexture( GLenum( GL_TEXTURE0 + WwCellUnit::DdgiAtlas ) );
		fn->glBindTexture( GL_TEXTURE_BUFFER, on ? K.tex : 0 );
		if ( on && K.attach ) {
			fn->glTexBuffer( GL_TEXTURE_BUFFER, GL_RGBA32F, K.buf );
			K.attach = false;
		}
		fn->glActiveTexture( GLenum( prevActive ) );
		prog->uni1i( "cellDdgiAtlas", WwCellUnit::DdgiAtlas );
		prog->uni1b_l( locOn, on );
		if ( !on )
			return;
		GLfloat org[4 * rtddgi::kMaxCascades] = {}, stp[4 * rtddgi::kMaxCascades] = {};
		GLint dims[4 * rtddgi::kMaxCascades] = {}, base[rtddgi::kMaxCascades] = {};
		for ( int l = 0; l < cnt; l++ ) {
			const RtDdgiLive & L = cascCell( l ).live->live;
			const RtDdgiVolume & v = L.volume();
			for ( int a = 0; a < 3; a++ ) {
				org[4 * l + a] = v.origin[a];
				stp[4 * l + a] = v.step[a];
				dims[4 * l + a] = v.dims[a];
			}
			org[4 * l + 3] = std::min( v.step[0], std::min( v.step[1], v.step[2] ) );
			stp[4 * l + 3] = L.spec().bias;
			dims[4 * l + 3] = v.probeCount();
			base[l] = K.base[l];
		}
		fn->glUniform1i( locCount, cnt );
		const int locO = prog->uniLocation( "cellDdgiCOrigin" ), locS = prog->uniLocation( "cellDdgiCStep" ),
			locD = prog->uniLocation( "cellDdgiCDims" ), locB = prog->uniLocation( "cellDdgiCBase" );
		if ( locO >= 0 )
			fn->glUniform4fv( locO, cnt, org );
		if ( locS >= 0 )
			fn->glUniform4fv( locS, cnt, stp );
		if ( locD >= 0 )
			fn->glUniform4iv( locD, cnt, dims );
		if ( locB >= 0 )
			fn->glUniform1iv( locB, cnt, base );
		prog->uni1i( "cellDdgiRed", cellRedBits() );
		return;
	}
	bindOne( scene, r, prog, locOn, c );
}

void rtddgiCellViewDraw( Scene * scene, const Transform & viewTrans )
{
	if ( !scene || !scene->renderer || scene->selecting || !rtddgiCellShaderDefine() )
		return;
	const int mode = rtddgiCellView();
	Cell & c = cell();
	const bool inFrame = c.viewDrewInFrame;   // lane RTXFIX1: drawn inside the linear frame this paint: no overlay
	c.viewDrewInFrame = false;
	const Cell & vc = cascCell( viewLevel() );   // RTXCASCADE2: the picked cascade (0 = M3's volume)
	if ( !mode || !vc.live || vc.nif != scene->nifModel || inFrame )
		return;
	const RtDdgiLive & L = vc.live->live;
	const RtDdgiVolume & v = L.volume();
	const float * irr = L.irr();
	const quint32 * age = L.age();
	if ( mode == 2 && ( !irr || !age ) )
		return;
	const std::vector<Vector3> & sph = sphereTris();
	const float rad = 0.12f * std::min( v.step[0], std::min( v.step[1], v.step[2] ) );
	c.vp.clear();
	c.vc.clear();
	size_t drawn = 0;
	const int nP = v.probeCount();
	for ( int p = 0; p < nP && drawn < kViewMaxProbes; p++ ) {
		float pos[3];
		v.position( p, pos );
		const Vector3 P( pos[0], pos[1], pos[2] );
		if ( double( ( viewTrans * P ).length() ) > kViewReach )   // view units, as the PRTP view's rings (cellvolumeview.cpp)
			continue;
		drawn++;
		const int st = v.state[size_t( p )];
		FloatVector4 flat( 0.2f, 0.9f, 0.3f, 1.0f );   // Vigilant green; Asleep blue; Off red; fresh (a scroll's, not back yet) white
		if ( st == rtddgi::kAsleep )
			flat = FloatVector4( 0.35f, 0.45f, 0.95f, 1.0f );
		else if ( st == rtddgi::kOff )
			flat = FloatVector4( 0.95f, 0.15f, 0.15f, 1.0f );
		else if ( st == rtddgi::kFresh )
			flat = FloatVector4( 0.95f, 0.95f, 0.95f, 1.0f );
		const bool usable = age && rtddgi::probeUsable( st, age[p] );
		for ( const Vector3 & n : sph ) {
			c.vp.push_back( Vector3( pos[0] + n[0] * rad, pos[1] + n[1] * rad, pos[2] + n[2] * rad ) );
			if ( mode == 1 ) {
				c.vc.push_back( flat );
				continue;
			}
			if ( !usable ) {   // the read never takes it
				c.vc.push_back( FloatVector4( 0.12f, 0.12f, 0.12f, 1.0f ) );
				continue;
			}
			// this probe alone at the sphere's normal, the read's decode: bilinear e, (e^2.5)^2 x 2 pi; shown E / pi as
			// the probe passes show it, pow(clamp, 1 / 2.2)
			float e[4];
			rtddgi::sampleTile( &irr[size_t( p ) * 256], rtddgi::kIrrTile, 4, rtddgi::V3( n[0], n[1], n[2] ), e );
			float rgb[3];
			for ( int k = 0; k < 3; k++ ) {
				const float q = std::pow( std::max( e[k], 0.0f ), 2.5f );
				rgb[k] = std::pow( std::clamp( q * q * 2.0f, 0.0f, 1.0f ), 1.0f / 2.2f );   // q^2 2 pi / pi
			}
			c.vc.push_back( FloatVector4( rgb[0], rgb[1], rgb[2], 1.0f ) );
		}
	}
	if ( c.vp.empty() )
		return;
	auto fn = scene->renderer->fn;
	scene->loadModelViewMatrix( viewTrans );
	fn->glEnable( GL_DEPTH_TEST );
	fn->glDepthFunc( GL_LEQUAL );
	fn->glDisable( GL_CULL_FACE );
	constexpr size_t kChunk = 65532;   // a multiple of 3
	for ( size_t i = 0; i < c.vp.size(); i += kChunk )
		scene->drawTriangles( c.vp.data() + i, std::min( kChunk, c.vp.size() - i ), c.vc.data() + i, true );
	fn->glDepthFunc( GL_LESS );
}

/* lane RTXFIX1 (bungo 10-29: rounder spheres, "fog", the spheres by the lamps as dark as the rest): the probes view drawn
 * INSIDE the cell's linear frame, after the opaque scene and before the tone map, by its own program
 * (res/shaders/cell_ddgiprobe.*): one unit sphere instanced per probe, 512 triangles within kViewNear of the camera and
 * 128 beyond, the irradiance read from the atlas on the GPU (no host copy under interop), E / pi linear (the exposure and
 * the tone map treat it as a white surface), then the fog. A probe whose sphere would hold the camera (2 radii + 10 u)
 * is not drawn: it clipped open at the near plane. WW_CELL_RTDDGI_VIEW_FOG=0 keeps the old overlay. */
void rtddgiCellViewDrawLinear( Scene * scene, const Transform & viewTrans )
{
	if ( !scene || !scene->renderer || scene->selecting || !rtddgiCellShaderDefine() || viewOverlay() )
		return;
	const int mode = rtddgiCellView();
	Cell & c = cell();
	Renderer * r = scene->renderer;
	Cell & vc = cascCell( viewLevel() );   // RTXCASCADE2: the picked cascade's volume and atlas (0 = M3's volume)
	if ( !mode || !vc.live || vc.nif != scene->nifModel || !vc.uploaded || !vc.tex || vc.glOwner != r )
		return;
	QElapsedTimer t;
	t.start();
	const RtDdgiVolume & v = vc.live->live.volume();
	const float rad = 0.12f * std::min( v.step[0], std::min( v.step[1], v.step[2] ) );
	const int nP = v.probeCount();
	// the instances: near ones first (x = the probe, y = its state code: 0 Vigilant, 1 Asleep, 2 Off, 3 fresh)
	std::vector<float> nearL, farL;
	int culled = 0;
	for ( int p = 0; p < nP && nearL.size() / 4 + farL.size() / 4 < kViewMaxProbes; p++ ) {
		float pos[3];
		v.position( p, pos );
		const double d = double( ( viewTrans * Vector3( pos[0], pos[1], pos[2] ) ).length() );
		if ( d > kViewReach )
			continue;
		if ( d < 2.0 * double( rad ) + 10.0 ) {
			culled++;
			continue;
		}
		const int st = v.state[size_t( p )];
		const float code = st == rtddgi::kAsleep ? 1.0f : st == rtddgi::kOff ? 2.0f : st == rtddgi::kFresh ? 3.0f : 0.0f;
		std::vector<float> & L = d < kViewNear ? nearL : farL;
		L.insert( L.end(), { float( p ), code, 0.0f, 0.0f } );
	}
	const int nNear = int( nearL.size() / 4 ), nFar = int( farL.size() / 4 );
	if ( nNear + nFar == 0 )
		return;
	auto fn = r->fn;
	static const std::vector<Vector3> meshNear = sphereLevel( kViewLevelNear );
	static const std::vector<Vector3> meshFar = sphereLevel( kViewLevelFar );
	if ( c.viewOwner != r || !c.viewVao ) {   // made once in this context (a new context makes its own; the old ones go with it)
		fn->glGenVertexArrays( 1, &c.viewVao );
		fn->glGenBuffers( 1, &c.viewMeshBuf );
		fn->glGenTextures( 1, &c.viewMeshTex );
		fn->glGenBuffers( 1, &c.viewListBuf );
		fn->glGenTextures( 1, &c.viewListTex );
		std::vector<float> m;
		for ( const std::vector<Vector3> * s : { &meshNear, &meshFar } )
			for ( const Vector3 & q : *s )
				m.insert( m.end(), { q[0], q[1], q[2], 0.0f } );
		fn->glBindBuffer( GL_TEXTURE_BUFFER, c.viewMeshBuf );
		fn->glBufferData( GL_TEXTURE_BUFFER, GLsizeiptr( m.size() * sizeof( float ) ), m.data(), GL_STATIC_DRAW );
		fn->glBindBuffer( GL_TEXTURE_BUFFER, 0 );
		c.viewOwner = r;
	}
	c.viewList = nearL;
	c.viewList.insert( c.viewList.end(), farL.begin(), farL.end() );
	fn->glBindBuffer( GL_TEXTURE_BUFFER, c.viewListBuf );
	fn->glBufferData( GL_TEXTURE_BUFFER, GLsizeiptr( c.viewList.size() * sizeof( float ) ), c.viewList.data(), GL_STREAM_DRAW );
	fn->glBindBuffer( GL_TEXTURE_BUFFER, 0 );
	NifSkopeOpenGLContext::Program * prog = r->useProgram( "cell_ddgiprobe.prog" );
	if ( !prog )
		return;
	GLint prevActive = 0;
	fn->glGetIntegerv( GL_ACTIVE_TEXTURE, &prevActive );
	// units: the old GI's half-res pair (cellunits.h GiHalf / GiHalfKey; that pass never runs since PRTOFF1, and every
	// user binds its own), the atlas on its own unit by rtddgiCellUniforms
	fn->glActiveTexture( GLenum( GL_TEXTURE0 + WwCellUnit::GiHalf ) );
	fn->glBindTexture( GL_TEXTURE_BUFFER, c.viewMeshTex );
	fn->glTexBuffer( GL_TEXTURE_BUFFER, GL_RGBA32F, c.viewMeshBuf );
	fn->glActiveTexture( GLenum( GL_TEXTURE0 + WwCellUnit::GiHalfKey ) );
	fn->glBindTexture( GL_TEXTURE_BUFFER, c.viewListTex );
	fn->glTexBuffer( GL_TEXTURE_BUFFER, GL_RGBA32F, c.viewListBuf );
	fn->glActiveTexture( GLenum( prevActive ) );
	prog->uni1i( "probeMesh", WwCellUnit::GiHalf );
	prog->uni1i( "probeList", WwCellUnit::GiHalfKey );
	bindOne( scene, r, prog, prog->uniLocation( "cellDdgiOn" ), vc );   // cellDdgiOn, the atlas, origin, step, dims (RTXCASCADE2: the pick's)
	prog->uni4m( "modelViewMatrix", viewTrans.toMatrix4() );
	prog->uni1f( "probeRad", rad );
	prog->uni1i( "probeMode", mode );
	const int fogMode = viewFogMode();
	prog->uni1b( "probeFog", fogMode == 1 );
	prog->uni1f( "probeExposure", wwSceneExposureScale() );
	wwLookdevFogUniforms( scene );
	GLint prevDepthFunc = GL_LESS;
	fn->glGetIntegerv( GL_DEPTH_FUNC, &prevDepthFunc );
	fn->glEnable( GL_DEPTH_TEST );
	fn->glDepthFunc( GL_LEQUAL );
	fn->glDepthMask( GL_TRUE );
	fn->glDisable( GL_BLEND );
	fn->glDisable( GL_CULL_FACE );
	fn->glBindVertexArray( c.viewVao );
	if ( nNear ) {
		prog->uni1i( "meshBase", 0 );
		prog->uni1i( "listBase", 0 );
		fn->glDrawArraysInstanced( GL_TRIANGLES, 0, GLsizei( meshNear.size() ), nNear );
	}
	if ( nFar ) {
		prog->uni1i( "meshBase", int( meshNear.size() ) );
		prog->uni1i( "listBase", nNear );
		fn->glDrawArraysInstanced( GL_TRIANGLES, 0, GLsizei( meshFar.size() ), nFar );
	}
	fn->glBindVertexArray( 0 );
	fn->glDepthFunc( GLenum( prevDepthFunc ) );
	r->stopProgram();
	c.viewDrewInFrame = true;
	const double ms = double( t.nsecsElapsed() ) / 1e6;
	// the census, when the counts move: the view's size and its CPU cost (bungo: inside the budget at 20000 probes)
	const QString line = QStringLiteral( "probes view in the frame (%1): %2 near spheres x %3 triangles + %4 far x %5, %6 hidden at the camera; "
		"%7 triangles%8" ).arg( fogMode == 1 ? QStringLiteral( "fogged" ) : QStringLiteral( "no fog" ) ).arg( nNear )
		.arg( meshNear.size() / 3 ).arg( nFar ).arg( meshFar.size() / 3 ).arg( culled )
		.arg( qint64( nNear ) * qint64( meshNear.size() / 3 ) + qint64( nFar ) * qint64( meshFar.size() / 3 ) )
		.arg( vc.level > 0 ? QStringLiteral( "; cascade %1" ).arg( vc.level ) : QString() );   // RTXCASCADE2
	if ( line != c.viewLast ) {
		c.viewLast = line;
		say( line + QStringLiteral( "; CPU %1 ms" ).arg( ms, 0, 'f', 3 ) );
	}
}
