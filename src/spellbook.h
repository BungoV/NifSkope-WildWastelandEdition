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

#ifndef SPELLBOOK_H
#define SPELLBOOK_H

#include "message.h"
#include "model/nifmodel.h"

#include <QMenu> // Inherited
#include <QApplication>
#include <QHash>
#include <QIcon>
#include <QKeySequence>
#include <QList>
#include <QMap>
#include <QPersistentModelIndex>
#include <QString>

#include <memory>


using QIconPtr = std::shared_ptr<QIcon>;

//! \file spellbook.h Spell, SpellBook and Librarian

//! Register a Spell using a Librarian
#define REGISTER_SPELL( SPELL ) static Librarian __ ## SPELL ## __( new SPELL );

//! Flexible context menu magic functions.
class Spell
{
public:
	//! Constructor
	Spell() {}
	//! Destructor
	virtual ~Spell() {}

	//! Name of spell
	virtual QString name() const = 0;

	/*! Text of the menu entry, when it should differ from name().
	 *
	 *  name() is an IDENTIFIER, not a label, and renaming one is not free:
	 *  SpellBook::lookup keys on "Page/Name" (so the CLI's -s argument and the
	 *  Collision Manager's and Rigging dock's castSpell calls break), and a
	 *  dozen spells build QSettings keys out of page() and name() — the user's
	 *  last-used texture path lives under "Spells/Texture/Choose/Last Texture
	 *  Path", the Remove By Id regex under its own name, the whole Simplify
	 *  dialog under "Spells/Mesh/Simplify".
	 *
	 *  Which is why four different spells are all called "Choose" and three are
	 *  called "Update": they were only ever told apart by sitting on different
	 *  pages. Merging pages into groups puts them side by side, so the label has
	 *  to become sayable on its own — and that has to happen without touching
	 *  the id it was hiding behind.
	 */
	virtual QString label() const { return name(); }
	//! Context sub-menu that the spell appears on
	virtual QString page() const { return QString(); }

	/*! Which submenu this spell is filed under, as a '/'-separated path.
	 *
	 *  Separate from page() because page() is overloaded five ways — submenu
	 *  label, CLI namespace (nifcli.cpp), Unfuck membership (unfucktools.cpp),
	 *  the batch() model-signal switch below, and the spell-id syntax that
	 *  SpellBook::lookup parses as "Page/Name". Editing page() strings to fix
	 *  the menu would silently rename CLI spell ids, move spells in and out of
	 *  the Unfuck dialog, and change which casts suppress model signals.
	 *
	 *  So page() is frozen as the internal id and group() is what the menu is
	 *  built from. Defaulting to page() means a spell that is already filed
	 *  correctly needs no override at all.
	 *
	 *  A path nests: "Material/Textures" puts the spell in a Textures submenu
	 *  inside Material. Returning an empty string puts it at the top level.
	 *
	 *  '/' is therefore the separator and CANNOT appear in a group's own name —
	 *  "Import / Export" produced two submenus called "Import " and " Export",
	 *  one nested in the other. '&' is fine: SpellBook doubles it so Qt shows the
	 *  character instead of eating it as a mnemonic marker.
	 */
	virtual QString group() const { return page(); }
	//! Unused?
	virtual QString hint() const { return QString(); }
	//! Icon displayed in block view
	virtual QIcon icon() const { return QIcon(); }
	//! Whether the spell does not modify the file
	virtual bool constant() const { return false; }
	//! Whether the spell records its complete mutation in the model undo stack
	virtual bool undoable() const { return false; }

	/*! Whether casting this spell destroys something the file cannot get back.
	 *
	 *  This replaces the old blanket "this action cannot currently be undone"
	 *  prompt, which fired for nearly every write — `undoable()` is overridden
	 *  true by six spells in the whole tree — and offered "Do not ask me again".
	 *  One tick of that box and Crop To Branch, Remove Branch and Apply
	 *  Transformation all ran silently forever after, which is exactly backwards:
	 *  the prompt was loudest where it mattered least and absent where it
	 *  mattered most.
	 *
	 *  So the question is asked by the few spells that destroy data, it names
	 *  what is lost (see destructiveWarning), and it cannot be suppressed.
	 */
	virtual bool destructive() const { return false; }

	/*! What a destructive spell is about to take away, in its own words.
	 *
	 *  "This cannot be undone" is not information — the user already knows they
	 *  asked for it, and cannot weigh a cost nobody stated. "Delete 214 of 217
	 *  blocks?" is a decision. Return empty only if the spell genuinely cannot
	 *  say more than its own name.
	 */
	virtual QString destructiveWarning( NifModel * nif, const QModelIndex & index ) const
	{
		Q_UNUSED( nif ); Q_UNUSED( index );
		return QString();
	}

	/*! True while a SpellBook cast is running whose destructive confirmation the
	 *  user has already answered.
	 *
	 *  Several of these spells are also used as plain helpers — spRemoveBranch is
	 *  called from six places in havok.cpp and collisiontools.cpp, spApplyTransformation
	 *  from Combine Shapes — through `cast()` directly, which never sees a
	 *  SpellBook. A spell that warns from inside its own `cast()` checks this so
	 *  the menu path shows one dialog rather than two, while the direct path keeps
	 *  whatever guard it had.
	 */
	static bool confirmedByBook() { return bookConfirmed; }
	/*! Whether this spell is driven programmatically and should build no menu entry.
	 *
	 *  For steps in a workflow that a dock owns. The Rigging Manager numbers and
	 *  gates its four transfer steps — 1. Generate CustomizationRemapData through
	 *  4. Transfer Weights — and casts them by id; the flat context submenu
	 *  offered the same four unnumbered and ungated, so nothing stopped you
	 *  running step 4 before step 1.
	 *
	 *  Registration is unaffected: SpellBook::lookup reads the registry, not the
	 *  menu, so castSpell( "Rigging/…" ) keeps working. Only the QAction is not
	 *  created, which also keeps them out of the command palette — correctly,
	 *  since the palette runs entries in isolation and these are a sequence.
	 */
	virtual bool menuHidden() const { return false; }
	//! Whether the spell shows up in block list instead of a context menu
	virtual bool instant() const { return false; }
	//! Whether the spell performs a sanitizing function
	virtual bool sanity() const { return false; }
	//! Whether the spell performs an error checking function
	virtual bool checker() const { return false; }
	//! Whether the spell has a high processing cost
	virtual bool batch() const { return (page() == "Batch") || (page() == "Block") || (page() == "Mesh"); }
	//! Hotkey sequence
	virtual QKeySequence hotkey() const { return QKeySequence(); }

	//! Determine if/when the spell can be cast
	virtual bool isApplicable( const NifModel * nif, const QModelIndex & index ) = 0;

	//! Cast (apply) the spell
	virtual QModelIndex cast( NifModel * nif, const QModelIndex & index ) = 0;

	//! Cast the spell if applicable
	void castIfApplicable( NifModel * nif, const QModelIndex & index )
	{
		if ( isApplicable( nif, index ) )
			cast( nif, index );
	}

	//! i18n wrapper for various strings
	/*!
	 * Note that we don't use QObject::tr() because that doesn't provide
	 * context. We also don't use the QCoreApplication Q_DECLARE_TR_FUNCTIONS()
	 * macro because that won't document properly.
	 *
	 * No spells should reimplement this function.
	 */
	static inline QString tr( const char * key, const char * comment = 0 ) { return QCoreApplication::translate( "Spell", key, comment ); }

private:
	friend class SpellBook;
	//! Set by SpellBook::cast for the duration of a confirmed cast. See confirmedByBook().
	static bool bookConfirmed;
};

using SpellPtr = std::shared_ptr<Spell>;

//! Spell menu
class SpellBook final : public QMenu
{
	Q_OBJECT

public:
	//! Constructor
	SpellBook( NifModel * nif, const QModelIndex & index = QModelIndex(), QObject * receiver = 0, const char * member = 0 );
	//! Destructor
	~SpellBook();

	//! From QMenu: Pops up the menu so that the action <i>act</i> will be at the specified global position <i>pos</i>
	QAction * exec( const QPoint & pos, QAction * act = 0 );

	/*! The action this book built for a named spell, or nullptr.
	 *
	 *  For callers that want to move an entry somewhere else — the Block List
	 *  hoists Copy/Paste/Duplicate Branch out of their submenu into a flat verb
	 *  row. Reparenting the action keeps it wired to its spell, so the hoisted
	 *  copy casts and stays enable-checked like any other; matching on the
	 *  action's TEXT would instead break the moment a label() changed.
	 */
	QAction * actionFor( const QString & spellName ) const;

	//! The spell an action casts, or nullptr when it is a native action this book merely hosts.
	//! The command palette needs it for hint() and destructive().
	SpellPtr spellFor( QAction * action ) const;

	//! Register spell with appropriate books
	static void registerSpell( SpellPtr spell );

	//! Locate spell by name
	static SpellPtr lookup( const QString & id );
	//! Locate spell(s) by hotkey
	static QList<SpellPtr> lookup( const QKeySequence & hotkey );
	//! Locate instant spells by datatype
	static SpellPtr instant( const NifModel * nif, const QModelIndex & index );

	//! Cast all sanitizing spells
	static QModelIndex sanitize( NifModel * nif );

	//! Cast all checking spells
	static QModelIndex check(NifModel * nif);

	static QList<SpellPtr> & spells();
	static QList<SpellPtr> & instants();
	static QList<SpellPtr> & sanitizers();
	static QList<SpellPtr> & checkers();

public slots:
	void sltNif( NifModel * nif );

	void sltIndex( const QModelIndex & index );

	void cast( NifModel * nif, const QModelIndex & index, SpellPtr spell );

	void checkActions();

signals:
	void sigIndex( const QModelIndex & index );

protected slots:
	void sltSpellTriggered( QAction * action );

protected:
	NifModel * Nif;
	QPersistentModelIndex Index;
	QMap<QAction *, SpellPtr> Map;

	void newSpellRegistered( SpellPtr spell );
	//! Reorder the submenus into the declared group order (see orderGroups in the .cpp)
	void orderGroups();
	void checkActions( QMenu * menu );

private:
	static QList<SpellBook *> & books();
	static QMultiHash<QString, SpellPtr> & hash();
};

//! SpellBook manager
class Librarian final
{
public:
	//! Contructor.
	/**
	 * Registers the spell with the appropriate spellbooks.
	 *
	 * \param spell The spell to manage
	 */
	Librarian( Spell * spell )
	{
		SpellBook::registerSpell( SpellPtr( spell ) );
	}
};

#endif
