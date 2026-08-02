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
 *  WHAT EACH ROW CAN OFFER
 *
 *  One finding, one primary action, in the second column:
 *
 *    "Fix this one"   the finding names a block and an array, so that array's
 *                     own index can be rebuilt and a spell cast on it alone.
 *                     Exactly one class of problem qualifies today.
 *    "Go to"          otherwise. Double-click does this on any row, including
 *                     the ones whose column is taken by a fix.
 *
 *  A group carries the whole-file repair for its class, labelled with the name
 *  of the spell that will run — never "Fix all", which hides the fact that the
 *  scope is the entire file. Its tooltip states the overreach.
 *
 *  Groups whose problem nothing repairs say so, in a muted child row. That
 *  emptiness is content: it distinguishes "not wired up yet" from "the spell
 *  library has no answer to this", and three of the six classes are the latter.
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
	FixOnBlockRole,                 //!< IssueClass::perRowOnBlock, carried to the click handler
};

/*! One row of the issue catalogue: a KIND of problem, not a spell.
 *
 *  Grouping by the spell that reported a finding was the first design and it is
 *  wrong, in a way that produced a real error in this file's previous version.
 *  One spell emits several unrelated problems with different answers.
 *  `spErrorNoneRefs` is the proof: it reports
 *
 *     "'Properties' link array contains 2 None Refs."     <- a hole in an array
 *     "'Skeleton Root' link is None."                     <- a missing single Ref
 *
 *  The first is repaired exactly by Collapse Link Arrays, which runs
 *  `numCollapser` over Properties and Extra Data List — the same two arrays the
 *  checker inspects. The second is a lone Ref that no collapse can touch, and
 *  nothing in the spell library repairs. Filed under one heading, either the
 *  fixable half loses its fix or the unfixable half gains a button that does
 *  nothing to it. This table splits them.
 *
 *  `perRow` is the honest per-finding fix bungo asked for, and it exists for
 *  exactly one class: the message carries both the block and the array name, so
 *  the array's own index can be rebuilt and `spCollapseArray` cast on that one
 *  array. Its isApplicable re-validates the index, so a wrong reconstruction
 *  fails closed rather than collapsing the wrong thing.
 */
struct IssueClass
{
	const char * pattern;      //!< matched against the finding text
	const char * title;        //!< the group heading
	const char * wholeFile;    //!< spell name that repairs the whole file, or ""
	const char * perRow;       //!< spell name that repairs this one occurrence, or ""
	/*! What `perRow` is cast ON.
	 *
	 *  Two different shapes, and getting this wrong is silent. The link-array
	 *  case needs the ARRAY inside the block — `spCollapseArray` operates on an
	 *  array index and its isApplicable rejects a block — so the target is
	 *  rebuilt from the array name the message quotes. The material cases need
	 *  the BLOCK itself, because `tlShaderBlock` resolves a shader from a block
	 *  index and there is no array in the message to name.
	 *
	 *  This field exists because I first filed both material fixes as `wholeFile`
	 *  and neither would have run: `spFillShaderTextureSet` and
	 *  `spSyncShaderMaterial` both gate isApplicable on `tlShaderBlock(...)
	 *  .isValid()`, which is false for the invalid index a whole-file cast uses.
	 *  The group button would have reported "does not apply to this file" forever
	 *  — the same failure as the Repairs button that shipped doing nothing.
	 */
	bool perRowOnBlock;        //!< true: cast on the block; false: on the named array
	const char * note;         //!< the caveat, or what to do when there is no fix
};

const IssueClass issueClasses[] = {
	{ "link array contains .* None Refs",
	  "Holes in a link array",
	  "Collapse Link Arrays", "Collapse", false,
	  "Collapse Link Arrays also collapses Children, Modifiers and Sub Shapes arrays, which this check never inspects." },

	{ "link is None",
	  "Missing required reference",
	  "", "", false,
	  "No automatic fix — the block it wants has to be supplied, or the field pointed somewhere valid." },

	{ "has a filepath without a file extension",
	  "Texture path has no extension",
	  "", "", false,
	  "No automatic fix — correct it by hand, or with Search/Replace Resource Paths." },

	{ "has an absolute filepath",
	  "Texture path is absolute",
	  "", "", false,
	  "No automatic fix — an absolute path will not resolve on another machine; make it relative to the Data folder." },

	{ "cannot have empty filepaths",
	  "Texture path is empty",
	  "", "", false,
	  "No automatic fix — supply the path or clear the slot properly." },

	{ "Flags lack",
	  "Shader flags disagree with the environment map",
	  "", "", false,
	  "No automatic fix — reconcile the flags with the map that is actually assigned." },

	/* From spCheckAllMaterials (meshtools.cpp). Ordered before the looser
	 * patterns above would ever reach them; "Texture is missing" has to precede
	 * any rule that matches "missing" alone, or every material finding lands in
	 * one heap.
	 */
	{ "Material file is missing",
	  "Material file not found",
	  "", "", false,
	  "No automatic fix — the .bgsm/.bgem is not in the loaded archives or the Data folder. Check the path, or that the right game is selected." },

	{ "Texture is missing",
	  "Texture named by the material not found",
	  "", "", false,
	  "No automatic fix — the material asks for a texture that is not in the archives. It renders as the missing-texture placeholder in game." },

	{ "neither a material nor a texture set",
	  "Shader has no way to be textured",
	  "", "", false,
	  "No automatic fix — assign a material in the Material Manager, or give the shader a BSShaderTextureSet." },

	{ "BSShaderTextureSet is missing",
	  "Texture set missing from a lighting shader",
	  "", "", false,
	  "No automatic fix — the material decoded but the shader has no BSShaderTextureSet to compare it against." },

	{ "BSShaderTextureSet differs",
	  "Texture set disagrees with the material",
	  "", "Fill BSShaderTextureSet from BGSM", true,
	  "Normal on vanilla assets. Fixing one rewrites that shader's whole texture set from its material file." },

	{ "Shader flags are out of sync",
	  "Shader flags disagree with the material file",
	  "", "", false,
	  "Normal on vanilla assets — the game applies the material at runtime. Sync Shader Property from BGSM/BGEM will reconcile it, but it asks Safe or Full, so it is not a one-click fix." },

	{ "Material type does not match",
	  "Material is the wrong kind for the shader",
	  "", "", false,
	  "No automatic fix — a lighting shader wants a .bgsm and an effect shader a .bgem; one of the two is wrong." },

	{ "Material file could not be decoded",
	  "Material file is unreadable",
	  "", "", false,
	  "No automatic fix — the file was found but did not parse. It is truncated, or a version this build does not read." },

	/* From spCheckCollision (collisiontools.cpp). The Collision Manager's own
	 * Check button offers an "Apply Safe Fixes" pass for the first two of these,
	 * but it is a member of that dock and ends in a modal, so nothing here can
	 * reach it — hence no fix names below. Hoisting it is its own piece of work.
	 */
	{ "reference is missing",
	  "Collision points at a block that is not there",
	  "", "", false,
	  "No automatic fix here — the Collision Manager's Check button can remove the broken object." },

	{ "Collision layer is Unidentified",
	  "Collision layer not set",
	  "", "", false,
	  "No automatic fix here — the Collision Manager's Check button can infer Static or Props from the motion system." },

	{ "Convex hull has .* vertices",
	  "Convex hull is over budget",
	  "", "", false,
	  "No automatic fix — rebuild it with fewer vertices, or use Create Convex Shapes with a lower ratio." },

	{ "Convex hull is box-shaped",
	  "Convex hull could be a box",
	  "", "", false,
	  "Not a fault — a Box collision would be cheaper for the same shape." },

	{ "non-uniform scale",
	  "Primitive collision is non-uniformly scaled",
	  "", "", false,
	  "No automatic fix — Havok ignores non-uniform scale on box, sphere and capsule shapes, so the collision is not the size it looks." },

	{ "STAIRHELPER",
	  "Stair helper has no slope",
	  "", "", false,
	  "No automatic fix — layer 31 tells the engine to smooth movement over a slope, and this body has only primitives." },

	{ "Visible geometry has no collision",
	  "Visible geometry without collision",
	  "", "", false,
	  "Often correct — effect meshes, editor markers and decoration are meant to be walked through." },

	{ "mixes compiled and editable collision",
	  "Compiled and editable collision in one file",
	  "", "", false,
	  "No automatic fix — finish editing, then compile consistently, or the game and the editor will disagree about what is solid." },
};

/*! Run order for the Repairs section, which is NOT its display order.
 *
 *  The rows are listed alphabetically because that is easy to scan, and that
 *  used to drive execution too — a coin flip that stays harmless only while
 *  nothing depends on anything. It does: compacting the string table has to come
 *  after the passes that ADD strings, and derived data (bounds, tangents, skin
 *  partitions) has to come after the structure it is derived from settles.
 *
 *  Alphabetically, "Adjust Texture Sources" runs FIRST. It belongs after the
 *  structural passes, and that is the misordering a default run actually hits —
 *  the two spells whose ordering looks most alarming here, Reorder Blocks and
 *  Enforce Node Name Authority, are both unticked by default because neither
 *  claims sanity(), so reaching them takes a deliberate click.
 *
 *  Anything not listed runs between the structural group and the derived group,
 *  in whatever order it was gathered.
 */
const char * const repairOrder[] = {
	"Collapse Link Arrays", "Reorder Link Arrays", "Reorder Blocks",
	"Fix Invalid Block Names", "Enforce Node Name Authority",
	"Zero Geometry Group ID", "Fill Blank NiControllerSequence Types",
	"Adjust Texture Sources",
	"Update All Bounds", "Update All Tangent Spaces", "Make All Skin Partitions",
	"Remove Unused Strings",
};

/*! Whole-file repairs that live on other pages and the Sanitize/sanity() rule
 *  would never gather.
 *
 *  Each is a genuine "my file is broken" answer, and each rewrites more than it
 *  repairs, so none is ticked by default. Deliberately NOT here: the batch
 *  optimisers and normal generators, which overwrite authoring intent on meshes
 *  that were fine, and anything that writes other files to disk.
 */
const char * const repairExtras[] = {
	"Update All Bounds", "Update All Tangent Spaces",
	"Remove Unused Strings", "Make All Skin Partitions",
};

//! Position in repairOrder, or a middle value for anything unlisted.
int orderOf( const QString & name )
{
	for ( int i = 0; i < int( std::size( repairOrder ) ); i++ )
		if ( name == QLatin1String( repairOrder[i] ) )
			return i;
	return int( std::size( repairOrder ) ) / 2;
}

/*! Why a repair is greyed out, in its own words.
 *
 *  "Nothing for this to do in this file" was the old text and it was not true:
 *  these are FORMAT gates, not content probes, so the honest answer is which
 *  format this file is not. Read straight off each isApplicable.
 */
QString whyNotApplicable( const NifModel * nif, const QString & name )
{
	if ( name == QLatin1String( "Reorder Link Arrays" )
		|| name == QLatin1String( "Collapse Link Arrays" ) )
		return QObject::tr( "needs NIF 20.0.0.4 or newer with a Bethesda version" );
	if ( name == QLatin1String( "Zero Geometry Group ID" ) )
		return QObject::tr( "Fallout 3 / New Vegas only" );
	if ( name == QLatin1String( "Adjust Texture Sources" ) )
		return QObject::tr( "only for games below BSVersion 130" );
	if ( name == QLatin1String( "Enforce Node Name Authority" ) )
		return QObject::tr( "this file has no object palette" );
	if ( nif && nif->getBSVersion() == 0 )
		return QObject::tr( "not a Bethesda NIF" );
	return QObject::tr( "does not apply to this file's format" );
}

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
	 * These are whole-file operations. MOST answer no finding in the Issues list,
	 * which is why they are not wired to its rows — that would suggest a
	 * connection that is not there. The exception is Collapse Link Arrays, which
	 * genuinely is the answer to one issue class and is therefore ALSO offered on
	 * that group; it appears in both places because it is both things.
	 *
	 * (The comment here used to claim none of them answered anything, and the
	 * label below said so to the user. That stopped being true the moment the
	 * catalogue gained its first pairing.)
	 */
	layout->addWidget( heading( QObject::tr( "Repairs" ), panel ) );
	/* This used to read "None of these fixes an issue above", which was true when
	 * the panel had no pairings at all and became false the moment it gained one:
	 * Collapse Link Arrays is in this list AND is the whole-file fix offered on
	 * the "Holes in a link array" group. The label was contradicting the panel it
	 * sits in.
	 */
	auto * repairHint = new QLabel( QObject::tr(
		"Whole-file operations. Most do not answer any issue above — the ones that do "
		"are offered on the issue itself." ), panel );
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

	/*! The spell called `name`, whatever page it lives on.
	 *
	 *  NOT SpellBook::lookup. That takes a "Page/Name" id and, given a bare name,
	 *  matches only spells whose page() is EMPTY — so `lookup("Collapse")` returns
	 *  null because its page is "Array", and every paged spell this panel wanted
	 *  came back null too. The Repairs button silently did nothing for exactly
	 *  that reason, and reported "does not apply to this file" while doing it.
	 *  Caught by the harness asserting that a fix RESOLVES its finding rather
	 *  than merely that a button existed.
	 *
	 *  Names are unique enough here, and a miss is handled by the caller.
	 */
	auto spellNamed = []( const QString & name ) -> SpellPtr {
		if ( name.isEmpty() )
			return nullptr;
		for ( SpellPtr s : SpellBook::spells() )
			if ( s && s->name() == name )
				return s;
		return nullptr;
	};

	auto model = [skope, nif]() -> NifModel * {
		NifModel * m = skope ? skope->getNifModel() : nif;
		return ( m && m->getBlockCount() > 0 ) ? m : nullptr;
	};

	//! Which catalogue entry a finding belongs to; -1 when nothing matches.
	auto classify = []( const QString & text ) {
		for ( int i = 0; i < int( std::size( issueClasses ) ); i++ ) {
			const QRegularExpression re( QString::fromUtf8( issueClasses[i].pattern ) );
			if ( re.match( text ).hasMatch() )
				return i;
		}
		return -1;
	};

	//! The array name a finding quotes, as in "'Properties' link array ...".
	auto arrayOf = []( const QString & text ) {
		static const QRegularExpression re( QStringLiteral( "'([^']+)'" ) );
		const auto m = re.match( text );
		return m.hasMatch() ? m.captured( 1 ) : QString();
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

		/* Findings are collected from every check FIRST, then grouped by class.
		 * The two cannot be interleaved: one spell contributes to several groups
		 * and several spells can contribute to one, which is the whole reason
		 * this is keyed on the problem rather than on who reported it.
		 */
		struct Finding
		{
			QString text;
			QtMsgType type;
			int block;
			QString spell;
			int cls;
		};
		QList<Finding> findings;

		const BaseModel::MsgMode was = m->getMessageMode();
		m->setMessageMode( BaseModel::MSG_TEST );
		for ( SpellPtr s : checkSpells() ) {
			if ( !s->isApplicable( m, QModelIndex() ) )
				continue;
			m->getMessages();						// drain anything pending
			s->cast( m, QModelIndex() );
			for ( const TestMessage & msg : m->getMessages() ) {
				const QString text = QString( msg );
				findings.append( Finding{ text, msg.type(), blockOf( text ), s->name(),
					classify( text ) } );
			}
		}
		m->setMessageMode( was );

		// worst-first, so the thing most likely to break the file is at the top
		auto rank = []( QtMsgType t ) {
			return ( t == QtCriticalMsg || t == QtFatalMsg ) ? 0 : ( t == QtWarningMsg ? 1 : 2 );
		};

		QList<int> order;								// class ids, first-seen order
		QHash<int, QList<int>> byClass;					// class id -> finding indices
		for ( int i = 0; i < findings.size(); i++ ) {
			const int c = findings.at( i ).cls;
			if ( !byClass.contains( c ) )
				order.append( c );
			byClass[c].append( i );
		}
		std::sort( order.begin(), order.end(), [&]( int a, int b ) {
			auto worstOf = [&]( int c ) {
				int w = 2;
				for ( int i : byClass.value( c ) )
					w = std::min( w, rank( findings.at( i ).type ) );
				return w;
			};
			return worstOf( a ) < worstOf( b );
		} );

		for ( int c : std::as_const( order ) ) {
			const QList<int> & idx = byClass.value( c );
			const bool known = ( c >= 0 );

			auto * group = new QTreeWidgetItem( tree );
			/* An unmatched finding still gets a home, keyed on the spell that
			 * reported it. A checker added later therefore shows up without
			 * anyone having to remember to extend the catalogue — it just shows
			 * up ungrouped and without a fix, which is the safe default.
			 */
			const QString title = known
				? QString::fromUtf8( issueClasses[c].title )
				: QObject::tr( "%1 (uncatalogued)" ).arg( findings.at( idx.first() ).spell );
			group->setText( 0, QObject::tr( "%1 — %n", nullptr, idx.size() ).arg( title ) );
			QFont gf = group->font( 0 );
			gf.setBold( true );
			group->setFont( 0, gf );

			QtMsgType worst = QtInfoMsg;
			for ( int i : idx ) {
				const Finding & f = findings.at( i );
				auto * item = new QTreeWidgetItem( group );
				item->setText( 0, f.text );
				item->setData( 0, BlockRole, f.block );
				item->setData( 0, SpellRole, f.spell );

				QColor col = plain;
				if ( f.type == QtCriticalMsg || f.type == QtFatalMsg ) {
					col = danger; errors++;
					worst = QtCriticalMsg;
				} else if ( f.type == QtWarningMsg ) {
					col = warn; warnings++;
					if ( worst != QtCriticalMsg ) worst = QtWarningMsg;
				} else {
					notes++;
				}
				// All columns, or the action cell stays default-coloured on a red row.
				item->setForeground( 0, col );
				item->setForeground( 1, col );

				/* The per-row fix, where the catalogue says one exists. Every
				 * finding names its block as "[19]"; whether that is enough
				 * depends on what the spell wants to be cast on. An array-scoped
				 * fix also needs the array name out of "'Properties'", and offering
				 * the button without it would build an invalid index and fail on
				 * click rather than being greyed out.
				 */
				const QString perRow = known ? QString::fromUtf8( issueClasses[c].perRow ) : QString();
				// `known` first: c is -1 for an uncatalogued group and indexing
				// issueClasses with it reads off the front of the array
				const bool onBlock = known && issueClasses[c].perRowOnBlock;
				const bool haveTarget = f.block >= 0
					&& ( onBlock || !arrayOf( f.text ).isEmpty() );
				if ( !perRow.isEmpty() && haveTarget ) {
					item->setData( 0, FixRole, perRow );
					item->setData( 0, FixOnBlockRole, onBlock );
					item->setText( 1, QObject::tr( "Fix this one" ) );
				} else if ( f.block >= 0 ) {
					item->setText( 1, QObject::tr( "Go to" ) );
				}
			}
			group->setForeground( 0, worst == QtCriticalMsg ? danger
				: worst == QtWarningMsg ? warn : plain );

			const QString wholeFile = known ? QString::fromUtf8( issueClasses[c].wholeFile ) : QString();
			group->setData( 0, FixRole, wholeFile );
			if ( !wholeFile.isEmpty() ) {
				// Named for the spell that runs, never "Fix all" — the label has to
				// say what it will do to the rest of the file.
				group->setText( 1, wholeFile );
				group->setToolTip( 1, QString::fromUtf8( issueClasses[c].note ) );
			}
			if ( known && *issueClasses[c].note ) {
				auto * note = new QTreeWidgetItem( group );
				note->setText( 0, QString::fromUtf8( issueClasses[c].note ) );
				note->setForeground( 0, muted );
				note->setData( 0, BlockRole, -1 );
			}
			group->setExpanded( true );
		}

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
	/*! Cast a spell on ONE index, in a single undo step, then re-scan.
	 *
	 *  The per-finding fix bungo asked for, and the only one the spell library
	 *  can honestly support: the finding names its block and its array, so the
	 *  array's own index is reconstructible and `spCollapseArray` can be cast on
	 *  exactly that array. isApplicable re-validates it, so a wrong
	 *  reconstruction refuses instead of collapsing something else.
	 */
	auto runFixAt = [=]( const QString & fixName, const QModelIndex & target ) {
		NifModel * m = model();
		SpellPtr fix = spellNamed( fixName );
		if ( !m || !fix || !target.isValid() )
			return false;
		if ( !fix->isApplicable( m, target ) ) {
			status->setText( QObject::tr(
				"%1 will not run here — the field this finding names is not the array it expected." )
				.arg( fixName ) );
			return false;
		}
		QByteArray before;
		{
			QBuffer buf( &before );
			buf.open( QIODevice::WriteOnly );
			if ( !m->save( buf ) )
				return false;
		}
		QUndoStack * stack = m->undoStack;
		m->undoStack = nullptr;
		fix->cast( m, target );
		/* Same refresh runFix does, for the same reason.
		 *
		 * Not load-bearing today -- NifModel::save calls updateHeader/updateFooter
		 * itself before serialising, so the snapshot below is correct either way,
		 * and spCollapseArray touches nothing that header CONDITIONS depend on.
		 * It is here so the two repair paths cannot drift: the moment a per-row
		 * fix is wired for a spell that changes a version or user-version field,
		 * the cached header conditions would be stale and only this line would
		 * have caught it.
		 */
		m->invalidateHeaderConditions();
		m->updateHeader();
		m->updateFooter();
		m->undoStack = stack;

		QByteArray after;
		{
			QBuffer buf( &after );
			buf.open( QIODevice::WriteOnly );
			if ( !m->save( buf ) )
				after = before;
		}
		const bool changed = ( before != after );
		if ( changed && m->undoStack )
			m->undoStack->push( new NifSnapshotCommand( m, before, after, fixName ) );
		/* Re-scan immediately, and not only for the feedback: collapsing an array
		 * shifts every later row of it, so any index cached from the previous scan
		 * now points at the wrong thing.
		 */
		scan();
		status->setText( changed
			? QObject::tr( "%1 ran. Ctrl+Z undoes it." ).arg( fixName )
			: QObject::tr( "%1 changed nothing." ).arg( fixName ) );
		return changed;
	};

	auto runFix = [=]( const QString & fixName ) {
		NifModel * m = model();
		if ( !m || fixName.isEmpty() )
			return;
		SpellPtr fix = spellNamed( fixName );
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
			/* The Sanitize page, PLUS four named repairs that live on other pages.
			 *
			 * Update All Bounds, Update All Tangent Spaces, Remove Unused Strings
			 * and Make All Skin Partitions are on the Batch and Optimize pages and
			 * none of them claims sanity(), so the rule above cannot see them —
			 * yet each is a genuine answer to "my file is broken" and Update All
			 * Bounds in particular fixes meshes that vanish as the camera moves.
			 * They were reachable from the old dialog and nowhere else.
			 */
			bool wanted = ( sp->page() == Spell::tr( "Sanitize" ) || sp->sanity() );
			for ( const char * extra : repairExtras )
				if ( sp->name() == QLatin1String( extra ) )
					wanted = true;
			if ( !wanted )
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
			item->setText( 0, ok ? sp->name()
				: QObject::tr( "%1 — %2" ).arg( sp->name(), whyNotApplicable( m, sp->name() ) ) );
			item->setData( 0, SpellRole, sp->name() );
			/* Armed from the spell's OWN sanity() flag, never from the group it
			 * landed in. Upstream uses that flag to keep dangerous spells out of
			 * automatic runs and says so in the source — Reorder Blocks renumbers
			 * every block and carries the comment "can really only cause issues
			 * with rendering and textureset overrides via the CK" — so ticking by
			 * group would override a judgement this panel did not make. The four
			 * extras have no sanity() either, and so arrive unticked, which is
			 * correct: each rewrites more than it repairs.
			 */
			item->setCheckState( 0, ( ok && sp->sanity() ) ? Qt::Checked : Qt::Unchecked );
			item->setDisabled( !ok );
			item->setToolTip( 0, sp->hint() );
			if ( !ok )
				item->setForeground( 0, QColor( wwSkinColor( "textMuted" ) ) );
		}
	};

	QObject::connect( rescan, &QPushButton::clicked, panel, [=]() { scan(); rebuildRepairs(); } );

	/*! Run several repairs as ONE undoable step, in the curated order.
	 *
	 *  Two things the previous version got wrong, both inherited from casting
	 *  runFix in a loop.
	 *
	 *  UNDO. Each runFix pushed its own snapshot, so a five-repair run left five
	 *  commands and Ctrl+Z walked back through half-repaired intermediate states
	 *  one at a time. A user who ticks five boxes and presses one button has done
	 *  one thing and expects one undo.
	 *
	 *  ORDER. The rows are displayed alphabetically, which put "Adjust Texture
	 *  Sources" first and "Remove Unused Strings" in the middle — and compacting
	 *  the string table has to come after everything that adds strings. See
	 *  repairOrder.
	 *
	 *  WHY NOT nifSnapshotOp. Two reasons, both of which only appear once several
	 *  spells run together. It nests: spEnforceNameAuthority::cast calls
	 *  nifSnapshotOp itself, so wrapping the batch in another one pushes two
	 *  commands for one run and the second Ctrl+Z walks the file FORWARD into a
	 *  half-repaired state. Detaching the undo stack for the duration makes every
	 *  spell's own snapshot a no-op — nifSnapshotOp only pushes
	 *  `if ( nif->undoStack )` — leaving exactly one command to push at the end.
	 *  And it pushes unconditionally: a run that changes nothing would still mark
	 *  the document modified and ask to save on close.
	 *
	 *  Header and footer are updated INSIDE the measured region. They are derived
	 *  bookkeeping, a wrong root list is a real breakage, and refreshing them
	 *  after the "after" snapshot would leave the undo command describing a file
	 *  that no longer matches the model.
	 */
	auto runFixes = [=]( QStringList names ) {
		NifModel * m = model();
		if ( !m || names.isEmpty() )
			return;
		std::sort( names.begin(), names.end(),
			[]( const QString & a, const QString & b ) { return orderOf( a ) < orderOf( b ); } );

		QByteArray before;
		{
			QBuffer buf( &before );
			buf.open( QIODevice::WriteOnly );
			if ( !m->save( buf ) ) {
				status->setText( QObject::tr( "Could not snapshot the file; nothing was run." ) );
				return;
			}
		}

		QUndoStack * stack = m->undoStack;
		m->undoStack = nullptr;					// no nested commands inside the run
		const BaseModel::MsgMode was = m->getMessageMode();
		m->setMessageMode( BaseModel::MSG_TEST );
		m->getMessages();						// drain

		QStringList ran;
		for ( const QString & n : std::as_const( names ) ) {
			SpellPtr sp = spellNamed( n );
			if ( !sp || !sp->isApplicable( m, QModelIndex() ) )
				continue;
			sp->cast( m, QModelIndex() );
			ran << n;
		}

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
				after = before;					// unreadable: assume unchanged
		}
		const bool changed = ( before != after );
		if ( changed && m->undoStack )
			m->undoStack->push( new NifSnapshotCommand( m, before, after,
				QObject::tr( "Unfuck" ) ) );

		scan();
		/* What happened, not what was attempted. "Ran 6 repairs" does not answer
		 * "is my file different now", and the byte comparison is the only honest
		 * answer available without a per-spell probe.
		 */
		status->setText( changed
			? QObject::tr( "Ran %n repair(s); the file changed. Ctrl+Z undoes the whole run. %1",
				nullptr, ran.size() ).arg( status->text() )
			: QObject::tr( "Ran %n repair(s); nothing changed.", nullptr, ran.size() ) );
	};

	QObject::connect( runRepairs, &QPushButton::clicked, panel, [=]() {
		QStringList picked;
		for ( int i = 0; i < repairs->topLevelItemCount(); i++ ) {
			QTreeWidgetItem * it = repairs->topLevelItem( i );
			if ( !it->isDisabled() && it->checkState( 0 ) == Qt::Checked )
				picked << it->data( 0, SpellRole ).toString();
		}
		runFixes( picked );
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
		const QString fix = item->data( 0, FixRole ).toString();
		if ( item->parent() == nullptr ) {
			runFix( fix );					// group: the whole-file repair
		} else if ( !fix.isEmpty() ) {
			/* Row: rebuild the spell's own target from the message text.
			 *
			 * Which index that is depends on the spell, not on the panel --
			 * see IssueClass::perRowOnBlock. Casting a block-scoped spell on an
			 * array index (or the reverse) does not crash, it just fails
			 * isApplicable and reports "will not run here", which reads as a
			 * broken button rather than a wrong target.
			 */
			NifModel * m = model();
			const int b = item->data( 0, BlockRole ).toInt();
			if ( m && b >= 0 ) {
				const QModelIndex block = m->getBlockIndex( b );
				if ( item->data( 0, FixOnBlockRole ).toBool() ) {
					runFixAt( fix, block );
				} else {
					const QString arr = arrayOf( item->text( 0 ) );
					if ( !arr.isEmpty() )
						runFixAt( fix, m->getIndex( block, arr ) );
				}
			}
		} else if ( skope && item->data( 0, BlockRole ).toInt() >= 0 ) {
			skope->select( model() ? model()->getBlockIndex( item->data( 0, BlockRole ).toInt() )
			                       : QModelIndex() );
		}
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
