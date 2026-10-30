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
};

Cell & cell()
{
	static Cell * c = new Cell;   // never destroyed: a live volume must not close after the bake device's statics
	return *c;
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
	say( QStringLiteral( "refused(%1)" ).arg( why ) );
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

void startBuild( Cell & c, const ProbeSoup & soup, const WwCellLighting & lights, const ProbeSkyLight & sky, const WwCellGi * G,
	const float eye[3], const CellSpec & sp, int paths )
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
	std::thread( [b, soupCopy, lights, sky, spec, share]() {
		QElapsedTimer tm;
		tm.start();
		const bool ok = b->live.open( *soupCopy, lights, sky, spec, share );
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
	say( QStringLiteral( "built %1 in %2 ms (soup copy %3 ms); grid %4 x %5 x %6, spacing %7 x %8 x %9%10; box %11; first probe %12 %13 "
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
			: QStringLiteral( "scrolls with the camera by whole cells" ) ) );
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
	say( QStringLiteral( "set %1, updates %2 (+%3); gpu ms trace %4 blend %5 border %6 scroll %7 (the last finished set's); %8; wall %9 ms; "
		"rays %10; %11; atlas bytes %12; busy frames %13%14" )
		.arg( c.sets ).arg( L.updates() ).arg( k ).arg( msText( ms[0] ), msText( ms[1] ), msText( ms[2] ), msText( ms[4] ) ).arg( rb )
		.arg( L.lastWallMs(), 0, 'f', 3 ).arg( L.lastRays() ).arg( statesOf( L.volume() ) ).arg( atlasBytes( L.volume().probeCount() ) )
		.arg( c.busy ).arg( tail ) );
}

void dumpLive( Cell & c, RtDdgiLive & L )
{
	const QString dir = QString::fromLocal8Bit( wwEnv( "WW_CELL_RTDDGI_DUMP" ) ).trimmed();
	if ( dir.isEmpty() || c.dumped )
		return;
	c.dumped = true;
	L.finish();   // the step that copied the tiles back, done
	RtDdgiVolume v;
	L.snapshot( &v );
	QString err;
	if ( rtddgiDump( v, L.spec(), dir, &err ) )
		say( QStringLiteral( "dumped %1 updates to %2" ).arg( L.updates() ).arg( QDir::toNativeSeparators( dir ) ) );
	else
		say( QStringLiteral( "dump refused(%1)" ).arg( err ) );
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
	return c.guiOn;
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

void rtddgiCellFrame( Scene * scene )
{
	if ( !scene || !scene->nifModel || !scene->renderer )
		return;
	Cell & c = cell();
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
	const WwCellGi * G = wwCellGiFor( nif );
	const CellSpec sp = cellSpecNow();
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
	if ( key == c.key && c.nif == nif && c.live )
		moved = scrollFor( *c.live, c.live->live.volume(), eye, cells );
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
		startBuild( c, *soup, lights, sky, G, eye, sp, paths );
		say( QStringLiteral( "%1 (%2 triangles)" ).arg( had ? QStringLiteral( "rebuilding: " ) + rebuildWhy : QStringLiteral( "building" ) )
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
	if ( !c.live )
		return;
	RtDdgiLive & L = c.live->live;
	const int view = rtddgiCellView();
	const bool wantDump = !c.dumped && !wwEnvStr( "WW_CELL_RTDDGI_DUMP" ).trimmed().isEmpty();
	GlShare & g = glShare();
	auto fn = r->fn;
	/* one step: with interop the submit is ordered against GL both ways (GL signals glDone before it, so the device
	 * writes only after GL's reads so far; GL waits on vkDone after it, before any read of this frame) */
	auto runStep = [&c, &L, &g, fn]( RtDdgiStep & o ) {
		const bool gl = c.interop && g.ok && !( cellRedPaths() & kPathNoWait );   // lane RTXFIX1: the red drops the order
		if ( gl ) {
			Cell * cp = &c;
			o.beforeSubmit = [cp, &g, fn]() {
				if ( !cp->glRead )
					return false;
				g.signalSem( cp->semGl, 1, &cp->buf, 0, nullptr, nullptr );
				cp->glRead = false;
				cp->syncSignals++;
				fn->glFlush();   // the signal reaches the GPU before the device's submit waits on it
				return true;
			};
		}
		const bool ok = L.step( o );
		if ( ok && gl && L.submitted() ) {   // every such submit signalled vkDone: GL waits on it exactly once
			QElapsedTimer t;
			t.start();
			g.waitSem( c.semVk, 1, &c.buf, 0, nullptr, nullptr );
			c.glWaitMs = double( t.nsecsElapsed() ) / 1e6;
			c.glRead = true;
			c.syncWaits++;
			c.uploaded = true;
		} else if ( ok && c.interop && !gl && L.submitted() ) {
			c.uploaded = true;   // the nowait red: GL reads the device's atlas with no order at all
		}
		return ok;
	};
	if ( moved )
		c.stopped = false;   // a scroll re-arms the spec's max: the new planes converge too
	if ( c.stopped ) {
		/* the irradiance view and a dump need the tiles on the host: one copy step (k = 0). A dump asked for after the
		 * stop is written now (M2 wrote it only at the stop: a row that set the key on a volume an earlier row had
		 * already stopped, with nothing to rebuild it, never dumped: M2's mem_rt_dump) */
		if ( ( view == 2 && viewOverlay() && !L.irr() ) || ( wantDump && !L.irr() ) ) {   // lane RTXFIX1: the in-frame view reads the atlas
			RtDdgiStep o;
			o.k = 0;
			runStep( o );
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
	RtDdgiStep o;
	o.k = k;
	for ( int a = 0; a < 3; a++ )
		o.scroll[a] = moved ? cells[a] : 0;
	// interop: no host copy unless the debug view or a dump at this step needs one; the step then waits for it
	// lane RTXFIX1: the in-frame irradiance view reads the atlas on the GPU; only the overlay (WW_CELL_RTDDGI_VIEW_FOG=0) needs the tiles here
	o.host = !c.interop || ( view == 2 && viewOverlay() ) || ( wantDump && ( atMax || ( max <= 0 && c.sets + 1 == 64 ) ) );
	o.wait = !c.interop || o.host;
	if ( !runStep( o ) ) {
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
		if ( moved )
			c.probesPacked = false;   // the states moved with the scroll (and came back with it: the step waited)
		if ( !upload( c, r, L ) ) {
			refuse( c, QStringLiteral( "no tiles came back" ) );
			return;
		}
	}
	if ( moved && ( L.scrolls() <= kScrollEcho || L.scrolls() % 64 == 0 ) ) {
		const RtDdgiVolume & v = L.volume();
		say( QStringLiteral( "scroll %1: scrolled %2 cells (%3 %4 %5), reset %6 probes (%7 in all); first probe %8 %9 %10; camera %11 %12 %13" )
			.arg( L.scrolls() ).arg( L.lastScrollCells() ).arg( cells[0] ).arg( cells[1] ).arg( cells[2] ).arg( L.lastScrollFresh() )
			.arg( L.freshTotal() ).arg( double( v.origin[0] ), 0, 'f', 0 ).arg( double( v.origin[1] ), 0, 'f', 0 )
			.arg( double( v.origin[2] ), 0, 'f', 0 ).arg( double( eye[0] ), 0, 'f', 0 ).arg( double( eye[1] ), 0, 'f', 0 )
			.arg( double( eye[2] ), 0, 'f', 0 ) );
	}
	const bool pow2 = ( c.sets & ( c.sets - 1 ) ) == 0;
	if ( pow2 || atMax || c.sets % 256 == 0 )
		censusSet( c, L, k, atMax ? QStringLiteral( "; stopped at the spec's max (%1 updates since the build or the last scroll); %2" )
			.arg( max ).arg( cameraWords( L.volume(), eye ) ) : QString() );
	if ( atMax ) {
		c.stopped = true;
		dumpLive( c, L );
	} else if ( max <= 0 && c.sets == 64 ) {
		dumpLive( c, L );
	}
}

bool rtddgiCellPending()
{
	if ( !rtddgiCellShaderDefine() )
		return false;
	const Cell & c = cell();
	if ( c.pending )
		return true;   // the worker's build (a failed one is harvested by the next frame, then nothing is pending)
	return c.live && c.live->spec.max > 0 && !c.stopped;
}

void rtddgiCellUniforms( Scene * scene )
{
	Renderer * r = scene ? scene->renderer : nullptr;
	NifSkopeOpenGLContext::Program * prog = r ? r->getCurrentProgram() : nullptr;
	if ( !prog )
		return;
	const int locOn = prog->uniLocation( "cellDdgiOn" );
	if ( locOn < 0 )
		return;   // a program without the read: with the switch off, every program
	Cell & c = cell();
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

void rtddgiCellViewDraw( Scene * scene, const Transform & viewTrans )
{
	if ( !scene || !scene->renderer || scene->selecting || !rtddgiCellShaderDefine() )
		return;
	const int mode = rtddgiCellView();
	Cell & c = cell();
	const bool inFrame = c.viewDrewInFrame;   // lane RTXFIX1: drawn inside the linear frame this paint: no overlay
	c.viewDrewInFrame = false;
	if ( !mode || !c.live || c.nif != scene->nifModel || inFrame )
		return;
	const RtDdgiLive & L = c.live->live;
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
	if ( !mode || !c.live || c.nif != scene->nifModel || !c.uploaded || !c.tex || c.glOwner != r )
		return;
	QElapsedTimer t;
	t.start();
	const RtDdgiVolume & v = c.live->live.volume();
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
	rtddgiCellUniforms( scene );   // cellDdgiOn, the atlas, origin, step, dims
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
		"%7 triangles" ).arg( fogMode == 1 ? QStringLiteral( "fogged" ) : QStringLiteral( "no fog" ) ).arg( nNear )
		.arg( meshNear.size() / 3 ).arg( nFar ).arg( meshFar.size() / 3 ).arg( culled )
		.arg( qint64( nNear ) * qint64( meshNear.size() / 3 ) + qint64( nFar ) * qint64( meshFar.size() / 3 ) );
	if ( line != c.viewLast ) {
		c.viewLast = line;
		say( line + QStringLiteral( "; CPU %1 ms" ).arg( ms, 0, 'f', 3 ) );
	}
}
