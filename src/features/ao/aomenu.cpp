// lane MODULES1: the AO module's Cell workspace rows (moved out of src/cellworkspace.cpp; same rows, same names)
#include "ao.h"

#include <QAction>
#include <QActionGroup>
#include <QCoreApplication>
#include <QMenu>
#include <QStringList>

namespace {
QString tr( const char * s ) { return QCoreApplication::translate( "CellWorkspace", s ); }

QMenu * pickMenu( QMenu * parent, const QString & title, const char * name, const QStringList & names, int cur,
	bool pinned, QObject * ctx, std::function<void( int )> set, std::function<void()> changed )
{
	QMenu * m = parent->addMenu( title );
	m->setObjectName( QString::fromLatin1( name ) );
	auto * grp = new QActionGroup( m );
	grp->setExclusive( true );
	for ( int k = 0; k < names.size(); k++ ) {
		QAction * a = m->addAction( names.at( k ) );
		a->setCheckable( true );
		a->setData( k );
		a->setChecked( k == cur );
		grp->addAction( a );
	}
	m->menuAction()->setEnabled( !pinned );
	QObject::connect( grp, &QActionGroup::triggered, ctx, [set, changed]( QAction * a ) {
		set( a->data().toInt() );
		if ( changed )
			changed();
	} );
	return m;
}
}

// lane OPENCELLFIX2 item 27 (lane SSAO2's methods): inline on the Ambient Occlusion row; a pin greys it
QMenu * wwAoMethodMenu( QMenu * parent, QObject * ctx, std::function<void()> changed )
{
	const QStringList names = { tr( "Vanilla Screen-Space Ambient Occlusion" ), tr( "Ground Truth Ambient Occlusion" ),
		tr( "Intel Xe Ground Truth Ambient Occlusion" ), tr( "Horizon-Based Ambient Occlusion Plus" ) };
	return pickMenu( parent, tr( "Ambient Occlusion method" ), "CellWorkspaceAoMethod", names, wwCellAoMethod(),
		wwCellAoMethodPinned(), ctx, []( int k ) { wwCellAoSetMethod( k ); }, changed );	// WW/CellAoMethod
}

// the AO quality tier (cellaomethod.h); the built-in presets pick it by name; live
QMenu * wwAoQualityMenu( QMenu * parent, QObject * ctx, std::function<void()> changed )
{
	const QStringList names = { tr( "Low" ), tr( "Medium" ), tr( "High" ), tr( "Ultra" ) };
	return pickMenu( parent, tr( "Ambient Occlusion quality" ), "CellWorkspaceAoQuality", names, wwCellAoQuality(),
		wwCellAoQualityPinned(), ctx, []( int k ) { wwCellAoSetQuality( k ); }, changed );	// WW/CellAoQuality
}
