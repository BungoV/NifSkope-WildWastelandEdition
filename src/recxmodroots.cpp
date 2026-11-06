/* lane WEATHERRECX1: the .recx roots' mod folders (recx.h wwRecxSetModRoots). Fusion Shaders' own data lives in the
 * MO2 mod "Fusion Shaders - Assets", which has no plugin, so the plugin folders never name it: every folder of the
 * profile's resource stack is offered, and wwRecxRoots keeps those that hold Plugins/. Kept out of recx.cpp so the
 * offline gates link recx.cpp without the mod-source resolver. */

#include "recx.h"
#include "cellmodsource.h"

#include <QElapsedTimer>
#include <QFileInfo>

QString wwAssetsModRoot()
{
	const QString pin = qEnvironmentVariable( "WW_ASSETS_MOD" );
	if ( !pin.isEmpty() )
		return QFileInfo( pin ).isDir() ? QFileInfo( pin ).absoluteFilePath() : QString();
	/* the Assets mod itself even when the profile does not enable it yet (a new mod): <MO2>/mods/<provider>, beside the
	 * profile folder's <MO2>/profiles/<name> */
	const CellModSource s = cellModSource();
	if ( s.kind != QLatin1String( "mo2" ) || s.from.isEmpty() )
		return QString();
	const QString assets = QFileInfo( s.from + QStringLiteral( "/../../mods/" ) + wwRecxProvider() ).absoluteFilePath();
	return QFileInfo( assets ).isDir() ? assets : QString();
}

namespace {

QStringList modFolders()
{
	static QStringList cached;
	static QElapsedTimer age;
	if ( age.isValid() && age.elapsed() < 10000 )
		return cached;
	age.start();
	cached.clear();
	// lowest of the mods (the provider's name ranks it in the merge anyway)
	const QString assets = wwAssetsModRoot();
	if ( !assets.isEmpty() )
		cached << assets;
	const CellModSource s = cellModSource();
	for ( const QString & p : s.stack )
		if ( QFileInfo( p ).isDir() && !cached.contains( QFileInfo( p ).absoluteFilePath(), Qt::CaseInsensitive ) )
			cached << p;
	return cached;
}

const bool registered = ( wwRecxSetModRoots( &modFolders ), true );

}	// namespace
