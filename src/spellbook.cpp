/***** BEGIN LICENSE BLOCK *****

BSD License

Copyright (c) 2005-2015, NIF File Format Library and Tools
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:
1. Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.
3. The name of the NIF File Format Library and Tools project may not be
   used to endorse or promote products derived from this software
   without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

***** END LICENCE BLOCK *****/

#include "spellbook.h"

#include <QCache>
#include <QDir>
#include <QMessageBox>
#include <QPushButton>



//! \file spellbook.cpp SpellBook implementation

QList<SpellPtr> & SpellBook::spells()
{
	static QList<SpellPtr> _spells = QList<SpellPtr>();
	return _spells;
}

QList<SpellBook *> & SpellBook::books()
{
	static QList<SpellBook *> _books = QList<SpellBook *>();
	return _books;
}

QMultiHash<QString, SpellPtr> & SpellBook::hash()
{
	static QMultiHash<QString, SpellPtr> _hash = QMultiHash<QString, SpellPtr>();
	return _hash;
}

QList<SpellPtr> & SpellBook::instants()
{
	static QList<SpellPtr> _instants = QList<SpellPtr>();
	return _instants;
}

QList<SpellPtr> & SpellBook::sanitizers()
{
	static QList<SpellPtr> _sanitizers = QList<SpellPtr>();
	return _sanitizers;
}

QList<SpellPtr>& SpellBook::checkers()
{
	static QList<SpellPtr> _checkers = QList<SpellPtr>();
	return _checkers;
}

SpellBook::SpellBook( NifModel * nif, const QModelIndex & index, QObject * receiver, const char * member ) : QMenu(), Nif( 0 )
{
	setTitle( "Spells" );

	// register this book in the library
	books().append( this );

	// attach this book to the specified nif
	sltNif( nif );

	// fill in the known spells
	for ( SpellPtr spell : spells() ) {
		newSpellRegistered( spell );
	}

	// set the current index
	sltIndex( index );

	connect( this, &SpellBook::triggered, this, &SpellBook::sltSpellTriggered );

	if ( receiver && member )
		connect( this, SIGNAL( sigIndex( const QModelIndex & ) ), receiver, member );
}

SpellBook::~SpellBook()
{
	books().removeAll( this );
}

/*! One segment of a Spell::group() path, as a menu title.
 *
 *  Qt reads '&' in an action or menu title as the mnemonic marker, so a group
 *  called "Select & View" would render as "Select  View" with a keyboard
 *  accelerator on the space. Doubling it displays the character.
 *
 *  Applied on both sides of every title comparison in this file, so the group
 *  strings the spells declare stay literal and only the display is escaped.
 */
static QString menuTitleFor( const QString & groupSegment )
{
	QString t = groupSegment;
	return t.replace( QLatin1Char( '&' ), QLatin1String( "&&" ) );
}

bool Spell::bookConfirmed = false;

void SpellBook::cast( NifModel * nif, const QModelIndex & index, SpellPtr spell )
{
	if ( !spell || !spell->isApplicable( nif, index ) )
		return;

	// Cast non-modifying spells
	if ( spell->constant() ) {
		auto idx = spell->cast( nif, index );
		emit sigIndex( idx );
		return;
	}

	/* Only the spells that destroy something ask, and they ask in their own
	 * words. There is deliberately no "do not ask me again": the whole failure
	 * of the prompt this replaces was that one tick disarmed it for the six
	 * operations that can empty a file. See Spell::destructive().
	 */
	bool asked = false;
	if ( spell->destructive() ) {
		QString what = spell->destructiveWarning( nif, index );
		if ( what.isEmpty() )
			what = tr( "%1 destroys data this file cannot get back, and it cannot be undone." )
				.arg( spell->name() );

		QMessageBox box( QMessageBox::Warning, spell->name(), what, QMessageBox::NoButton, this );
		// The go-ahead button is labelled with the operation, not "OK" — the one
		// place a stray Return must not land is on a button whose text is agreement.
		QPushButton * go = box.addButton( spell->name(), QMessageBox::AcceptRole );
		box.setDefaultButton( box.addButton( QMessageBox::Cancel ) );	// Enter and Esc back out
		box.exec();
		// clickedButton() is null when the dialog is closed by the title bar, so
		// this tests for consent rather than for refusal
		if ( box.clickedButton() != go )
			return;
		asked = true;
	}

	// tells a spell that also warns from its own cast() that the question has
	// already been put to the user on this path
	struct ConfirmScope
	{
		explicit ConfirmScope( bool on ) : was( Spell::bookConfirmed ) { Spell::bookConfirmed = on; }
		~ConfirmScope() { Spell::bookConfirmed = was; }
		bool was;
	} confirmScope( asked );

	bool noSignals = spell->batch();
	if ( noSignals )
		nif->setState( BaseModel::Processing );
	// Cast the spell and return index
	auto idx = spell->cast( nif, index );
	if ( noSignals )
		nif->resetState();

	// Refresh the header
	nif->invalidateHeaderConditions();
	nif->updateHeader();

	if ( nif->getProcessingResult() ) {
		QModelIndex i = idx;
		if ( !i.isValid() )
			i = nif->getRootIndex();
		emit nif->dataChanged( i, i );
	}

	emit sigIndex( idx );
}

void SpellBook::sltSpellTriggered( QAction * action )
{
	// A SpellBook can also host native application actions (for example the
	// Block List's Hierarchy submenu). Only actions registered as spells belong
	// on the spell-casting path.
	auto it = Map.constFind( action );
	if ( it == Map.constEnd() )
		return;
	cast( Nif, Index, it.value() );
}

void SpellBook::sltNif( NifModel * nif )
{
	if ( Nif )
		disconnect( Nif, &NifModel::modelReset, this, static_cast<void (SpellBook::*)()>(&SpellBook::checkActions) );

	Nif = nif;
	Index = QModelIndex();

	if ( Nif )
		connect( Nif, &NifModel::modelReset, this, static_cast<void (SpellBook::*)()>(&SpellBook::checkActions) );
}

void SpellBook::sltIndex( const QModelIndex & index )
{
	if ( index.model() == Nif )
		Index = index;
	else
		Index = QModelIndex();

	checkActions();
}

void SpellBook::checkActions()
{
	checkActions( this );
}

/*! Enable/disable/hide each entry against the current index.
 *
 *  This used to find the spell behind an action by scanning all 200 of them for
 *  one whose name() matched the action's text AND whose page() matched the
 *  submenu title. That was a linear search per action per index change, but the
 *  real problem was that it was AMBIGUOUS: four spells are called "Choose" and
 *  three are called "Update", and they were kept apart only by living on
 *  different pages. Two spells sharing a name on one page — which merging pages
 *  into groups makes easy to do by accident — would each overwrite the other's
 *  enabled state, and the last one in registration order would win.
 *
 *  Map already holds the exact QAction -> Spell relation, built at registration.
 *  Using it is both faster and unambiguous, and it is what sltSpellTriggered has
 *  always done to decide which spell an action casts. A menu that only contains
 *  actions we know nothing about is left alone rather than disabled — SpellBooks
 *  also host native actions (the Block List's Hierarchy submenu), and those are
 *  not ours to judge.
 */
void SpellBook::checkActions( QMenu * menu )
{
	bool menuEnable = false;
	bool sawSpell = false;
	for ( QAction * action : menu->actions() ) {
		if ( action->menu() ) {
			checkActions( action->menu() );
			menuEnable |= action->menu()->isEnabled();
			action->setVisible( action->menu()->isEnabled() );
			sawSpell = true;
		} else {
			auto it = Map.constFind( action );
			if ( it == Map.constEnd() )
				continue;
			sawSpell = true;
			bool actionEnable = Nif && it.value()->isApplicable( Nif, Index );
			action->setVisible( actionEnable );
			action->setEnabled( actionEnable );
			menuEnable |= actionEnable;
		}
	}
	menu->setEnabled( sawSpell ? menuEnable : true );
}

void SpellBook::newSpellRegistered( SpellPtr spell )
{
	QMenu * menu = nullptr;

	/* Built from group(), not page(). group() is a '/'-separated PATH, so one
	 * walk handles both levels: "Material/Textures" finds-or-creates Material,
	 * then finds-or-creates Textures inside it.
	 *
	 * Only Material needs the second level today — the nine legacy
	 * NiTexturingProperty "Add … Map" spells, which cannot fire on anything
	 * Bethesda has shipped since Skyrim, and which crowded out the entries that
	 * can. But the walk costs nothing and the alternative is a second
	 * special-cased title, which is exactly how the two-page hoist that
	 * orderGroups replaced got started.
	 */
	const QStringList path = spell->group().split( QLatin1Char( '/' ), Qt::SkipEmptyParts );
	for ( const QString & segment : path ) {
		const QString title = menuTitleFor( segment );
		QMenu * parent = menu;
		QMenu * found = nullptr;
		for ( QAction * action : ( parent ? parent->actions() : actions() ) ) {
			if ( action->menu() && action->menu()->title() == title ) {
				found = action->menu();
				break;
			}
		}

		if ( !found ) {
			found = new QMenu( title, parent ? static_cast<QWidget *>( parent )
											 : static_cast<QWidget *>( this ) );
			if ( parent )
				parent->addMenu( found );
			else
				addMenu( found );
		}
		menu = found;
	}

	QAction * act;
	if ( menu )
		act = menu->addAction( spell->icon(), spell->label() );
	else
		act = addAction( spell->icon(), spell->label() );
	act->setShortcut( spell->hotkey() );
	/* Spell::hint() was declared "Unused?" and had zero overrides tree-wide, while
	 * the same sentences sat in a static table inside the Unfuck dialog where only
	 * that dialog could read them. One line here puts them on every menu a
	 * SpellBook builds — which is now visible, because the Block List's context
	 * menu calls setToolTipsVisible(true).
	 */
	act->setToolTip( spell->hint() );
	Map.insert( act, spell );

	orderGroups();
}

/*! Put the submenus in a DECLARED order rather than in link order.
 *
 *  Submenus are created the first time a spell on that page registers, so the
 *  top-level order of this menu was an artefact of the order the object files
 *  happen to be listed in NifSkope.pro. Reordering SOURCES silently reordered
 *  the user's context menu.
 *
 *  This replaces a hardcoded special case that did the same thing for exactly
 *  two pages — hoisting Block to sit directly under Transform — with the same
 *  remove-then-insert logic walked over a list. Pages not named here keep their
 *  established relative order, after the named ones, so adding a page does not
 *  require touching this table.
 *
 *  Deliberately ORDER only. Nothing here renames a group or moves a spell
 *  between groups — that is what Spell::group() is for. This walks whatever
 *  groups exist and puts the ones it knows about in sequence.
 *
 *  Note Spell::tr and not SpellBook's own tr: these titles were produced by
 *  Spell::group(), whose overrides all wrap their string in Spell::tr, so a
 *  translated build has to look the title up in the same context it was
 *  translated in. The two agree in an English build, which is why the version
 *  this replaced got away with the wrong one.
 */
void SpellBook::orderGroups()
{
	static const char * const groupOrder[] = {
		"Block", "Add", "Transform", "Select & View", "Geometry", "Recompute",
		"Skinning", "Material", "Collision", "Animation", "Flags",
		"Import & Export", "Fix", "Info",
	};

	/* Both passes below have to ask the same question, and when they asked it in
	 * two hand-written copies they disagreed: one compared the escaped title and
	 * one the raw group name, so "Import & Export" — the only group with an '&'
	 * in it — was ordered by the first pass and then left behind by the second.
	 */
	auto isGroup = []( const QAction * a, const char * name ) {
		return a->menu() && a->menu()->title() == menuTitleFor( Spell::tr( name ) );
	};

	// walk backwards, inserting each named group in front of the one after it, so
	// a single pass leaves them in the declared sequence
	QAction * anchor = nullptr;
	for ( int i = int( std::size( groupOrder ) ) - 1; i >= 0; i-- ) {
		QAction * found = nullptr;
		for ( QAction * a : actions() )
			if ( isGroup( a, groupOrder[i] ) ) { found = a; break; }
		if ( !found )
			continue;
		removeAction( found );
		if ( anchor )
			insertAction( anchor, found );
		else
			addAction( found );
		anchor = found;
	}

	/* The named groups are now in order but sitting at the BOTTOM, because each
	 * removeAction/addAction pair moved them past everything unnamed. One more
	 * pass lifts the whole run to the front, which is where the structural
	 * categories belong.
	 */
	if ( anchor ) {
		QList<QAction *> named;
		for ( const char * name : groupOrder )
			for ( QAction * a : actions() )
				if ( isGroup( a, name ) ) { named.append( a ); break; }
		QAction * first = actions().isEmpty() ? nullptr : actions().first();
		if ( first && !named.contains( first ) ) {
			for ( QAction * a : named ) {
				removeAction( a );
				insertAction( first, a );
			}
		}
	}
}

QAction * SpellBook::actionFor( const QString & spellName ) const
{
	for ( auto it = Map.constBegin(); it != Map.constEnd(); ++it )
		if ( it.value() && it.value()->name() == spellName )
			return it.key();
	return nullptr;
}

void SpellBook::registerSpell( SpellPtr spell )
{
	spells().append( spell );
	hash().insert( spell->name(), spell );

	if ( spell->instant() )
		instants().append( spell );

	if ( spell->sanity() )
		sanitizers().append( spell );

	if ( spell->checker() )
		checkers().append( spell );

	for ( SpellBook * book : books() ) {
		book->newSpellRegistered( spell );
	}
}

SpellPtr SpellBook::lookup( const QString & id )
{
	if ( id.isEmpty() )
		return nullptr;

	QString page;
	QString name = id;

	if ( id.contains( "/" ) ) {
		QStringList split = id.split( "/" );
		page = split.value( 0 );
		name = split.value( 1 );
	}

	for ( SpellPtr spell : hash().values( name ) ) {
		if ( spell->page() == page )
			return spell;
	}

	return nullptr;
}

QList<SpellPtr> SpellBook::lookup( const QKeySequence & hotkey )
{
	QList<SpellPtr> spellsFound;

	if ( !hotkey.isEmpty() ) {
		for ( SpellPtr spell : spells() ) {
			if ( spell->hotkey() == hotkey )
				spellsFound.append( spell );
		}
	}

	return spellsFound;
}

SpellPtr SpellBook::instant( const NifModel * nif, const QModelIndex & index )
{
	for ( SpellPtr spell : instants() ) {
		if ( spell->isApplicable( nif, index ) )
			return spell;
	}
	return nullptr;
}

QModelIndex SpellBook::sanitize( NifModel * nif )
{
	QPersistentModelIndex ridx;

	for ( SpellPtr spell : sanitizers() ) {
		if ( spell->isApplicable( nif, QModelIndex() ) ) {
			QModelIndex idx = spell->cast( nif, QModelIndex() );

			if ( idx.isValid() && !ridx.isValid() )
				ridx = idx;
		}
	}

	return ridx;
}

QModelIndex SpellBook::check( NifModel * nif )
{
	QPersistentModelIndex ridx;

	for ( SpellPtr spell : checkers() ) {
		if ( spell->isApplicable(nif, QModelIndex()) ) {
			QModelIndex idx = spell->cast(nif, QModelIndex());

			if ( idx.isValid() && !ridx.isValid() )
				ridx = idx;
		}
	}

	return ridx;
}

QAction * SpellBook::exec( const QPoint & pos, QAction * act )
{
	if ( isEnabled() )
		return QMenu::exec( pos, act );

	return nullptr;
}
