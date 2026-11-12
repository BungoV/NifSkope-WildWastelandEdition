/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#include "lodgengpu.h"

#include "lodgenbc7.h"
#include "gl/wwshadertext.h"	// lane VKCOMPUTE1: the BC7 shader is a file
#include "harnesswindow.h"   // wwHarnessSettingsSuffix
#include "version.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QGuiApplication>
#include <QMutex>
#include <QMutexLocker>
#include <QOffscreenSurface>
#include <QOpenGLContext>
#include <QOpenGLFunctions_4_3_Core>
#include <QOpenGLVersionFunctionsFactory>
#include <QSettings>
#include <QStringList>
#include <QThread>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

#include <atomic>
#include <cmath>
#include <cstring>
#include <vector>

// the vendored detex BC7 decoder (lib/libfo76utils/src/decompress-bptc.c): the self-check's independent measure
extern "C" bool detexDecompressBlockBPTC( const uint8_t * bitstring, uint32_t mode_mask, uint32_t flags, uint8_t * pixel_buffer );

const char * const kLodgenGpuSettingKey = "Settings/Nif/Use GPU";

// THE BC7 ENCODER IN GLSL: res/shaders/features/far-lod/lodgen_bc7.glsl (lane VKCOMPUTE1: moved out byte for byte).

namespace
{

//! The shader's weight formula (wOf) against the vendored detex weight tables; true when every entry matches.
bool weightFormulaMatches()
{
	const uint16_t * tabs[3] = { detex_bptc_table_aWeight2, detex_bptc_table_aWeight3, detex_bptc_table_aWeight4 };
	for ( int ib = 2; ib <= 4; ib++ )
		for ( int k = 0; k < ( 1 << ib ); k++ )
			if ( int( float( 64 * k ) / float( ( 1 << ib ) - 1 ) + 0.5f ) != int( tabs[ib - 2][k] ) )
				return false;
	return true;
}

//! The shader source with the vendored detex partition tables filled in; empty (*miss set) without the file.
QByteArray bc7Source( QString * miss )
{
	const QByteArray kBc7Glsl = wwShaderText( "lodgen_bc7.glsl", miss );
	if ( kBc7Glsl.isEmpty() )
		return kBc7Glsl;
	QString t;
	t += "const uint P2SUB[64] = uint[64](";
	for ( int pt = 0; pt < 64; pt++ ) {
		quint32 bits = 0;
		for ( int i = 0; i < 16; i++ )
			if ( detex_bptc_table_P2[pt * 16 + i] )
				bits |= 1u << i;
		t += QString( "%1%2u" ).arg( pt ? "," : "" ).arg( bits );
	}
	t += ");\nconst int ANCHOR2[64] = int[64](";
	for ( int pt = 0; pt < 64; pt++ )
		t += QString( "%1%2" ).arg( pt ? "," : "" ).arg( int( detex_bptc_table_anchor_index_second_subset[pt] ) );
	t += ");\n";
	QString s = QString::fromLatin1( kBc7Glsl );
	s.replace( "%TABLES%", t );
	return s.toLatin1();
}

/* Below this many blocks an image goes to the CPU loop: a dispatch round trip
 * costs more than the CPU spends on it. */
const int kMinGpuBlocks = 256;
//! The first batch; later batches are sized to ~kTargetMs each (Windows resets a GPU job that runs 2 s).
const int kFirstBatch = 4096;
const double kTargetMs = 150.0;

struct GpuState
{
	bool tried = false;          // the context and its self-check are made once per process
	bool ready = false;          // ... and passed
	bool dead = false;           // a job failed after that: the context is not trusted again
	QString why;                 // why not ready / why dead
	QString ready1;              // the report line of a ready context
	std::atomic<bool> on{ false };
	QString report = QStringLiteral( "gpu: CPU path -- not configured (only `lodgen` runs set the path)" );
	QMutex jobLock;
	QThread * thread = nullptr;
	QObject * worker = nullptr;
	QOffscreenSurface * surface = nullptr;
	QOpenGLContext * ctx = nullptr;
	QOpenGLFunctions_4_3_Core * gl = nullptr;
	GLuint prog = 0, bufIn = 0, bufOut = 0, bufErr = 0, bufList = 0;
	GLint locW = -1, locH = -1, locBw = -1, locFirst = -1, locCount = -1, locWt = -1, locPass = -1;
	qint64 capIn = 0, capOut = 0, capErr = 0, capList = 0;
	double blocksPerMs[3] = { 0.0, 0.0, 0.0 };   // per pass: batch sizing only
	std::atomic<quint64> images{ 0 }, blocks{ 0 }, fellBack{ 0 }, gpuMs{ 0 };
	QString lastError;
};

GpuState & G()
{
	static GpuState g;
	return g;
}

template <class F> bool onGpuThread( F && f )
{
	GpuState & g = G();
	bool ok = false;
	QMetaObject::invokeMethod( g.worker, [&]() { ok = f(); }, Qt::BlockingQueuedConnection );
	return ok;
}

//! GPU thread: context current, program built, buffers made. Fills `why` on failure.
bool gpuInit( QString * why, QString * renderer )
{
	GpuState & g = G();
	if ( !g.ctx->makeCurrent( g.surface ) ) {
		*why = QStringLiteral( "makeCurrent on the offscreen surface failed" );
		return false;
	}
	g.gl = QOpenGLVersionFunctionsFactory::get<QOpenGLFunctions_4_3_Core>( g.ctx );
	if ( !g.gl || !g.gl->initializeOpenGLFunctions() ) {
		*why = QStringLiteral( "the driver gives no OpenGL 4.3 core functions" );
		return false;
	}
	auto * gl = g.gl;
	*renderer = QString::fromLatin1( reinterpret_cast<const char *>( gl->glGetString( GL_RENDERER ) ) );
	if ( !weightFormulaMatches() ) {
		*why = QStringLiteral( "the shader's BC7 weight formula does not match the detex tables" );
		return false;
	}
	const QByteArray src = bc7Source( why );
	if ( src.isEmpty() )
		return false;
	const char * p = src.constData();
	GLuint sh = gl->glCreateShader( GL_COMPUTE_SHADER );
	gl->glShaderSource( sh, 1, &p, nullptr );
	gl->glCompileShader( sh );
	GLint ok = 0;
	gl->glGetShaderiv( sh, GL_COMPILE_STATUS, &ok );
	if ( !ok ) {
		char log[2048] = {};
		gl->glGetShaderInfoLog( sh, sizeof( log ) - 1, nullptr, log );
		*why = QStringLiteral( "the BC7 shader did not compile: " ) + QString::fromLatin1( log ).left( 400 ).simplified();
		gl->glDeleteShader( sh );
		return false;
	}
	g.prog = gl->glCreateProgram();
	gl->glAttachShader( g.prog, sh );
	gl->glLinkProgram( g.prog );
	gl->glDeleteShader( sh );
	gl->glGetProgramiv( g.prog, GL_LINK_STATUS, &ok );
	if ( !ok ) {
		char log[2048] = {};
		gl->glGetProgramInfoLog( g.prog, sizeof( log ) - 1, nullptr, log );
		*why = QStringLiteral( "the BC7 shader did not link: " ) + QString::fromLatin1( log ).left( 400 ).simplified();
		return false;
	}
	g.locW = gl->glGetUniformLocation( g.prog, "uW" );
	g.locH = gl->glGetUniformLocation( g.prog, "uH" );
	g.locBw = gl->glGetUniformLocation( g.prog, "uBw" );
	g.locFirst = gl->glGetUniformLocation( g.prog, "uFirst" );
	g.locCount = gl->glGetUniformLocation( g.prog, "uCount" );
	g.locWt = gl->glGetUniformLocation( g.prog, "uWt" );
	gl->glGenBuffers( 1, &g.bufIn );
	gl->glGenBuffers( 1, &g.bufOut );
	gl->glGenBuffers( 1, &g.bufErr );
	gl->glGenBuffers( 1, &g.bufList );
	// the list buffer must exist before pass 0 binds it
	gl->glBindBuffer( GL_SHADER_STORAGE_BUFFER, g.bufList );
	gl->glBufferData( GL_SHADER_STORAGE_BUFFER, 4096, nullptr, GL_DYNAMIC_DRAW );
	g.capList = 4096;
	g.locPass = gl->glGetUniformLocation( g.prog, "uPass" );
	return gl->glGetError() == GL_NO_ERROR;
}

//! GPU thread: release everything the context owns.
void gpuRelease()
{
	GpuState & g = G();
	if ( g.ctx && g.gl && g.ctx->makeCurrent( g.surface ) ) {
		if ( g.bufIn ) g.gl->glDeleteBuffers( 1, &g.bufIn );
		if ( g.bufOut ) g.gl->glDeleteBuffers( 1, &g.bufOut );
		if ( g.bufErr ) g.gl->glDeleteBuffers( 1, &g.bufErr );
		if ( g.bufList ) g.gl->glDeleteBuffers( 1, &g.bufList );
		if ( g.prog ) g.gl->glDeleteProgram( g.prog );
		g.ctx->doneCurrent();
	}
	g.bufIn = g.bufOut = g.bufErr = g.bufList = g.prog = 0;
	g.gl = nullptr;
	delete g.ctx;
	g.ctx = nullptr;
}

/*! GPU thread: one image. False on any GL error (the caller then runs the CPU loop).
 *  `errs`, when given, receives each block's reported weighted error. */
bool gpuBc7( const quint32 * px, int w, int h, const int wt[4], quint8 * out, std::vector<qint32> * errs = nullptr )
{
	GpuState & g = G();
	auto * gl = g.gl;
	if ( !gl || !g.ctx->isValid() || !g.ctx->makeCurrent( g.surface ) ) {
		g.lastError = QStringLiteral( "the context is gone" );
		return false;
	}
	while ( gl->glGetError() != GL_NO_ERROR ) {}
	const int bw = ( w + 3 ) / 4, bh = ( h + 3 ) / 4;
	const int nb = bw * bh;
	const qint64 inBytes = qint64( w ) * h * 4, outBytes = qint64( nb ) * 16;
	gl->glBindBuffer( GL_SHADER_STORAGE_BUFFER, g.bufIn );
	if ( inBytes > g.capIn ) {
		gl->glBufferData( GL_SHADER_STORAGE_BUFFER, inBytes, nullptr, GL_DYNAMIC_DRAW );
		g.capIn = inBytes;
	}
	gl->glBufferSubData( GL_SHADER_STORAGE_BUFFER, 0, inBytes, px );
	gl->glBindBuffer( GL_SHADER_STORAGE_BUFFER, g.bufOut );
	if ( outBytes > g.capOut ) {
		gl->glBufferData( GL_SHADER_STORAGE_BUFFER, outBytes, nullptr, GL_DYNAMIC_READ );
		g.capOut = outBytes;
	}
	const qint64 errBytes = qint64( nb ) * 4;
	gl->glBindBuffer( GL_SHADER_STORAGE_BUFFER, g.bufErr );
	if ( errBytes > g.capErr ) {
		gl->glBufferData( GL_SHADER_STORAGE_BUFFER, errBytes, nullptr, GL_DYNAMIC_READ );
		g.capErr = errBytes;
	}
	gl->glBindBufferBase( GL_SHADER_STORAGE_BUFFER, 0, g.bufIn );
	gl->glBindBufferBase( GL_SHADER_STORAGE_BUFFER, 1, g.bufOut );
	gl->glBindBufferBase( GL_SHADER_STORAGE_BUFFER, 2, g.bufErr );
	gl->glUseProgram( g.prog );
	gl->glUniform1i( g.locW, w );
	gl->glUniform1i( g.locH, h );
	gl->glUniform1i( g.locBw, bw );
	gl->glUniform4i( g.locWt, wt[0], wt[1], wt[2], wt[3] );
	gl->glBindBufferBase( GL_SHADER_STORAGE_BUFFER, 3, g.bufList );
	QElapsedTimer clock;
	clock.start();
	// Three passes (see the shader's uPass).
	std::vector<qint32> err( static_cast<size_t>( nb ) );
	std::vector<qint32> list;
	for ( int pass = 0; pass < 3; pass++ ) {
		int todo = nb;
		if ( pass > 0 ) {
			gl->glMemoryBarrier( GL_BUFFER_UPDATE_BARRIER_BIT );
			gl->glBindBuffer( GL_SHADER_STORAGE_BUFFER, g.bufErr );
			gl->glGetBufferSubData( GL_SHADER_STORAGE_BUFFER, 0, errBytes, err.data() );
			list.clear();
			for ( int b = 0; b < nb; b++ )
				if ( err[size_t( b )] > 0 )
					list.push_back( b );
			todo = int( list.size() );
			if ( !todo )
				break;
			const qint64 listBytes = qint64( todo ) * 4;
			gl->glBindBuffer( GL_SHADER_STORAGE_BUFFER, g.bufList );
			if ( listBytes > g.capList ) {
				gl->glBufferData( GL_SHADER_STORAGE_BUFFER, listBytes, nullptr, GL_DYNAMIC_DRAW );
				g.capList = listBytes;
			}
			gl->glBufferSubData( GL_SHADER_STORAGE_BUFFER, 0, listBytes, list.data() );
			gl->glMemoryBarrier( GL_SHADER_STORAGE_BARRIER_BIT );
		}
		gl->glUniform1i( g.locPass, pass );
		double & perMs = g.blocksPerMs[pass];
		for ( int first = 0; first < todo; ) {
			int count = perMs > 0.0 ? int( perMs * kTargetMs ) : kFirstBatch;
			count = qBound( 256, ( count + 63 ) / 64 * 64, 1 << 20 );
			count = qMin( count, todo - first );
			QElapsedTimer one;
			one.start();
			gl->glUniform1i( g.locFirst, first );
			gl->glUniform1i( g.locCount, count );
			gl->glDispatchCompute( GLuint( ( count + 63 ) / 64 ), 1, 1 );
			gl->glFinish();
			const double ms = qMax( 1.0, double( one.nsecsElapsed() ) / 1e6 );
			const double rate = double( count ) / ms;
			perMs = perMs > 0.0 ? qMin( perMs, rate ) * 0.5 + rate * 0.5 : rate;
			if ( gl->glGetError() != GL_NO_ERROR ) {
				g.lastError = QStringLiteral( "GL error in the BC7 dispatch" );
				return false;
			}
			first += count;
		}
	}
	gl->glMemoryBarrier( GL_BUFFER_UPDATE_BARRIER_BIT );
	gl->glBindBuffer( GL_SHADER_STORAGE_BUFFER, g.bufOut );
	gl->glGetBufferSubData( GL_SHADER_STORAGE_BUFFER, 0, outBytes, out );
	if ( errs ) {
		errs->assign( size_t( nb ), 0 );
		gl->glBindBuffer( GL_SHADER_STORAGE_BUFFER, g.bufErr );
		gl->glGetBufferSubData( GL_SHADER_STORAGE_BUFFER, 0, errBytes, errs->data() );
	}
	const bool ok = gl->glGetError() == GL_NO_ERROR && g.ctx->isValid();
	g.gpuMs += quint64( clock.elapsed() );
	if ( !ok )
		g.lastError = QStringLiteral( "GL error reading the BC7 blocks back" );
	return ok;
}

//! A fixed image for the start-up self-check: flat, two-tone, gradients, noise, cut-out alpha.
std::vector<quint32> selfCheckImage( int w, int h )
{
	std::vector<quint32> px( size_t( w ) * h );
	quint32 s = 0x9E3779B9u;
	auto rnd = [&s]() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; };
	for ( int y = 0; y < h; y++ )
		for ( int x = 0; x < w; x++ ) {
			const int bx = x / 4, by = y / 4, kind = ( bx + by * 7 ) % 8;
			quint32 r = 0, gg = 0, b = 0, a = 255;
			switch ( kind ) {
			case 0: r = 128; gg = 128; b = 255; break;
			case 1: r = ( x & 1 ) ? 20 : 230; gg = 128; b = ( y & 1 ) ? 40 : 200; break;
			case 2: r = x * 255 / qMax( 1, w - 1 ); gg = y * 255 / qMax( 1, h - 1 ); b = ( x + y ) & 255; break;
			case 3: r = rnd() & 255; gg = rnd() & 255; b = rnd() & 255; a = rnd() & 255; break;
			case 4: r = rnd() & 255; gg = rnd() & 255; b = rnd() & 255; break;
			case 5: r = 128 + ( rnd() & 7 ); gg = 128 - ( rnd() & 7 ); b = ( bx * 37 ) & 255; a = ( ( x ^ y ) & 2 ) ? 0 : 255; break;
			case 6: r = 250; gg = 3; b = 128 + ( y & 3 ) * 30; a = 200 + ( x & 3 ); break;
			default: r = gg = b = ( rnd() & 1 ) ? 255 : 0; break;
			}
			px[size_t( y ) * w + x] = ( a << 24 ) | ( r << 16 ) | ( gg << 8 ) | b;
		}
	return px;
}

//! Block (bx, by) of the image as the encoder reads it: R G B A, the edge pixels repeated.
void blockPixels( const quint32 * px, int w, int h, int bx, int by, uint8_t rgba[16][4] )
{
	for ( int i = 0; i < 16; i++ ) {
		const int sx = qMin( bx * 4 + ( i & 3 ), w - 1 ), sy = qMin( by * 4 + ( i >> 2 ), h - 1 );
		const quint32 p = px[size_t( sy ) * w + sx];
		rgba[i][0] = uint8_t( p >> 16 );
		rgba[i][1] = uint8_t( p >> 8 );
		rgba[i][2] = uint8_t( p );
		rgba[i][3] = uint8_t( p >> 24 );
	}
}

//! The CPU encoder over the image; returns its total weighted error.
qint64 cpuBc7( const std::vector<quint32> & px, int w, int h, const int wt[4], std::vector<quint8> & out )
{
	const int bw = ( w + 3 ) / 4, bh = ( h + 3 ) / 4;
	out.assign( size_t( bw ) * bh * 16, 0 );
	qint64 tot = 0;
	for ( int by = 0; by < bh; by++ )
		for ( int bx = 0; bx < bw; bx++ ) {
			uint8_t rgba[16][4];
			blockPixels( px.data(), w, h, bx, by, rgba );
			tot += LodgenBc7::encodeBlock( rgba, wt, out.data() + ( size_t( by ) * bw + bx ) * 16 );
		}
	return tot;
}

/*! The weighted squared error of one written block against its source pixels,
 *  measured by DECODING the block with the vendored detex decoder (not the
 *  encoder's own arithmetic). -1 when the block does not decode. */
qint64 decodedError( const quint8 blk[16], const uint8_t rgba[16][4], const int wt[4] )
{
	uint8_t dec[64];
	if ( !detexDecompressBlockBPTC( blk, 0xFFFFFFFFu, 0, dec ) )
		return -1;
	qint64 e = 0;
	for ( int i = 0; i < 16; i++ )
		for ( int c = 0; c < 4; c++ ) {
			const int d = int( dec[i * 4 + c] ) - int( rgba[i][c] );
			e += qint64( wt[c] ) * d * d;
		}
	return e;
}

void shutdownGpu()
{
	GpuState & g = G();
	g.on = false;
	if ( g.thread ) {
		QMetaObject::invokeMethod( g.worker, []() { gpuRelease(); }, Qt::BlockingQueuedConnection );
		g.thread->quit();
		g.thread->wait();
		delete g.worker;
		g.worker = nullptr;
		delete g.thread;
		g.thread = nullptr;
	}
	delete g.ctx;          // a context that never reached the thread
	g.ctx = nullptr;
	delete g.surface;
	g.surface = nullptr;
}

} // namespace

static QSettings * guiSettings()
{
	return new QSettings( QStringLiteral( "NifTools" ),
		QStringLiteral( "NifSkope " ) + NifSkopeVersion::rawToMajMin( NIFSKOPE_VERSION ) + wwHarnessSettingsSuffix() );
}

bool lodgenGpuSettingEnabled()
{
	QScopedPointer<QSettings> s( guiSettings() );
	return s->value( QLatin1String( kLodgenGpuSettingKey ), true ).toBool();
}

bool lodgenGpuWantedForArgs( int argc, char ** argv )
{
	bool lodgen = false;
	for ( int i = 1; i < argc; i++ ) {
		if ( !qstrcmp( argv[i], "--no-gpu" ) )
			return false;
		if ( !qstrcmp( argv[i], "lodgen" ) )
			lodgen = true;
	}
	if ( !lodgen || !lodgenGpuSettingEnabled() || argc < 1 )
		return false;
	/* Without the platform plugin a QGuiApplication aborts the process, so the
	 * plugin must be there before one is made: beside the exe, as every
	 * NifSkope folder ships it. */
	QString dir;
#ifdef Q_OS_WIN
	wchar_t buf[4096];
	const DWORD n = GetModuleFileNameW( nullptr, buf, 4096 );
	if ( n > 0 && n < 4096 )
		dir = QFileInfo( QString::fromWCharArray( buf, int( n ) ) ).absolutePath();
#endif
	if ( dir.isEmpty() )
		dir = QFileInfo( QString::fromLocal8Bit( argv[0] ) ).absolutePath();
	return QFileInfo::exists( dir + QStringLiteral( "/platforms/qwindows.dll" ) );
}

//! Main thread, once per process: the context, its thread and the self-check. Sets ready / why.
static void makeContext()
{
	GpuState & g = G();
	g.tried = true;
	auto fail = [&g]( const QString & why ) {
		g.why = QStringLiteral( "no GPU context: " ) + why;
		shutdownGpu();
	};
	if ( !qobject_cast<QGuiApplication *>( QCoreApplication::instance() ) ) {
		fail( QStringLiteral( "this process has no GUI platform (platforms/qwindows.dll missing beside the exe)" ) );
		return;
	}
	QSurfaceFormat fmt;
	fmt.setVersion( 4, 3 );
	fmt.setProfile( QSurfaceFormat::CoreProfile );
	g.surface = new QOffscreenSurface();
	g.surface->setFormat( fmt );
	g.surface->create();
	if ( !g.surface->isValid() ) {
		fail( QStringLiteral( "no offscreen surface" ) );
		return;
	}
	g.ctx = new QOpenGLContext();
	g.ctx->setFormat( fmt );
	if ( !g.ctx->create() ) {
		delete g.ctx;
		g.ctx = nullptr;
		fail( QStringLiteral( "the driver made no OpenGL context" ) );
		return;
	}
	const QSurfaceFormat got = g.ctx->format();
	if ( got.majorVersion() * 10 + got.minorVersion() < 43 ) {
		const QString v = QString( "OpenGL %1.%2 < 4.3" ).arg( got.majorVersion() ).arg( got.minorVersion() );
		delete g.ctx;
		g.ctx = nullptr;
		fail( v );
		return;
	}
	g.thread = new QThread();
	g.thread->setObjectName( QStringLiteral( "lodgen-gpu" ) );
	g.worker = new QObject();
	g.worker->moveToThread( g.thread );
	g.ctx->moveToThread( g.thread );
	g.thread->start();
	qAddPostRoutine( shutdownGpu );
	QString why, renderer;
	if ( !onGpuThread( [&]() { return gpuInit( &why, &renderer ); } ) ) {
		fail( why.isEmpty() ? QStringLiteral( "OpenGL setup failed" ) : why );
		return;
	}
	// THE SELF-CHECK, on a fixed image under an even and an uneven weighting (the card sheet's).
	const int sw = 128, sh = 64, bw = sw / 4, bh = sh / 4;
	const std::vector<quint32> img = selfCheckImage( sw, sh );
	const int wts[2][4] = { { 1, 1, 1, 1 }, { 1, 1, 32, 1 } };
	int blocks = 0, bad = 0;
	qint64 cpuTot = 0, gpuTot = 0;
	QString over;
	for ( const auto & wt : wts ) {
		std::vector<quint8> cpu, gpu( size_t( bw ) * bh * 16 );
		std::vector<qint32> errs;
		const qint64 c = cpuBc7( img, sw, sh, wt, cpu );
		if ( !onGpuThread( [&]() { return gpuBc7( img.data(), sw, sh, wt, gpu.data(), &errs ); } ) ) {
			fail( QStringLiteral( "the self-check dispatch failed: " ) + g.lastError );
			return;
		}
		qint64 t = 0;
		for ( int by = 0; by < bh; by++ )
			for ( int bx = 0; bx < bw; bx++ ) {
				const int b = by * bw + bx;
				uint8_t rgba[16][4];
				blockPixels( img.data(), sw, sh, bx, by, rgba );
				const qint64 e = decodedError( gpu.data() + size_t( b ) * 16, rgba, wt );
				blocks++;
				if ( e < 0 || e != errs[size_t( b )] )
					bad++;
				t += e < 0 ? 0 : e;
			}
		cpuTot += c;
		gpuTot += t;
		if ( t > c && over.isEmpty() )
			over = QString( "weights %1,%2,%3,%4: GPU error %5 > CPU %6" ).arg( wt[0] ).arg( wt[1] ).arg( wt[2] ).arg( wt[3] ).arg( t ).arg( c );
	}
	g.gpuMs = 0;
	g.blocksPerMs[0] = g.blocksPerMs[1] = g.blocksPerMs[2] = 0.0;
	if ( bad ) {
		fail( QString( "the BC7 self-check: %1 of %2 GPU blocks do not decode to their reported error" ).arg( bad ).arg( blocks ) );
		return;
	}
	if ( !over.isEmpty() ) {
		fail( QStringLiteral( "the BC7 self-check: " ) + over );
		return;
	}
	g.ready = true;
	g.ready1 = QString( "%1, OpenGL %2.%3; BC7 self-check %4 blocks, error GPU %5 <= CPU %6" )
		.arg( renderer ).arg( got.majorVersion() ).arg( got.minorVersion() ).arg( blocks ).arg( gpuTot ).arg( cpuTot );
}

void lodgenGpuConfigure( bool forceCpu )
{
	GpuState & g = G();
	if ( forceCpu ) {
		g.on = false;
		g.report = QStringLiteral( "gpu: CPU path -- switch --no-gpu" );
		return;
	}
	if ( !lodgenGpuSettingEnabled() ) {
		g.on = false;
		g.report = QStringLiteral( "gpu: CPU path -- setting Use GPU off (Settings > NIF > LOD bake)" );
		return;
	}
	if ( !g.tried )
		makeContext();
	if ( g.dead ) {
		g.on = false;
		g.report = QStringLiteral( "gpu: CPU path -- the GPU failed earlier in this session: " ) + g.lastError;
	} else if ( !g.ready ) {
		g.on = false;
		g.report = QStringLiteral( "gpu: CPU path -- " ) + g.why;
	} else {
		g.on = true;
		g.report = QStringLiteral( "gpu: GPU path -- setting Use GPU on; " ) + g.ready1;
	}
}

bool lodgenGpuOn()
{
	return G().on;
}

QString lodgenGpuReport()
{
	return G().report;
}

QString lodgenGpuDigestWord()
{
	return G().on ? QStringLiteral( "|bc7gpu" ) : QString();
}

QString lodgenGpuSummary()
{
	GpuState & g = G();
	return QString( "gpu: bc7 %1 image(s), %2 block(s) on the GPU in %3 ms; %4 image(s) fell back to the CPU%5" )
		.arg( g.images.load() ).arg( g.blocks.load() ).arg( g.gpuMs.load() ).arg( g.fellBack.load() )
		.arg( g.lastError.isEmpty() ? QString() : QStringLiteral( " (last: " ) + g.lastError + QChar( ')' ) );
}

bool lodgenGpuEncodeBc7( const quint32 * px, int w, int h, const int wt[4], quint8 * out )
{
	GpuState & g = G();
	if ( !g.on || w <= 0 || h <= 0 )
		return false;
	for ( int c = 0; c < 4; c++ )
		if ( wt[c] > 500 )
			return false;     // the GPU error sum is an int; see the shader's header
	const int nb = ( ( w + 3 ) / 4 ) * ( ( h + 3 ) / 4 );
	if ( nb < kMinGpuBlocks )
		return false;
	QMutexLocker lock( &g.jobLock );
	if ( !g.on )
		return false;
	const bool ok = onGpuThread( [&]() { return gpuBc7( px, w, h, wt, out ); } );
	if ( ok ) {
		g.images++;
		g.blocks += quint64( nb );
	} else {
		/* One failure turns the GPU path off for the rest of the run: a lost
		 * context does not come back, and every later image goes straight to
		 * the CPU instead of paying for a failed dispatch first. */
		g.fellBack++;
		g.on = false;
		g.dead = true;
	}
	return ok;
}
