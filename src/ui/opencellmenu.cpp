/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#include "opencellmenu.h"

#include "opencellcatalog.h"

#include <QAction>
#include <QDir>
#include <QFileInfo>
#include <QMenu>

namespace {
std::function<void()> & showPicker()
{
	static std::function<void()> f;
	return f;
}
}

void openCellSetShowPicker( const std::function<void()> & show )
{
	showPicker() = show;
}

void openCellInstallMenu( QMenu * file, QAction * openBefore, QAction * recentBefore, QWidget * parent,
	const std::function<void( const QString & )> & open )
{
	auto * openCell = new QAction( QObject::tr( "Open Cell..." ), file );
	openCell->setObjectName( QStringLiteral( "aOpenCell" ) );
	file->insertAction( openBefore, openCell );
	QObject::connect( openCell, &QAction::triggered, parent, []() {
		if ( showPicker() )
			showPicker()();
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
