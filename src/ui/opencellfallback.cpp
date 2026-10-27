/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#include "opencelldialog.h"
#include "cellmodsource.h"

#include <QDir>
#include <QFileInfo>

// lane OPENCELLFIX1: the Cell dock's load order when the saved one is gone: the same source the Mods list uses
// (the LOD panel's MO2 profile, an MO2 on this machine, else the game's Data folder and plugins.txt).
OpenCellFallback openCellFallback()
{
	OpenCellFallback fb;
	const CellModSource s = cellModSource();
	for ( const QString & p : s.plugins ) {
		if ( QFileInfo( p ).fileName().compare( QLatin1String( "Fallout4.esm" ), Qt::CaseInsensitive ) == 0 ) {
			fb.dataDir = QDir::cleanPath( QFileInfo( p ).absolutePath() );
			break;
		}
	}
	if ( s.kind == QLatin1String( "mo2" ) ) {
		fb.pluginsTxt = QDir( s.from ).filePath( QStringLiteral( "plugins.txt" ) );
	} else if ( s.kind == QLatin1String( "data" ) || s.kind == QLatin1String( "masters" ) ) {
		if ( fb.dataDir.isEmpty() )
			fb.dataDir = QDir::cleanPath( s.from );
		const QString local = qEnvironmentVariable( "LOCALAPPDATA" );
		const QString txt = QDir( local ).filePath( QStringLiteral( "Fallout4/plugins.txt" ) );
		if ( !local.isEmpty() && QFileInfo( txt ).isFile() )
			fb.pluginsTxt = txt;
	}
	if ( !fb.pluginsTxt.isEmpty() && !QFileInfo( fb.pluginsTxt ).isFile() )
		fb.pluginsTxt.clear();
	return fb;
}
