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

/*! @file unfucktools.cpp
 *
 *  The Unfuck workspace: everything wrong with the open file, as a list.
 *
 *  This replaces a dialog of checkboxes. The dialog asked the user to decide,
 *  up front and blind, which repairs to run; it never said what was actually
 *  wrong, so the only safe way to use it was to tick everything and hope. A
 *  file is not a set of operations you might want, it is a set of problems it
 *  either has or does not, and that is what this shows.
 *
 *  WHERE THE CONTENT COMES FROM
 *
 *  Nothing here knows anything about NIFs. The "checker" spells already walk
 *  the file and report through `nif->logMessage`, which in MSG_TEST mode lands
 *  in a list rather than a popup — so a scan is: switch the model to test mode,
 *  cast every check, drain the messages, switch back. Severity rides along
 *  (`BaseModel::testMsg` was passing the level through as of this change; it
 *  used to drop it), and most messages embed the offending block as "[19]",
 *  which is what makes Go to possible.
 *
 *  THE HONEST BIT
 *
 *  The plan was a Fix button on every finding. Two things killed it, and both
 *  are worth knowing before anyone tries again.
 *
 *  First, every repair spell acts on the WHOLE FILE — none of them can fix one
 *  occurrence — so a per-finding button could never mean what it appeared to.
 *
 *  Second, and decisively: reading the spells shows that NONE of the four
 *  checks has an automatic repair at all. See the checkFixes table for the
 *  detail. So the panel is split by what it can honestly offer. Issues are
 *  DIAGNOSIS: what is wrong, how bad, and where — each finding takes you to its
 *  block, and each group says plainly that there is no automatic fix. Repairs
 *  are the whole-file operations that do exist, listed separately because they
 *  are not answers to the findings above them and pretending otherwise would be
 *  the same lie one level up.
 */

#include "nifskope.h"
#include "glview.h"
#include "message.h"
#include "nifsnapshot.h"
#include "spellbook.h"
#include "model/nifmodel.h"
#include "wwskin.h"

#include <QApplication>
#include <QClipboard>
#include <QDockWidget>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QRegularExpression>
#include <QToolButton>
#include <QTreeWidget>
#include <QVBoxLayout>


namespace
{

//! Section-heading label, matching the other manager docks.
QLabel * heading( const QString & text, QWidget * parent )
{
	auto * l = new QLabel( text, parent );
	l->setStyleSheet( QStringLiteral( "QLabel { font-weight: 600; }" ) );
	return l;
}

enum Roles
{
	BlockRole = Qt::UserRole + 1,   //!< block number a finding points at, or -1
	SpellRole,                      //!< name() of the check that produced it
	FixRole,                        //!< name() of the spell that repairs it, or empty
};

/*! Which repair answers which check.
 *
 *  Deliberately a short, explicit table rather than anything clever. The
 *  pairing is a claim about what a spell actually changes, so it has to be made
 *  by a person reading both — matching on words in the names would happily pair
 *  "Check Links" with "Collapse Link Arrays", which do not touch the same thing.
 *
 *  An empty fix is not an oversight: several of these problems have no repair
 *  in the spell library at all, and saying so is more useful than hiding it.
 */
struct CheckFix
{
	const char * check;
	const char * fix;          //!< empty when nothing repairs it
	const char * note;         //!< shown when there is no fix
};

/* As it turns out: none of them.
 *
 * This table started with `Invalid Paths -> Adjust Texture Sources`, on the
 * strength of the names. Reading that spell says otherwise — it swaps "/" for
 * "\\" in a NiSourceTexture File Name and, on Oblivion only, overwrites three
 * Format Prefs fields. It cannot add a missing extension, does not touch
 * absolute paths, and never looks at BSEffectShaderProperty or
 * BSShaderTextureSet, which is where every Invalid Paths finding on a modern
 * file comes from. A Fix button there would have been a lie on the one row that
 * offered one.
 *
 * The entries stay, empty, because the emptiness is the point: it is the
 * difference between "we have not wired this up" and "the spell library has no
 * answer to this problem", and the panel says which out loud. Fill one in the
 * day a spell genuinely resolves it.
 */
const CheckFix checkFixes[] = {
	{ "None Refs", "",
	  "No automatic fix — the missing block has to be supplied, or the reference cleared by hand." },
	{ "Invalid Paths", "",
	  "No automatic fix — a path with no extension or an absolute path has to be corrected by hand, or with Search/Replace Resource Paths." },
	{ "Environment Mapping Flags", "",
	  "No automatic fix — the shader flags and the assigned environment map have to be reconciled by hand." },
	{ "Check Links", "",
	  "No automatic fix — a link pointing outside the file usually means the block it wanted was deleted." },
};

//! Block number embedded in a finding, as "[19] ...". -1 when there is none.
int blockOf( const QString & text )
{
	static const QRegularExpression re( QStringLiteral( "^\\[(\\d+)\\]" ) );
	const auto m = re.match( text );
	return m.hasMatch() ? m.captured( 1 ).toInt() : -1;
}

} // namespace


QDockWidget * tlCreateUnfuckManagerDock( NifModel * nif, QMainWindow * mw, GLView * ogl )
{
	Q_UNUSED( ogl );
	auto * skope = qobject_cast<NifSkope *>( mw );

	auto * dock = new QDockWidget( QObject::tr( "Unfuck" ), mw );
	dock->setObjectName( QStringLiteral( "UnfuckManagerDock" ) );
	dock->setAllowedAreas( Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea );

	auto * panel = new QWidget( dock );
	auto * layout = new QVBoxLayout( panel );
	layout->setContentsMargins( 6, 6, 6, 6 );
	layout->setSpacing( 5 );

	layout->addWidget( heading( QObject::tr( "Issues" ), panel ) );

	auto * status = new QLabel( QObject::tr( "Open a NIF to check it." ), panel );
	status->setObjectName( QStringLiteral( "UnfuckStatus" ) );
	status->setWordWrap( true );
	layout->addWidget( status );

	auto * tree = new QTreeWidget( panel );
	tree->setObjectName( QStringLiteral( "UnfuckIssueTree" ) );
	tree->setColumnCount( 2 );
	tree->setHeaderLabels( { QObject::tr( "Issue" ), QString() } );
	tree->header()->setStretchLastSection( false );
	tree->header()->setSectionResizeMode( 0, QHeaderView::Stretch );
	tree->header()->setSectionResizeMode( 1, QHeaderView::ResizeToContents );
	tree->setRootIsDecorated( true );
	tree->setUniformRowHeights( false );
	tree->setSelectionMode( QAbstractItemView::SingleSelection );
	layout->addWidget( tree, 1 );

	auto * buttons = new QWidget( panel );
	auto * bl = new QHBoxLayout( buttons );
	bl->setContentsMargins( 0, 0, 0, 0 );
	bl->setSpacing( 4 );
	auto * rescan = new QPushButton( QObject::tr( "Re-scan" ), buttons );
	rescan->setObjectName( QStringLiteral( "UnfuckRescanButton" ) );
	auto * copyReport = new QToolButton( buttons );
	copyReport->setText( QObject::tr( "Copy report" ) );
	copyReport->setToolTip( QObject::tr( "Copy the whole report to the clipboard" ) );
	bl->addWidget( rescan );
	bl->addStretch( 1 );
	bl->addWidget( copyReport );
	layout->addWidget( buttons );

	/* Repairs, kept apart from the findings above on purpose.
	 *
	 * These are the whole-file operations that genuinely exist. Not one of them
	 * answers a finding in the Issues list — see checkFixes — so putting them
	 * under it, or wiring them to its rows, would suggest a connection that is
	 * not there. They are things you may want to do to a file, listed as that.
	 */
	layout->addWidget( heading( QObject::tr( "Repairs" ), panel ) );
	auto * repairHint = new QLabel( QObject::tr(
		"Whole-file operations. None of these fixes an issue above." ), panel );
	repairHint->setWordWrap( true );
	repairHint->setStyleSheet( QStringLiteral( "color: %1;" ).arg( wwSkinColor( "textMuted" ) ) );
	layout->addWidget( repairHint );

	auto * repairs = new QTreeWidget( panel );
	repairs->setObjectName( QStringLiteral( "UnfuckRepairList" ) );
	repairs->setColumnCount( 1 );
	repairs->setHeaderHidden( true );
	repairs->setRootIsDecorated( false );
	repairs->setMaximumHeight( 150 );
	layout->addWidget( repairs );

	auto * runRepairs = new QPushButton( QObject::tr( "Run checked repairs" ), panel );
	runRepairs->setObjectName( QStringLiteral( "UnfuckRunRepairsButton" ) );
	layout->addWidget( runRepairs );

	// ---------------------------------------------------------------------
	// scanning
	// ---------------------------------------------------------------------

	/* Every check, cast with an INVALID index.
	 *
	 * That is what these spells want — they answer isApplicable only for one,
	 * because they walk the whole file — and it is also why they were unreachable
	 * from the menu bar for so long, which hid them behind the current selection.
	 */
	auto checkSpells = []() {
		/* checker() only, plus a named few.
		 *
		 * This used to take `checker() || constant()`, and that hung the app.
		 * `constant()` promises a spell does not MODIFY the file; it promises
		 * nothing about whether it talks to the user, and a good number of
		 * constant spells open a QMessageBox. Casting them all when the panel
		 * opens meant a modal dialog nobody had asked for, behind a panel that
		 * was still building, with no one to dismiss it — the window simply
		 * stopped responding.
		 *
		 * checker() is the flag that actually means "reports through
		 * logMessage", which is the contract this panel depends on. Anything
		 * else has to be named here, after reading it, and `Check Links` is the
		 * only one so far: it walks the links and logs, and nothing else.
		 */
		static const char * const alsoSilent[] = { "Check Links" };
		QList<SpellPtr> out;
		for ( SpellPtr s : SpellBook::spells() ) {
			if ( !s || out.contains( s ) )
				continue;
			bool wanted = s->checker();
			for ( const char * n : alsoSilent )
				if ( s->name() == QLatin1String( n ) )
					wanted = true;
			if ( wanted )
				out.append( s );
		}
		std::sort( out.begin(), out.end(),
			[]( const SpellPtr & a, const SpellPtr & b ) { return a->name() < b->name(); } );
		return out;
	};

	auto fixFor = []( const QString & check ) -> QPair<QString, QString> {
		for ( const CheckFix & cf : checkFixes )
			if ( check == QLatin1String( cf.check ) )
				// fromUtf8, not fromLatin1: these literals contain em-dashes, and Latin-1
				// turns each one into three mojibake characters in the panel.
				return { QString::fromUtf8( cf.fix ), QString::fromUtf8( cf.note ) };
		return {};
	};

	auto model = [skope, nif]() -> NifModel * {
		NifModel * m = skope ? skope->getNifModel() : nif;
		return ( m && m->getBlockCount() > 0 ) ? m : nullptr;
	};

	auto scan = [=]() {
		tree->clear();
		NifModel * m = model();
		if ( !m ) {
			status->setText( QObject::tr( "Open a NIF to check it." ) );
			return;
		}

		const QColor danger( wwSkinColor( "danger" ) );
		const QColor warn( wwSkinColor( "accent" ) );      // amber, the app's caution colour
		const QColor plain( wwSkinColor( "text" ) );
		const QColor muted( wwSkinColor( "textMuted" ) );

		int errors = 0, warnings = 0, notes = 0;

		const BaseModel::MsgMode was = m->getMessageMode();
		m->setMessageMode( BaseModel::MSG_TEST );
		for ( SpellPtr s : checkSpells() ) {
			if ( !s->isApplicable( m, QModelIndex() ) )
				continue;
			m->getMessages();						// drain anything pending
			s->cast( m, QModelIndex() );
			const QList<TestMessage> found = m->getMessages();
			if ( found.isEmpty() )
				continue;

			auto * group = new QTreeWidgetItem( tree );
			group->setText( 0, QObject::tr( "%1 — %n issue(s)", nullptr, found.size() ).arg( s->name() ) );
			group->setData( 0, SpellRole, s->name() );
			QFont gf = group->font( 0 );
			gf.setBold( true );
			group->setFont( 0, gf );

			// The group takes the worst severity of its children, so a collapsed
			// list still shows where the real problems are.
			QtMsgType worst = QtInfoMsg;
			for ( const TestMessage & msg : found ) {
				const QString text = QString( msg );
				auto * item = new QTreeWidgetItem( group );
				item->setText( 0, text );
				item->setData( 0, BlockRole, blockOf( text ) );
				item->setData( 0, SpellRole, s->name() );

				QColor c = plain;
				if ( msg.type() == QtCriticalMsg || msg.type() == QtFatalMsg ) {
					c = danger; errors++;
					worst = QtCriticalMsg;
				} else if ( msg.type() == QtWarningMsg ) {
					c = warn; warnings++;
					if ( worst != QtCriticalMsg ) worst = QtWarningMsg;
				} else {
					notes++;
				}
				item->setForeground( 0, c );

				if ( blockOf( text ) >= 0 )
					item->setText( 1, QObject::tr( "Go to" ) );
			}
			group->setForeground( 0, worst == QtCriticalMsg ? danger
				: worst == QtWarningMsg ? warn : plain );

			const auto fx = fixFor( s->name() );
			group->setData( 0, FixRole, fx.first );
			if ( !fx.first.isEmpty() ) {
				group->setText( 1, QObject::tr( "Fix all" ) );
			} else if ( !fx.second.isEmpty() ) {
				auto * note = new QTreeWidgetItem( group );
				note->setText( 0, fx.second );
				note->setForeground( 0, muted );
				note->setData( 0, BlockRole, -1 );
			}
			group->setExpanded( true );
		}
		m->setMessageMode( was );

		if ( tree->topLevelItemCount() == 0 ) {
			status->setText( QObject::tr( "No issues found." ) );
			status->setStyleSheet( QString() );
			return;
		}

		QStringList parts;
		if ( errors )   parts << QObject::tr( "%n error(s)", nullptr, errors );
		if ( warnings ) parts << QObject::tr( "%n warning(s)", nullptr, warnings );
		if ( notes )    parts << QObject::tr( "%n note(s)", nullptr, notes );
		status->setText( parts.join( QObject::tr( ", " ) ) );
		status->setStyleSheet( errors
			? QStringLiteral( "color: %1;" ).arg( wwSkinColor( "danger" ) ) : QString() );


	};

	// ---------------------------------------------------------------------
	// acting
	// ---------------------------------------------------------------------

	//! Run one repair, in a single undo step, then re-scan so the list shrinks.
	auto runFix = [=]( const QString & fixName ) {
		NifModel * m = model();
		if ( !m || fixName.isEmpty() )
			return;
		SpellPtr fix = SpellBook::lookup( fixName );
		if ( !fix || !fix->isApplicable( m, QModelIndex() ) ) {
			status->setText( QObject::tr( "%1 does not apply to this file." ).arg( fixName ) );
			return;
		}
		/* The undo stack is detached for the cast: some repair spells snapshot
		 * themselves, and a nested snapshot pushes a second command whose undo
		 * walks the file FORWARD into a half-repaired state. One command, pushed
		 * here, and only if the bytes actually moved.
		 */
		QByteArray before;
		{
			QBuffer buf( &before );
			buf.open( QIODevice::WriteOnly );
			if ( !m->save( buf ) )
				return;
		}
		QUndoStack * stack = m->undoStack;
		m->undoStack = nullptr;
		const BaseModel::MsgMode was = m->getMessageMode();
		m->setMessageMode( BaseModel::MSG_TEST );
		m->getMessages();
		fix->cast( m, QModelIndex() );
		m->invalidateHeaderConditions();
		m->updateHeader();
		m->updateFooter();
		m->getMessages();
		m->setMessageMode( was );
		m->undoStack = stack;

		QByteArray after;
		{
			QBuffer buf( &after );
			buf.open( QIODevice::WriteOnly );
			if ( !m->save( buf ) )
				after = before;
		}
		if ( before != after && m->undoStack )
			m->undoStack->push( new NifSnapshotCommand( m, before, after, fixName ) );

		scan();
		status->setText( before != after
			? QObject::tr( "%1 ran; Ctrl+Z undoes it. %2" ).arg( fixName, status->text() )
			: QObject::tr( "%1 changed nothing." ).arg( fixName ) );
	};


	/* The repair list, rebuilt with the file.
	 *
	 * Each row is armed from the spell's OWN sanity() flag, never from the group
	 * it landed in. Upstream uses that flag to keep dangerous spells out of
	 * automatic runs and says so in the source — Reorder Blocks renumbers every
	 * block and carries the comment "can really only cause issues with rendering
	 * and textureset overrides via the CK" — so a panel that ticked them all by
	 * default would be overriding a judgement it did not make.
	 */
	auto rebuildRepairs = [=]() {
		repairs->clear();
		NifModel * m = model();
		if ( !m )
			return;
		QList<SpellPtr> fixes;
		for ( SpellPtr sp : SpellBook::spells() ) {
			// A repair must change something; anything that only reports belongs
			// in the Issues list above, not here.
			if ( !sp || sp->checker() || sp->constant() || fixes.contains( sp ) )
				continue;
			if ( sp->page() != Spell::tr( "Sanitize" ) && !sp->sanity() )
				continue;
			// A spell that wants a selected block cannot run from here at all.
			if ( sp->name() == QLatin1String( "Sort Keys" ) )
				continue;
			fixes.append( sp );
		}
		std::sort( fixes.begin(), fixes.end(),
			[]( const SpellPtr & a, const SpellPtr & b ) { return a->name() < b->name(); } );
		for ( SpellPtr sp : fixes ) {
			auto * item = new QTreeWidgetItem( repairs );
			const bool ok = sp->isApplicable( m, QModelIndex() );
			item->setText( 0, sp->name() );
			item->setData( 0, SpellRole, sp->name() );
			item->setCheckState( 0, ( ok && sp->sanity() ) ? Qt::Checked : Qt::Unchecked );
			item->setDisabled( !ok );
			if ( !ok )
				item->setForeground( 0, QColor( wwSkinColor( "textMuted" ) ) );
		}
	};

	QObject::connect( rescan, &QPushButton::clicked, panel, [=]() { scan(); rebuildRepairs(); } );

	QObject::connect( runRepairs, &QPushButton::clicked, panel, [=]() {
		QStringList picked;
		for ( int i = 0; i < repairs->topLevelItemCount(); i++ ) {
			QTreeWidgetItem * it = repairs->topLevelItem( i );
			if ( !it->isDisabled() && it->checkState( 0 ) == Qt::Checked )
				picked << it->data( 0, SpellRole ).toString();
		}
		for ( const QString & f : std::as_const( picked ) )
			runFix( f );
		rebuildRepairs();
	} );

	QObject::connect( copyReport, &QToolButton::clicked, panel, [=]() {
		QStringList lines;
		for ( int i = 0; i < tree->topLevelItemCount(); i++ ) {
			QTreeWidgetItem * g = tree->topLevelItem( i );
			lines << g->text( 0 );
			for ( int c = 0; c < g->childCount(); c++ )
				lines << QStringLiteral( "    " ) + g->child( c )->text( 0 );
		}
		if ( lines.isEmpty() )
			lines << QObject::tr( "No issues found." );
		QApplication::clipboard()->setText( lines.join( QStringLiteral( "\n" ) ) );
		status->setText( QObject::tr( "Report copied." ) );
	} );

	// Column 1 is the action column: clicking it acts, rather than needing a real
	// button per row, which on a list this long would cost a widget per finding.
	QObject::connect( tree, &QTreeWidget::itemClicked, panel,
		[=]( QTreeWidgetItem * item, int column ) {
		if ( column != 1 || item->text( 1 ).isEmpty() )
			return;
		if ( item->parent() == nullptr )
			runFix( item->data( 0, FixRole ).toString() );
		else if ( skope && item->data( 0, BlockRole ).toInt() >= 0 )
			skope->select( model() ? model()->getBlockIndex( item->data( 0, BlockRole ).toInt() )
			                       : QModelIndex() );
	} );

	// ...and double-clicking a finding anywhere goes to its block, which is the
	// convention the Animation lint already set.
	QObject::connect( tree, &QTreeWidget::itemDoubleClicked, panel,
		[=]( QTreeWidgetItem * item, int ) {
		const int b = item->data( 0, BlockRole ).toInt();
		if ( skope && b >= 0 && model() )
			skope->select( model()->getBlockIndex( b ) );
	} );

	/* Re-scan when the file changes — but only while this dock is visible.
	 *
	 * A scan casts every check over every block, so running it from behind a
	 * closed dock would put that on every edit in the app. The Skeleton Manager
	 * shipped without this guard and did exactly that.
	 */
	if ( skope ) {
		QObject::connect( skope, &NifSkope::completeLoading, panel,
			[=]( bool, QString & ) { if ( panel->isVisible() ) { scan(); rebuildRepairs(); } } );
	}
	QObject::connect( dock, &QDockWidget::visibilityChanged, panel, [=]( bool visible ) {
		if ( visible ) {
			scan();
			rebuildRepairs();
		}
	} );

	dock->setWidget( panel );
	return dock;
}
