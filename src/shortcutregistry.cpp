#include "shortcutregistry.h"

#include <QSettings>

//! @file shortcutregistry.cpp ShortcutRegistry

ShortcutRegistry & ShortcutRegistry::get()
{
	static ShortcutRegistry instance;
	return instance;
}

void ShortcutRegistry::reg( const QString & id, const QString & label,
	const QString & category, const QKeySequence & def )
{
	if ( map.contains( id ) )
		return;
	Entry e{ id, label, category, def, def };
	QSettings settings;
	const QVariant v = settings.value( QStringLiteral( "Shortcuts/" ) + id );
	if ( v.isValid() )
		e.cur = QKeySequence::fromString( v.toString(), QKeySequence::PortableText );
	map.insert( id, e );
}

bool ShortcutRegistry::matches( const QString & id, int key, Qt::KeyboardModifiers mods ) const
{
	auto it = map.constFind( id );
	if ( it == map.constEnd() || it->cur.isEmpty() )
		return false;
	// a lone modifier key can never complete a binding
	if ( key <= 0 || key == Qt::Key_Control || key == Qt::Key_Shift
		|| key == Qt::Key_Alt || key == Qt::Key_Meta || key == Qt::Key_AltGr )
		return false;
	const Qt::KeyboardModifiers m = mods
		& ( Qt::ShiftModifier | Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier );
	return it->cur == QKeySequence( QKeyCombination( m, Qt::Key( key ) ) );
}

QKeySequence ShortcutRegistry::seq( const QString & id ) const
{
	auto it = map.constFind( id );
	return ( it != map.constEnd() ) ? it->cur : QKeySequence();
}

void ShortcutRegistry::setSeq( const QString & id, const QKeySequence & ks )
{
	auto it = map.find( id );
	if ( it == map.end() )
		return;
	it->cur = ks;
	QSettings settings;
	const QString key = QStringLiteral( "Shortcuts/" ) + id;
	if ( ks == it->def )
		settings.remove( key );
	else
		settings.setValue( key, ks.toString( QKeySequence::PortableText ) );
}

void ShortcutRegistry::resetAll()
{
	QSettings settings;
	for ( auto it = map.begin(); it != map.end(); ++it ) {
		it->cur = it->def;
		settings.remove( QStringLiteral( "Shortcuts/" ) + it->id );
	}
}

void ShortcutRegistry::noteActionDefault( const QString & objectName, const QKeySequence & ks )
{
	if ( !objectName.isEmpty() && !actionDefaults.contains( objectName ) )
		actionDefaults.insert( objectName, ks );
}

QKeySequence ShortcutRegistry::actionDefault( const QString & objectName ) const
{
	return actionDefaults.value( objectName );
}

QString ShortcutRegistry::actionSettingsKey( const QString & objectName )
{
	return QStringLiteral( "Shortcuts/action." ) + objectName;
}
