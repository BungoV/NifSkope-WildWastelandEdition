/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#include "opencellmenu.h"

#include "opencellcatalog.h"
#include "opencelldialog.h"

#include <QAction>
#include <QDir>
#include <QFileInfo>
#include <QMenu>

void openCellInstallMenu( QMenu * file, QAction * openBefore, QAction * recentBefore, QWidget * parent,
	const std::function<void( const QString & )> & open )
{
	auto * openCell = new QAction( QObject::tr( "Open Cell..." ), file );
	openCell->setObjectName( QStringLiteral( "aOpenCell" ) );
	file->insertAction( openBefore, openCell );
	QObject::connect( openCell, &QAction::triggered, parent, [parent, open]() {
		OpenCellDialog dlg( parent );
		if ( dlg.exec() == QDialog::Accepted && !dlg.specPath().isEmpty() )
			open( dlg.specPath() );
	} );

	auto * recent = new QMenu( QObject::tr( "Recent Cells" ), file );
	recent->setObjectName( QStringLiteral( "mRecentCells" ) );
	recent->addAction( QObject::tr( "(none)" ) )->setEnabled( false );   // an empty menu would not open
	file->insertMenu( recentBefore, recent );
	// Rebuilt on every show, so a cell opened any other way is listed too.
	QObject::connect( recent, &QMenu::aboutToShow, recent, [recent, open]() {
		recent->clear();
		const QStringList cells = openCellRecent();
		for ( const QString & p : cells )
			recent->addAction( QFileInfo( p ).completeBaseName(), [open, p]() { open( p ); } )
				->setToolTip( QDir::toNativeSeparators( p ) );
		if ( cells.isEmpty() )
			recent->addAction( QObject::tr( "(none)" ) )->setEnabled( false );
	} );
	recent->setToolTipsVisible( true );
}
