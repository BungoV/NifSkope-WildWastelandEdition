#ifndef SHORTCUTREGISTRY_H
#define SHORTCUTREGISTRY_H

#include <QKeySequence>
#include <QMap>
#include <QString>

//! @file shortcutregistry.h ShortcutRegistry

//! Central table of rebindable shortcuts.
//!
//! Two kinds of bindings live here:
//! - Built-in (hard-coded) key handlers — the 3D viewport operators — register
//!   an Entry with reg() and test key events with matches(). The user override,
//!   if any, is persisted under QSettings "Shortcuts/<id>".
//! - QAction shortcuts are not registered as entries; their overrides are
//!   stored under "Shortcuts/action.<objectName>" and applied by
//!   NifSkope::applyShortcutOverrides(). The registry only records each
//!   action's factory-default sequence (noteActionDefault) so the settings
//!   page can offer per-row resets after an override is active.
class ShortcutRegistry
{
public:
	struct Entry
	{
		QString id;
		QString label;
		QString category;
		QKeySequence def;
		QKeySequence cur;
	};

	static ShortcutRegistry & get();

	//! Register a built-in binding (idempotent); loads any stored override
	void reg( const QString & id, const QString & label, const QString & category,
		const QKeySequence & def );

	//! Exact match of a key event (key + modifiers) against the binding for id
	bool matches( const QString & id, int key, Qt::KeyboardModifiers mods ) const;

	//! The current sequence for id (empty when explicitly unbound or unknown)
	QKeySequence seq( const QString & id ) const;

	//! Change a built-in binding and persist it (== default removes the override)
	void setSeq( const QString & id, const QKeySequence & ks );

	//! Drop every stored built-in override and return to the defaults
	void resetAll();

	const QMap<QString, Entry> & entries() const { return map; }

	//! Record a QAction's factory-default sequence the first time it is seen
	void noteActionDefault( const QString & objectName, const QKeySequence & ks );
	//! The recorded factory default for a QAction (empty if never seen)
	QKeySequence actionDefault( const QString & objectName ) const;

	//! QSettings key for a QAction override
	static QString actionSettingsKey( const QString & objectName );

private:
	QMap<QString, Entry> map;
	QMap<QString, QKeySequence> actionDefaults;
};

#endif // SHORTCUTREGISTRY_H
