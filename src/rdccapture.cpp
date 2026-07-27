/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#include "rdccapture.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QtGlobal>

#ifdef Q_OS_WIN

#include "renderdoc/renderdoc_app.h"

#include <windows.h>

namespace
{

RENDERDOC_API_1_4_1 * rdcApi = nullptr;
int rdcRemaining = 0;
bool rdcCapturing = false;
QString rdcOutBase;

//! Where renderdoc.dll lives when it was not injected into this process.
QString rdcDefaultDll()
{
	const QString fromEnv = qEnvironmentVariable( "WW_RDC_DLL" );
	if ( !fromEnv.isEmpty() )
		return fromEnv;
	// The install this was developed against. Only a default: WW_RDC_DLL wins,
	// and an injected renderdoc.dll is found without either.
	return QStringLiteral( "E:/Tools/RenderDoc/RenderDoc_1.45_64/renderdoc.dll" );
}

} // namespace

bool rdcInit()
{
	rdcRemaining = qEnvironmentVariableIntValue( "WW_RDC_FRAMES" );
	if ( rdcRemaining < 1 )
		return false;

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

	rdcOutBase = qEnvironmentVariable( "WW_RDC_OUT" );
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
		const QString path = QStringLiteral( "%1_%2" ).arg( rdcOutBase, label );
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
