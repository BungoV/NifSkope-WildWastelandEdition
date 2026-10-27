/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#include "wwdropdown.h"
#include "wwswitch.h"

#include <QAction>
#include <QActionGroup>
#include <QFontMetrics>
#include <QMenu>

#include <algorithm>

QString wwBoxedButtonQss( const QString & padding );	// src/nifskope_ui.cpp (declared in wwskin.h)

namespace {
const char * const kPadding = "3px 6px";	// the Object Mode selector's
}

WwDropdown::WwDropdown( QWidget * parent ) : QToolButton( parent )
{
	if ( wwEnvStr( "WW_DROPDOWN_RED" ) != QLatin1String( "1" ) )	// red: a plain tool button (the unit tests')
		applyStyle( this );
	setToolButtonStyle( Qt::ToolButtonTextOnly );	// bungo: no icons, just text
	setSizePolicy( QSizePolicy::Preferred, QSizePolicy::Fixed );
	popup = new QMenu( this );
	group = new QActionGroup( popup );
	group->setExclusive( true );
	setMenu( popup );
	connect( group, &QActionGroup::triggered, this, [this]( QAction * a ) {
		const int i = int( acts.indexOf( a ) );
		if ( i < 0 )
			return;
		setCurrentIndex( i );
		emit activated( i );
	} );
}

void WwDropdown::applyStyle( QToolButton * button )
{
	if ( !button )
		return;
	button->setPopupMode( QToolButton::InstantPopup );
	button->setAutoRaise( false );
	button->setStyleSheet( wwBoxedButtonQss( QLatin1String( kPadding ) ) );
	button->setProperty( "wwDropdown", true );
}

bool WwDropdown::hasStyle( const QWidget * w )
{
	auto * b = qobject_cast<const QToolButton *>( w );
	return b && b->property( "wwDropdown" ).toBool() && b->popupMode() == QToolButton::InstantPopup && !b->autoRaise()
		&& b->styleSheet() == wwBoxedButtonQss( QLatin1String( kPadding ) );
}

int WwDropdown::addItem( const QString & text, const QVariant & data )
{
	QAction * a = popup->addAction( text );
	a->setCheckable( true );
	group->addAction( a );
	acts.append( a );
	QHash<int, QVariant> h;
	if ( data.isValid() )
		h.insert( Qt::UserRole, data );
	roles.append( h );
	refit();
	if ( cur < 0 )
		setCurrentIndex( 0 );	// as a QComboBox: the first item becomes current
	return int( acts.size() ) - 1;
}

void WwDropdown::addItems( const QStringList & texts )
{
	for ( const QString & t : texts )
		addItem( t );
}

void WwDropdown::clear()
{
	for ( QAction * a : acts ) {
		group->removeAction( a );
		popup->removeAction( a );
		delete a;
	}
	acts.clear();
	roles.clear();
	refit();
	setCurrentIndex( -1 );
}

void WwDropdown::setCurrentIndex( int i )
{
	if ( i < -1 || i >= int( acts.size() ) )
		i = -1;
	if ( i >= 0 && !acts.at( i )->isChecked() )
		acts.at( i )->setChecked( true );
	else if ( i < 0 && group->checkedAction() )
		group->checkedAction()->setChecked( false );
	setText( i >= 0 ? acts.at( i )->text() : QString() );
	if ( i == cur )
		return;
	cur = i;
	emit currentIndexChanged( i );
}

void WwDropdown::pick( int i )
{
	if ( i >= 0 && i < int( acts.size() ) && acts.at( i )->isEnabled() )
		acts.at( i )->trigger();
}

QString WwDropdown::itemText( int i ) const
{
	return i >= 0 && i < int( acts.size() ) ? acts.at( i )->text() : QString();
}

QVariant WwDropdown::itemData( int i, int role ) const
{
	if ( i < 0 || i >= int( roles.size() ) )
		return QVariant();
	if ( role == Qt::DisplayRole )
		return acts.at( i )->text();
	return roles.at( i ).value( role );
}

void WwDropdown::setItemData( int i, const QVariant & value, int role )
{
	if ( i < 0 || i >= int( roles.size() ) )
		return;
	roles[i].insert( role, value );
}

void WwDropdown::setItemEnabled( int i, bool on )
{
	if ( i >= 0 && i < int( acts.size() ) )
		acts.at( i )->setEnabled( on );
}

int WwDropdown::findData( const QVariant & value, int role ) const
{
	for ( int i = 0; i < int( roles.size() ); i++ )
		if ( itemData( i, role ) == value )
			return i;
	return -1;
}

int WwDropdown::findText( const QString & text ) const
{
	for ( int i = 0; i < int( acts.size() ); i++ )
		if ( acts.at( i )->text() == text )
			return i;
	return -1;
}

void WwDropdown::refit()
{
	/* As the mode selector: wide enough for the longest label, so the row does not shift when the pick changes.
	 * The text width plus 24 covers the sheet's padding, the border and the menu arrow (no icon here). */
	ensurePolished();
	const QFontMetrics fm( font() );
	int w = 0;
	for ( QAction * a : acts )
		w = std::max( w, fm.horizontalAdvance( a->text() ) );
	fitWidth = w + 24;
	updateGeometry();
}

QSize WwDropdown::sizeHint() const
{
	QSize s = QToolButton::sizeHint();
	s.setWidth( std::max( s.width(), fitWidth ) );
	return s;
}

QSize WwDropdown::minimumSizeHint() const
{
	return sizeHint();
}
