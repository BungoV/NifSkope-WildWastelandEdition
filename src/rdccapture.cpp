/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#include "rdccapture.h"
#include "wwswitch.h"	// lane TESTDRIVER1: the WW_* switch store

#ifdef WW_TRACY
#include "features/shared/cellpasstime.h"	// PERFCPU1: the hold's begin/end as Tracy messages
#endif

#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QtGlobal>

#include <algorithm>
#include <cstdio>

#ifdef Q_OS_WIN

#include "renderdoc/renderdoc_app.h"

#include <windows.h>

namespace
{

RENDERDOC_API_1_4_1 * rdcApi = nullptr;
int rdcRemaining = 0;
bool rdcCapturing = false;
QString rdcOutBase;
QString rdcTag;	// PERFCPU1: WW_RDC_ARM's tag, in the armed captures' file names

//! Where renderdoc.dll lives when it was not injected into this process.
QString rdcDefaultDll()
{
	const QString fromEnv = wwEnvStr( "WW_RDC_DLL" );
	if ( !fromEnv.isEmpty() )
		return fromEnv;
	// The install this was developed against. Only a default: WW_RDC_DLL wins,
	// and an injected renderdoc.dll is found without either.
	return QStringLiteral( "E:/Tools/RenderDoc/RenderDoc_1.45_64/renderdoc.dll" );
}

} // namespace

bool rdcInit()
{
	rdcRemaining = wwEnvInt( "WW_RDC_FRAMES" );
	if ( rdcRemaining < 1 && wwEnvInt( "WW_RDC_LOAD" ) != 1 )	// PERFCPU1: WW_RDC_LOAD=1 loads it disarmed (WW_RDC_ARM)
		return false;
	if ( rdcRemaining < 0 )
		rdcRemaining = 0;

	// Already injected (launched from the RenderDoc UI) - reuse that instance
	// rather than loading a second copy.
	HMODULE mod = GetModuleHandleA( "renderdoc.dll" );
	if ( !mod ) {
		const QString dll = QDir::toNativeSeparators( rdcDefaultDll() );
		if ( !QFileInfo::exists( dll ) ) {
			qWarning() << "WW_RDC_FRAMES set but renderdoc.dll not found at" << dll
					   << "- set WW_RDC_DLL";
			rdcRemaining = 0;
			return false;
		}
		mod = LoadLibraryW( reinterpret_cast<const wchar_t *>( dll.utf16() ) );
		if ( !mod ) {
			qWarning() << "WW_RDC_FRAMES set but LoadLibrary failed for" << dll;
			rdcRemaining = 0;
			return false;
		}
	}

	auto getApi = reinterpret_cast<pRENDERDOC_GetAPI>( GetProcAddress( mod, "RENDERDOC_GetAPI" ) );
	if ( !getApi || getApi( eRENDERDOC_API_Version_1_4_1, reinterpret_cast<void **>( &rdcApi ) ) != 1 ) {
		qWarning() << "renderdoc.dll loaded but RENDERDOC_GetAPI failed";
		rdcApi = nullptr;
		rdcRemaining = 0;
		return false;
	}

	rdcOutBase = wwEnvStr( "WW_RDC_OUT" );
	if ( rdcOutBase.isEmpty() )
		rdcOutBase = QCoreApplication::applicationDirPath() + QStringLiteral( "/ww_rdc" );
	rdcApi->SetCaptureFilePathTemplate( rdcOutBase.toLocal8Bit().constData() );

	// No overlay: these runs are usually grabbed with grabFramebuffer(), and the
	// overlay would end up in the PNG and in any pixel comparison.
	rdcApi->MaskOverlayBits( 0, 0 );

	return true;
}

bool rdcArmed()
{
	return rdcApi && rdcRemaining > 0;
}

int rdcFramesRemaining()
{
	return rdcApi ? rdcRemaining : 0;
}

void rdcBeginFrame( const QString & label )
{
	if ( !rdcArmed() || rdcCapturing )
		return;
	// One file per labelled capture, so a run that grabs several different
	// renders is readable afterwards without matching timestamps. Done through
	// the path template rather than SetCaptureTitle, which only exists from API
	// 1.6 and would be a null pointer in the struct this asks for.
	if ( !label.isEmpty() ) {
		const QString path = rdcTag.isEmpty() ? QStringLiteral( "%1_%2" ).arg( rdcOutBase, label )
			: QStringLiteral( "%1_%2_%3" ).arg( rdcOutBase, rdcTag, label );
		rdcApi->SetCaptureFilePathTemplate( path.toLocal8Bit().constData() );
	}
	// Null device and window: capture the context that is current right now,
	// which is the whole point - the viewport's context, not the compositor's.
	rdcApi->StartFrameCapture( nullptr, nullptr );
	rdcCapturing = true;
}

void rdcEndFrame()
{
	if ( !rdcApi || !rdcCapturing )
		return;
	rdcApi->EndFrameCapture( nullptr, nullptr );
	rdcCapturing = false;
	if ( rdcRemaining > 0 )
		rdcRemaining--;
}

#else // !Q_OS_WIN

bool rdcInit() { return false; }
bool rdcArmed() { return false; }
int rdcFramesRemaining() { return 0; }
void rdcBeginFrame( const QString & ) {}
void rdcEndFrame() {}

#endif

QString rdcAfterPicture( const std::function<void( int )> & pump )
{
	const int hold = std::max( 0, wwEnvInt( "WW_RDC_HOLD" ) );
	const QString arm = wwEnvStr( "WW_RDC_ARM" ).trimmed();
	if ( hold == 0 && arm.isEmpty() )
		return QString();	// unset: the picture's reply and its frames are exactly as before
	const int n = arm.section( QLatin1Char( ':' ), 0, 0 ).toInt();
	const QString tag = arm.section( QLatin1Char( ':' ), 1 ).trimmed();
	QString r;
	if ( hold > 0 ) {
		// a window of settled frames for an outside profiler (tracy-capture); the hold script waits for "begin"
		QElapsedTimer t;
		t.start();
		const QByteArray b = QStringLiteral( "rdc hold: %1 %2 frames begin" ).arg( tag ).arg( hold ).toUtf8();
		std::fprintf( stderr, "%s\n", b.constData() );
		std::fflush( stderr );
#ifdef WW_TRACY
		PassTime::tracyMessage( b.constData() );
#endif
		pump( hold );
		const QByteArray e = QStringLiteral( "rdc hold: %1 end after %2 ms" ).arg( tag ).arg( t.elapsed() ).toUtf8();
		std::fprintf( stderr, "%s\n", e.constData() );
		std::fflush( stderr );
#ifdef WW_TRACY
		PassTime::tracyMessage( e.constData() );
#endif
		r += QStringLiteral( "; held %1 frames in %2 ms" ).arg( hold ).arg( t.elapsed() );
	}
	if ( n > 0 ) {
#ifdef Q_OS_WIN
		if ( !rdcApi )
			return r + QStringLiteral( "; rdc arm %1 refused: renderdoc not loaded (launch with WW_RDC_LOAD=1)" ).arg( n );
		rdcTag = tag;
		rdcRemaining = n;
		pump( n + 1 );	// the paint that begins each capture ends it; one more so the last one is closed
		rdcTag.clear();
		const int left = rdcRemaining;
		rdcRemaining = 0;
		r += QStringLiteral( "; rdc armed %1 (%2) captured %3" ).arg( n ).arg( tag ).arg( n - left );
#else
		r += QStringLiteral( "; rdc arm refused: not Windows" );
#endif
	}
	return r;
}
